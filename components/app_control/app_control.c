/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2022-06-17 18:19:35
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-07-27 10:54:43
 * @FilePath: /smart-air-bed-board-program/components/app_control/app_control.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "app_control.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "use_wifi.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include <string.h>
#include "use_wifi.h"
#include "esp_wifi.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "sntp.h"
#include <time.h>
#include "use_uart.h"
#include "driver/uart.h"
#include "pb_decode.h"
#include "keesoncloud.pb.h"
#include "qs_protobuf.h"
#include "mqtt_client.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#define UP_RATIO_60S    6   // 60s上报一次需改为12

static const char *TAG = "control";
extern device_info_t *device_info;
extern esp_mqtt_client_handle_t client;
extern char user_5s_data_publish_topic[64];
extern char user_60s_data_publish_topic[64];
extern char user_sleep_data_publish_topic[64];
// extern char user_cli_data_subscribe_topic[64];

extern qs_pb_msg_sensor_1min_info *user_60s_sensor_info;
extern qs_pb_msg_sensor_5sec_info *user_5s_sensor_info;
time_t now;
struct tm ti;
bool pause_uart_task = true;
TaskHandle_t uart_handle = NULL;
bool get_5s_flag = false;
bool mqtt_send_mutex = true;

uint8_t *uart_recbuff;
uint8_t *sing_cmd_value;
uint8_t *mux_sing_cmd_value;

char json_report_name[32] = {0};

static char device_id[12] = {0},device_version[32] = {0},set_rtc_flag = 0,devic_id_flag = 0;   //bc重启后将两个标志位置零，重新设置addr与rtc 
static char sleep_up_flag = 0,ota_now_flag = 0,sensor_upgrade_flag = 0;//sleep_up睡眠报告上传进行中、esp32固件升级、博创传感器升级标志
static uint16_t  sensor_ota_mode_cnt = 0;
static uint8_t set_mode_flag = 0;   // xinzeng:set_mode_flag 强制生成报告
static char report_cli_data[2]={0},cli_report_name[32]={0};
void set_cli_report_name(char* data,char len)
{
    memset(cli_report_name, 0, 32);
    memcpy(cli_report_name,data,len);
}
static void report_cli_up(void)   //指令下发
{
    // return_value[1024] = {0};
    // char get_report_cmd1[]="list";
    char get_report_cmd2[12]={0};
    switch (report_cli_data[0])
    {
    // case 1:
    //     printf("get_report_cmd1");
    //     set_bc(device_info->utc.time_stamp, get_report_cmd1, 1, 1, return_value, 50);  //获取list列表
    //     printf("ble_test list %s",return_value);
    //     report_cli_data[0] = 0;
    //     break;
    case 2:   //睡眠报告上传指令
        sprintf(get_report_cmd2, "report %d",report_cli_data[1]);
        printf("get_report_cmd2 %s",get_report_cmd2);
        // sleep_up_flag = 1;
        memset(json_report_name, 0, 32);
        strcpy(json_report_name,cli_report_name);
        set_bc(device_info->utc.time_stamp, get_report_cmd2, 0, 0, NULL, 50);   //睡眠报告上传云端
        
        //vTaskDelay(60 * 1000 / portTICK_PERIOD_MS);
        sleep_up_flag = 0;
        report_cli_data[0] = 0;
        break;
  
    default:
        break;
    }
}
/*
    static void bochuang_test(uint8_t data1,uint8_t data2)
    {
        char return_value[1024] = {0};
        char get_report_cmd1[]="list";

        switch (data1)
        {
        case 1:
            printf("get_report_cmd1");
            set_bc(device_info->utc.time_stamp, get_report_cmd1, 1, 1, return_value, 50);  //获取list列表
            printf("ble_test list %s",return_value);
            data1 = 0;
            break;
        case 2:
            if(sleep_up_flag == 0)
            {
                sleep_up_flag = 1;
                report_cli_data[0]=data1;
                report_cli_data[1]=data2;
            }
            else
            {
                printf("sleep_uping now/n");
            }
            data1 = 0;
            break;
    
        default:
            break;
        }
    }
*/
//蓝牙数据解析
int ble_data_parser_cb(uint8_t *data)
{

#if BLE_TEST
if(data[0] == 0x11 && data[1]==0x22 && data[2]==0x33)
{
    //bochuang_test(data[3],data[4]);
}
else if (data[0] == 0x22 && data[1]==0x33 && data[2]==0x44)
{
    memset(device_info->wifi.one_key_config.ssid, 0, 32);
    memcpy(device_info->wifi.one_key_config.ssid, MY_WIFI_SSID, strlen(MY_WIFI_SSID));
    printf("ssid = %s \n",device_info->wifi.one_key_config.ssid);

    memset(device_info->wifi.one_key_config.passwd, 0, 64);
    memcpy(device_info->wifi.one_key_config.passwd, MY_WIFI_PASSWD, strlen(MY_WIFI_PASSWD));
    printf("passwd = %s \n",device_info->wifi.one_key_config.passwd);

    if(get_mqtt_status())
    {
        esp_mqtt_client_stop(client);
        set_mqtt_status(0);
    }
    if (get_wifi_status())
    {
        ESP_ERROR_CHECK(esp_wifi_disconnect());
    }
    set_wifi_status(0);

    set_one_key_config_wifi_status(1);
    ESP_ERROR_CHECK(esp_wifi_stop());
    memset(wifi_config.sta.ssid, 0, 32);
    memset(wifi_config.sta.password, 0, 64);
    memcpy(wifi_config.sta.ssid, (uint8_t *)device_info->wifi.one_key_config.ssid, strlen(device_info->wifi.one_key_config.ssid));
    memcpy(wifi_config.sta.password, (uint8_t *)device_info->wifi.one_key_config.passwd, strlen(device_info->wifi.one_key_config.passwd));
    printf("ssid = %s \n",wifi_config.sta.ssid);
    printf("passwd = %s \n",wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    s_retry_num = 0;
    ESP_ERROR_CHECK(esp_wifi_start());
}
else if (data[0] == 0x33 && data[1]==0x44 && data[2]==0x55)
{
    char temp[100] = {0};
   if(sleep_up_flag == 0 && ota_now_flag == 0)
                {
                    
                    char set_mode[] = "set mode 3";
                    char return_value[50] = {0};
                    printf("set mode 3\n");
                    set_bc(device_info->utc.time_stamp, set_mode, 1, 0, return_value, 100);
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                    if(strncmp(return_value, "ok",2) == 0)
                    {
                        sensor_upgrade_flag = 1;
                        pause_uart_task = false;
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        uart_flush(UART_NUM_1);
                        vTaskDelay(1000 / portTICK_PERIOD_MS);

                        sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":5,\"sdate\":\"Please start upgrading\"}",
                                                        device_info->id,
                                                        device_info->utc.time_stamp);
                        
                    }
                    else{
                        sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":5,\"sdate\":\"set upgrade mode fail\"}",
                                                        device_info->id,
                                                        device_info->utc.time_stamp);
                    } 
                }
                else
                {
                    sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":5,\"sdate\":\"now now ota or sleepUp,please wait and again\"}",
                                                        device_info->id,
                                                        device_info->utc.time_stamp);
                }
                
                printf("sensorUpgrade = %s \n",temp);
                esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                            device_info->ble->conn_id,
                                            device_info->ble->handle,
                                            strlen((char *)temp), (uint8_t *)temp, false);
            
}

#endif

    if(sensor_upgrade_flag == 1)
    {
        if(data[0 == 0xBB] && data[1] == 0xCC)
        {
            if(data[2] == 0x03)     //结束升级
            {
                uint8_t temp[50] = {0};

                if(data[3] == 0x01)
                {
                    sensor_upgrade_flag = 0;
                    pause_uart_task = true;
                    sensor_reboot_config();
                    sensor_ota_mode_cnt = 0;
                    printf("sensor up grade finish \n");

                    temp[0] = 0xBB;
                    temp[1] = 0xCC;
                    memcpy(&temp[2], device_id, 10);
                    temp[12] = 0x4D;
                    temp[13] = 0x03;
                    temp[14] = 0x01;
            
                    if(get_ble_status() && device_info->data_up_switch)
                    {
                       esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                            device_info->ble->conn_id,
                                            device_info->ble->handle,
                                            15, temp, false);
                   }

                }
                else if (data[3] == 0x02)
                {
                    sensor_upgrade_flag = 0;
                    pause_uart_task = true;
                    char set_mode[] = "set mode 2";
                    char return_value[50] = {0};
                    set_bc(device_info->utc.time_stamp, set_mode, 1, 0, return_value, 100);
                    printf("set mode 2 %s\n",return_value);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    sensor_ota_mode_cnt = 0;

                    temp[0] = 0xBB;
                    temp[1] = 0xCC;
                    memcpy(&temp[2], device_id, 10);
                    temp[12] = 0x4D;
                    temp[13] = 0x03;
                    if(strstr(return_value,"ok"))
                    {
                        temp[14] = 0x02;
                    }
                    else{
                        temp[14] = 0x03;
                    }
                    
            
                    if(get_ble_status() && device_info->data_up_switch)
                    {
                       esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                            device_info->ble->conn_id,
                                            device_info->ble->handle,
                                            15, temp, false);
                   }

                }
            
            
            }
            else{           //接收bc固件升级字节流
                sensor_ota_bc((char *)&data[2]);
            }
        }

        else
        {
            printf("sensor up grade DATA WRONG \n");
        }
        
    }
    else
    {
        cJSON *firstItem = NULL;
        cJSON *sencondItem = NULL;
        cJSON *thirdItem = NULL;
        firstItem = cJSON_Parse((char *)data);
    if (firstItem)
    {
        sencondItem = cJSON_GetObjectItem(firstItem, "id");
        if(!sencondItem) return 0;
        printf("id = %s\n", sencondItem->valuestring);
        if(strcmp(sencondItem->valuestring, device_info->id) != 0) return 0;
        sencondItem = cJSON_GetObjectItem(firstItem, "type");
        if(!sencondItem) return 0;
        if(sencondItem->valueint == 3)
        {
            sencondItem = cJSON_GetObjectItem(firstItem, "cmd");
            if(!sencondItem) return 0;

            thirdItem = cJSON_GetObjectItem(sencondItem, "ssid");
            if(!thirdItem) return 0;
            memset(device_info->wifi.one_key_config.ssid, 0, 32);
            memcpy(device_info->wifi.one_key_config.ssid, thirdItem->valuestring, strlen(thirdItem->valuestring));
            printf("ssid = %s \n",device_info->wifi.one_key_config.ssid);

            thirdItem = cJSON_GetObjectItem(sencondItem, "passwd");
            if(!thirdItem) return 0;
            memset(device_info->wifi.one_key_config.passwd, 0, 64);
            memcpy(device_info->wifi.one_key_config.passwd, thirdItem->valuestring, strlen(thirdItem->valuestring));
            printf("passwd = %s \n",device_info->wifi.one_key_config.passwd);
            if(get_mqtt_status())
            {
                esp_mqtt_client_stop(client);
                set_mqtt_status(0);
            }
            if (get_wifi_status())
            {
                ESP_ERROR_CHECK(esp_wifi_disconnect());
            }
            set_wifi_status(0);

            set_one_key_config_wifi_status(1);
            ESP_ERROR_CHECK(esp_wifi_stop());
            memset(wifi_config.sta.ssid, 0, 32);
            memset(wifi_config.sta.password, 0, 64);
            memcpy(wifi_config.sta.ssid, (uint8_t *)device_info->wifi.one_key_config.ssid, strlen(device_info->wifi.one_key_config.ssid));
            memcpy(wifi_config.sta.password, (uint8_t *)device_info->wifi.one_key_config.passwd, strlen(device_info->wifi.one_key_config.passwd));
            printf("ssid = %s \n",wifi_config.sta.ssid);
            printf("passwd = %s \n",wifi_config.sta.password);
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            s_retry_num = 0;
            ESP_ERROR_CHECK(esp_wifi_start());
        }
        else if(sencondItem->valueint == 4)
        {
            sencondItem = cJSON_GetObjectItem(firstItem, "cmd");
            if(!sencondItem) return 0;

            
            char temp[100] = {0}; 
            if(strcmp(sencondItem->valuestring, "sensorVersion") == 0) 
            {
                
                // if(strlen(device_version) < 2)
                // {
                //     sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":4,\"version\":\"version not get yet,pleas wait\"}",
                //                                         device_info->id,
                //                                         device_info->utc.time_stamp);
                // }
                // else
                // {
                //     sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":4,\"version\":\"%s\"}",
                //                                         device_info->id,
                //                                         device_info->utc.time_stamp,
                //                                         device_version);
                // }

                sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":4,\"version\":\"%s\"}",
                                                        device_info->id,
                                                        device_info->utc.time_stamp,
                                                        device_version);
                
                printf("sensorVersion = %s \n",temp);
                esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                            device_info->ble->conn_id,
                                            device_info->ble->handle,
                                            strlen((char *)temp), (uint8_t *)temp, false);
            }
        }
        else if(sencondItem->valueint == 5)
        {
            sencondItem = cJSON_GetObjectItem(firstItem, "cmd");
            if(!sencondItem) return 0;

            char temp[100] = {0};
            if(strcmp(sencondItem->valuestring, "sensorUpgrade") == 0)
            {
                // memset(temp,0,100);
                if(sleep_up_flag == 0 && ota_now_flag == 0)
                {
                    char set_mode[] = "set mode 3";
                    char return_value[50] = {0};
                    printf("set mode 3\n");
                    set_bc(device_info->utc.time_stamp, set_mode, 1, 0, return_value, 100);
                    vTaskDelay(100 / portTICK_PERIOD_MS);

                    if(strncmp(return_value, "ok",2) == 0)
                    {
                        sensor_upgrade_flag = 1;
                        pause_uart_task = false;
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        uart_flush(UART_NUM_1);
                        vTaskDelay(1000 / portTICK_PERIOD_MS);

                        sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":5,\"sdate\":\"Please start upgrading\"}",
                                                        device_info->id,
                                                        device_info->utc.time_stamp);
                        
                    }
                    else{
                        sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":5,\"sdate\":\"set upgrade mode fail\"}",
                                                        device_info->id,
                                                        device_info->utc.time_stamp);
                    } 
                }
                else
                {
                    sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":5,\"sdate\":\"now now ota or sleepUp,please wait and again\"}",
                                                        device_info->id,
                                                        device_info->utc.time_stamp);
                }
                
                printf("sensorUpgrade = %s \n",temp);
                esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                            device_info->ble->conn_id,
                                            device_info->ble->handle,
                                            strlen((char *)temp), (uint8_t *)temp, false);
            }
        }

        
    }
    cJSON_Delete(firstItem);
    }
    return 0;
}

void one_key_config_wifi_task(void *pv)
{
    uint8_t enevt_id;
    char temp[100] = {0};
    while (1)
    {
        if (xQueueReceive(device_info->wifi.one_key_config.xQueue, &enevt_id, portMAX_DELAY))
        {
            switch (enevt_id)
            {
            case wifi_ok:
                sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":3,\"state\":\"wifiOk\"}",
                                                                                    device_info->id,
                                                                                    device_info->utc.time_stamp);
                break;
            case wifi_fail:
                sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":3,\"state\":\"wifiFail\"}",
                                                                                    device_info->id,
                                                                                    device_info->utc.time_stamp);
                break;
            case mqtt_ok:
                sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":3,\"state\":\"mqttOk\"}",
                                                                                    device_info->id,
                                                                                    device_info->utc.time_stamp);                                                               
                break;
            case mqtt_fail:
                sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":3,\"state\":\"mqttFail\"}",
                                                                                    device_info->id,
                                                                                    device_info->utc.time_stamp);
                break;
            default:
                break;
            }

            printf("%s\n",temp);
            if(get_ble_status())
            {
                esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                                device_info->ble->conn_id,
                                                device_info->ble->handle,
                                                strlen(temp), (uint8_t *)temp, false);
            }
        }
    }
    vTaskDelete(NULL);
}

void ble_data_parser_task(void *pv)
{
    while (1)
    {
        if (xQueueReceive(device_info->ble->xQueue, device_info->ble->data_rec.value, portMAX_DELAY))
        {
            printf("ble:%s\n",device_info->ble->data_rec.value);
            ble_data_parser_cb(device_info->ble->data_rec.value);
        }
    }
    vTaskDelete(NULL);
}

/*
//mqtt配置解析
int user_mqtt_data_parser_cb(mmqtt_msg_t *msg)
{
    ESP_LOGI(TAG,"user get data success! from_topic=[%d][%s], msg=[%d][%s].", msg->topic_len, msg->topic, msg->data_len, msg->data);
    if(strstr(msg->topic, user_cli_data_subscribe_topic))
    {
        printf("1111111111111\n");
        cJSON *firstItem = cJSON_Parse((char *)msg->data);
        printf("%s\n",msg->data);
        if(firstItem)
        {
            printf("22222222222222\n");
            cJSON *secondItem = cJSON_GetObjectItem(firstItem, "id");
            if(strstr(secondItem->valuestring, device_info->id))
            {
                printf("33333333333333\n");
                secondItem = cJSON_GetObjectItem(firstItem, "cmd");
                if(strstr(secondItem->valuestring, "queryReport"))
                {
                    printf("444444444444\n");
                    // char get_report_cmd2[] = "report 0";
                    // set_bc(device_info->utc.time_stamp, get_report_cmd2, 0, NULL, NULL, NULL);
                }
            }
            cJSON_Delete(firstItem);
        }
    }
    return 0;
}

//mqtt接收topic
void mqtt_data_parser_task(void *pv)
{
    mmqtt_msg_t *recv_msg = (mmqtt_msg_t *)malloc(sizeof(mmqtt_msg_t));
        
    while (1)
    {   
        memset(recv_msg, 0, sizeof(mmqtt_msg_t));
        if (xQueueReceive(device_info->aliyun.xQueue, recv_msg, portMAX_DELAY))
        {
            // printf("device_info->aliyun.msg.data = %s\n",(char *)device_info->aliyun.msg.data);
            printf("topic len: %d  data len: %d\r\n", recv_msg->topic_len, recv_msg->data_len);
            user_mqtt_data_parser_cb(recv_msg);
        }
    }

    free(recv_msg);
    vTaskDelete(NULL);
}

*/

uint16_t crc16_compute(uint8_t const *p_data, uint32_t size)
{
	uint16_t crc = 0xFFFF;
	for (uint32_t i = 0; i < size; i++)
	{
		crc = (uint8_t)(crc >> 8) | (crc << 8);
		crc ^= p_data[i];
		crc ^= (uint8_t)(crc & 0xFF) >> 4;
		crc ^= (crc << 8) << 4;
		crc ^= ((crc & 0xFF) << 4) << 1;
	}
	return crc;
}

void report_to_aliyun(uint8_t type, uint8_t *value, uint16_t len)
{
    if (!value || len == 0) return;
    uint16_t one_packet_len = 200;   //256;  //128;    //64;   //修改
    uint16_t packet_num = len / one_packet_len;
    uint16_t i,j;
    cJSON *firstItem = NULL, *secondItem = NULL, *thirdItem = NULL;
    char *p_str = NULL;
    char *json_buff = malloc(1200);     //1024  //修改
    if((len % one_packet_len) != 0)
        packet_num ++;
    printf("%d packet to report_to_aliyun\n", packet_num);
    for(i = 0; i < packet_num; i++)
    {
        printf("i = %d\n",i);
        if((i == (packet_num - 1)) && ((len % one_packet_len) != 0))
        {
            printf("ii = %d\n",i);
            firstItem = cJSON_CreateObject();
            cJSON_AddItemToObject(firstItem,"id",cJSON_CreateString(device_info->id));
            cJSON_AddItemToObject(firstItem,"ts",cJSON_CreateNumber(device_info->utc.time_stamp));
            cJSON_AddItemToObject(firstItem,"type",cJSON_CreateNumber(type));
            cJSON_AddItemToObject(firstItem,"report",cJSON_CreateString(json_report_name));

            secondItem = cJSON_CreateObject();
            cJSON_AddItemToObject(secondItem,"len",cJSON_CreateNumber(len));
            cJSON_AddItemToObject(secondItem,"allPacket", cJSON_CreateNumber(packet_num));
            cJSON_AddItemToObject(secondItem,"packet", cJSON_CreateNumber(i));
            cJSON_AddItemToObject(secondItem,"packetLen",cJSON_CreateNumber(len % one_packet_len));
            
            thirdItem = cJSON_CreateArray();
            
            for(j = 0; j < len % one_packet_len; j ++)
            {
                cJSON_AddItemToArray(thirdItem, cJSON_CreateNumber(value[i*one_packet_len + j]));
            }
            cJSON_AddItemToObject(secondItem,"value",thirdItem);
            cJSON_AddItemToObject(firstItem,"data",secondItem);
            p_str = cJSON_Print(firstItem);
            if(p_str)
            {
                cJSON_Minify(p_str);
                printf("%s\n", p_str); //打印创建的字符串
                if(get_mqtt_status())
                {   
                    
                    if(mqtt_send_mutex == true)
                    {
                        mqtt_send_mutex = false;
                        esp_mqtt_client_publish(client, user_sleep_data_publish_topic, (char *)p_str, strlen((char *)p_str), 0, 0);
                        mqtt_send_mutex = true;                        
                    }
                    else
                    {
                        printf("mqtt_send_mutex1\n");
                    }
                }
                free(p_str);                      //一定要记得释放,不然会导致内存泄漏
                p_str = NULL;
            }
            else printf("p_str fail\n");
            cJSON_Delete(firstItem); 
            continue;
        }
        
        printf("i = %d\n",i);
        firstItem = cJSON_CreateObject();
        cJSON_AddItemToObject(firstItem,"id",cJSON_CreateString(device_info->id));
        cJSON_AddItemToObject(firstItem,"ts",cJSON_CreateNumber(device_info->utc.time_stamp));
        cJSON_AddItemToObject(firstItem,"type",cJSON_CreateNumber(type));
        cJSON_AddItemToObject(firstItem,"report",cJSON_CreateString(json_report_name));

        secondItem = cJSON_CreateObject();
        cJSON_AddItemToObject(secondItem,"len",cJSON_CreateNumber(len));
        cJSON_AddItemToObject(secondItem,"allPacket", cJSON_CreateNumber(packet_num));
        cJSON_AddItemToObject(secondItem,"packet", cJSON_CreateNumber(i));
        cJSON_AddItemToObject(secondItem,"packetLen",cJSON_CreateNumber(one_packet_len));
        
        thirdItem = cJSON_CreateArray();
        
        for(j = 0; j < one_packet_len; j ++)
        {
            cJSON_AddItemToArray(thirdItem, cJSON_CreateNumber(value[i*one_packet_len + j]));
        }

        cJSON_AddItemToObject(secondItem,"value",thirdItem);
        cJSON_AddItemToObject(firstItem,"data",secondItem);
        memset(json_buff, 0, 1200);     //1024
        cJSON_PrintPreallocated(firstItem, json_buff, 1200, 1);   //1024
        printf("%s\n", json_buff); //打印创建的字符串
        if(get_mqtt_status())
        {   
            if(mqtt_send_mutex == true)
            {
                mqtt_send_mutex = false;
                esp_mqtt_client_publish(client, user_sleep_data_publish_topic, (char *)json_buff, strlen((char *)json_buff), 0, 0);
                mqtt_send_mutex = true;                        
            }
            else
            {
                printf("mqtt_send_mutex2\n");
            }
        }
        
        cJSON_Delete(firstItem); 
    }
    free(json_buff);
}
void single_cmd_parse(uint8_t *value, uint16_t len, uint8_t type)
{
    qs_ret_code_t ret;
    qs_pb_msg_sensor_5sec_info *m_5s_pb_msg_de;
    qs_pb_msg_sensor_1min_info *m_60s_pb_msg_de;
    qs_pb_msg_cli_command *m_cli_command_de;//命令数据 4
    qs_pb_msg_sleep_cycle_repo *m_sleep_cycle_pb_msg_de;//综合数据 5
    qs_pb_msg_state_raw_data *m_state_pb_msg_de;//睡眠分期  6
    qs_pb_msg_heartbeat_raw_data *m_heartbeat_pb_msg_de;//心率  7
    qs_pb_msg_breathrate_raw_data *m_breathrate_pb_msg_de;//呼吸率  8
    qs_pb_msg_movement_raw_data *m_movement_pb_msg_de;//体动    9
    qs_pb_msg_snore_raw_data *m_snore_pb_msg_de;//打鼾  a
    qs_pb_msg_sbp_raw_data *m_sbp_pb_msg_de;//收缩压    b
    qs_pb_msg_dbp_raw_data *m_dbp_pb_msg_de;//舒张压    c
    char send_json_value[1024] = {0};
    printf("report %d\n",type);
    switch (type)
    {
    case 0x04:
    printf("report %d\n",type);
        m_cli_command_de = (qs_pb_msg_cli_command *)malloc(sizeof(qs_pb_msg_cli_command));
        memset(m_cli_command_de, 0, sizeof(qs_pb_msg_cli_command));
        ret = qs_pb_cli_command_decode((char *)value, len, m_cli_command_de);
        if(ret == QS_SUCCESS)
        {
            if(strstr(m_cli_command_de->command, "list updata") != NULL){
                printf("get list updata cil\n");
                //set_mode_flag_config(0);
                // set_bc(device_info->utc.time_stamp, m_cli_command_de->command, 1, 0, device_info->id, 200);
            }
           /*
                else if{strstr(m_cli_command_de->command, "hello") != NULL
                    set_bc(device_info->utc.time_stamp, "set addr 3", 1, 0, return_value, 200);
                    printf("get hello and set addr 3\n");
                }
            */
            printf("在再再在 cli_command_de cmd = %s\n",m_cli_command_de->command);
        }
        free(m_cli_command_de);
        break;
    case 0x05:
    printf("report %d\n",type);
        m_sleep_cycle_pb_msg_de = (qs_pb_msg_sleep_cycle_repo *)malloc(sizeof(qs_pb_msg_sleep_cycle_repo));
        memset(m_sleep_cycle_pb_msg_de, 0, sizeof(qs_pb_msg_sleep_cycle_repo));
        ret = qs_pb_sleep_cycle_repo_decode((char *)value, len, m_sleep_cycle_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            //printf("sleep_cycle_len = %d\n",m_sleep_cycle_pb_msg_de->cal_result);
        
            memset(send_json_value,0,1024);
            sprintf(send_json_value,"{\"id\":\"%s\",\"ts\":%d,\"type\":5,\"report\":\"%s\",\"data\":{\"calResult\":%d,\"startTime\":%d,\"totalSleepTime\":%d,\"sleepEfficiency\":%d,\"sleepQuality\":%d,\"turnoverTimes\":%d,\"sleepLatency\":%d,\"offBedTimes\":%d,\"cRSD\":%d,\"slop1\":%d,\"slop2\":%d,\"osaTimes\":%d}}",
                                                                                                        device_info->id,
                                                                                                        device_info->utc.time_stamp,
                                                                                                        json_report_name,
                                                                                                        m_sleep_cycle_pb_msg_de->cal_result,
                                                                                                        m_sleep_cycle_pb_msg_de->start_time,
                                                                                                        m_sleep_cycle_pb_msg_de->total_sleep_time,
                                                                                                        m_sleep_cycle_pb_msg_de->sleep_efficiency,
                                                                                                        m_sleep_cycle_pb_msg_de->sleep_quality,
                                                                                                        m_sleep_cycle_pb_msg_de->turnover_times,
                                                                                                        m_sleep_cycle_pb_msg_de->sleep_latency,
                                                                                                        m_sleep_cycle_pb_msg_de->off_bed_times,
                                                                                                        m_sleep_cycle_pb_msg_de->cRSD,
                                                                                                        m_sleep_cycle_pb_msg_de->slop1,
                                                                                                        m_sleep_cycle_pb_msg_de->slop2,
                                                                                                        m_sleep_cycle_pb_msg_de->oSA_times);
            printf("%s\n",send_json_value);
          
            if(get_mqtt_status())
            {
                if(mqtt_send_mutex == true)
                {
                    mqtt_send_mutex = false;
                    // printf("%s\n",send_json_value);
                    esp_mqtt_client_publish(client, user_sleep_data_publish_topic, (char *)send_json_value, strlen((char *)send_json_value), 0, 0);
                    mqtt_send_mutex = true;
                }
            }
        }
        free(m_sleep_cycle_pb_msg_de);
        break;
    case 0x06:
    printf("report %d\n",type);
        m_state_pb_msg_de = (qs_pb_msg_state_raw_data *)malloc(sizeof(qs_pb_msg_state_raw_data));
        memset(m_state_pb_msg_de, 0, sizeof(qs_pb_msg_state_raw_data));
        ret = qs_pb_state_raw_data_decode((char *)value, len, m_state_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            printf("state_len = %d\n",m_state_pb_msg_de->data_len);
            report_to_aliyun(type, m_state_pb_msg_de->data, m_state_pb_msg_de->data_len);
        }

        free(m_state_pb_msg_de);
        break;
    case 0x07:
    printf("report %d\n",type);
        m_heartbeat_pb_msg_de = (qs_pb_msg_heartbeat_raw_data *)malloc(sizeof(qs_pb_msg_heartbeat_raw_data));
        memset(m_heartbeat_pb_msg_de, 0, sizeof(qs_pb_msg_heartbeat_raw_data));
        ret = qs_pb_heartbeat_raw_data_decode((char *)value, len, m_heartbeat_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            printf("heartbeat_len = %d\n",m_heartbeat_pb_msg_de->data_len);
            report_to_aliyun(type, m_heartbeat_pb_msg_de->data, m_heartbeat_pb_msg_de->data_len);
        }

        free(m_heartbeat_pb_msg_de);
        break;
    case 0x08:
    printf("report %d\n",type);
        m_breathrate_pb_msg_de = (qs_pb_msg_breathrate_raw_data *)malloc(sizeof(qs_pb_msg_breathrate_raw_data));
        memset(m_breathrate_pb_msg_de, 0, sizeof(qs_pb_msg_breathrate_raw_data));
        ret = qs_pb_breathrate_raw_data_decode((char *)value, len, m_breathrate_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            printf("breathrate_len = %d\n",m_breathrate_pb_msg_de->data_len);
            report_to_aliyun(type, m_breathrate_pb_msg_de->data, m_breathrate_pb_msg_de->data_len);
        }

        free(m_breathrate_pb_msg_de);
        break;
    case 0x09:
    printf("report %d\n",type);
        m_movement_pb_msg_de = (qs_pb_msg_movement_raw_data *)malloc(sizeof(qs_pb_msg_movement_raw_data));
        memset(m_movement_pb_msg_de, 0, sizeof(qs_pb_msg_movement_raw_data));
        ret = qs_pb_movement_raw_data_decode((char *)value, len, m_movement_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            printf("movemen_len = %d\n",m_movement_pb_msg_de->data_len);
            report_to_aliyun(type, m_movement_pb_msg_de->data, m_movement_pb_msg_de->data_len);
        }

        free(m_movement_pb_msg_de);
        break;
    case 0x0a:
    printf("report %d\n",type);
        m_snore_pb_msg_de = (qs_pb_msg_snore_raw_data *)malloc(sizeof(qs_pb_msg_snore_raw_data));
        memset(m_snore_pb_msg_de, 0, sizeof(qs_pb_msg_snore_raw_data));
        ret = qs_pb_snore_raw_data_decode((char *)value, len, m_snore_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            printf("snore_len = %d\n",m_snore_pb_msg_de->data_len);
            report_to_aliyun(type, m_snore_pb_msg_de->data, m_snore_pb_msg_de->data_len);
        }
 
        free(m_snore_pb_msg_de);
        break;
    case 0x0b:
    printf("report %d\n",type);
        m_sbp_pb_msg_de = (qs_pb_msg_sbp_raw_data *)malloc(sizeof(qs_pb_msg_sbp_raw_data));
        memset(m_sbp_pb_msg_de, 0, sizeof(qs_pb_msg_sbp_raw_data));
        ret = qs_pb_sbp_raw_data_decode((char *)value, len, m_sbp_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            printf("sbp_len = %d\n",m_sbp_pb_msg_de->data_len);
            report_to_aliyun(type, m_sbp_pb_msg_de->data, m_sbp_pb_msg_de->data_len);
        }

        free(m_sbp_pb_msg_de);
        break;
    case 0x0c:
    printf("report %d\n",type);
        m_dbp_pb_msg_de = (qs_pb_msg_dbp_raw_data *)malloc(sizeof(qs_pb_msg_dbp_raw_data));
        memset(m_dbp_pb_msg_de, 0, sizeof(qs_pb_msg_dbp_raw_data));
        ret = qs_pb_dbp_raw_data_decode((char *)value, len, m_dbp_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            printf("dbp_len = %d\n",m_dbp_pb_msg_de->data_len);
            report_to_aliyun(type, m_dbp_pb_msg_de->data, m_dbp_pb_msg_de->data_len);
        }
  
        free(m_dbp_pb_msg_de);
        break;
    case 0x0d:
        m_5s_pb_msg_de = (qs_pb_msg_sensor_5sec_info *)malloc(sizeof(qs_pb_msg_sensor_5sec_info));
        memset(m_5s_pb_msg_de, 0, sizeof(qs_pb_msg_sensor_5sec_info));
        printf("5s: 单帧");
        //  for (uint8_t i = 0; i < len; i++)
        //  {
        //      printf("%x ", value[i]);
        //  }
        printf("\n\r");
        ret = qs_pb_sensor_5sec_info_decode((char *)value, len, m_5s_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            // printf("device_id = %s\n",m_5s_pb_msg_de->device_id);
            // printf("timestamp = %d\n",m_5s_pb_msg_de->timestamp);
            // printf("state--// ");
            // for (uint8_t i = 0; i < 12; i++)
            // {
            //     printf("%x ", m_5s_pb_msg_de->status[i]);
            // }
            // printf("\n\r");
            if(get_5s_flag == false)
            {
                get_5s_flag = true;
            }
            memcpy(user_5s_sensor_info, m_5s_pb_msg_de, sizeof(qs_pb_msg_sensor_5sec_info));
        }
        free(m_5s_pb_msg_de);
        break;
    case 0x0e:
        m_60s_pb_msg_de = (qs_pb_msg_sensor_1min_info *)malloc(sizeof(qs_pb_msg_sensor_1min_info));
        memset(m_60s_pb_msg_de, 0, sizeof(qs_pb_msg_sensor_1min_info));
        printf("60s:单帧");
        // for (uint8_t i = 0; i < len; i++)
        // {
        //     printf("%x ", value[i]);
        // }
        printf("\n\r");
        ret = qs_pb_sensor_1min_info_decode((char *)value, len, m_60s_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            // printf("device_id = %s\n",m_60s_pb_msg_de->device_id);
            // printf("timestamp = %d\n",m_60s_pb_msg_de->timestamp);
            // printf("heartbeat = %d\n",m_60s_pb_msg_de->heartbeat);
            // printf("breath_rate = %d\n",m_60s_pb_msg_de->breath_rate);
            // printf("\n\r"); 
            memcpy(user_60s_sensor_info, m_60s_pb_msg_de, sizeof(qs_pb_msg_sensor_1min_info));
        }
        free(m_60s_pb_msg_de);
        break;
    
    default:
        break;
    }
    
}

void muilt_cmd_parse(uint8_t *value, size_t len)
{
    uint8_t cmd_num = 0;
    uint16_t mux_sing_cmd_len = 0;
    uint8_t type =0;
    size_t value_offset = 0;

    for(uint16_t i=0;i<len;i++)
    {
        if(value[i] == 0xaa && value[i+1] == 0x55)
        {
            cmd_num ++;
        }
    }

    printf("has %d frams\n",cmd_num);

    for(uint8_t i=0;i<cmd_num;i++)
    {   

        printf("%d frams is parse ...\n",i);
        memset(sing_cmd_value, 0, 1024 * sizeof(uint8_t));
        memcpy(sing_cmd_value, &value[value_offset], value[value_offset + 3] + 8);//memcpy(sing_cmd_value, value, value[3] + 8);
        // for (uint16_t j = 0; j < value[3] + 8; j++)
        // {
        //     printf("%x ", sing_cmd_value[j]);
        // }
        printf("\n\r");
        value_offset = value_offset + value[value_offset + 3] + 8;//len = len - value[3] -8;
        //memcpy(value, value + value[3] + 8, len);
        if((sing_cmd_value[2] & 0xf0) == 0)   //单帧
        {
            single_cmd_parse(sing_cmd_value + 7, sing_cmd_value[3] - 1, sing_cmd_value[6]);
        }
        else if((sing_cmd_value[2] & 0xf0) > 0)
        {
            printf("mux_sing_cmd_value %x\n",sing_cmd_value[2]);
            if((sing_cmd_value[2] == 0x11))    //多帧首帧
            {
                type = sing_cmd_value[6];
                memcpy(mux_sing_cmd_value + mux_sing_cmd_len, sing_cmd_value + 7, sing_cmd_value[3] - 1);
                mux_sing_cmd_len += sing_cmd_value[3] - 1;
                printf("0x11 len = %d\n",mux_sing_cmd_len);
            }
            else if((sing_cmd_value[2] & 0x10) == 0x10)   //多帧非尾帧
            {
                memcpy(mux_sing_cmd_value + mux_sing_cmd_len, sing_cmd_value + 6, sing_cmd_value[3]);
                mux_sing_cmd_len += sing_cmd_value[3];
                printf("0x31 len = %d\n",mux_sing_cmd_len);
            }
            else if((sing_cmd_value[2] & 0x10) == 0x00)   //多帧尾帧
            {
                memcpy(mux_sing_cmd_value + mux_sing_cmd_len, sing_cmd_value + 6, sing_cmd_value[3]);
                mux_sing_cmd_len += sing_cmd_value[3];
                printf("0x41 len = %d\n",mux_sing_cmd_len);
                single_cmd_parse(mux_sing_cmd_value, mux_sing_cmd_len, type);    //解析数据
                printf("mux: ");
                // for (uint16_t i = 0; i < mux_sing_cmd_len; i++)
                // {
                //     printf("%x ", mux_sing_cmd_value[i]);
                // }
                printf("\n\r");
                mux_sing_cmd_len = 0;
            }
        }
    }

}

char *report_muilt_cmd_parse(uint8_t *value, size_t len)
{
    uint8_t cmd_num = 0;
    uint8_t sing_cmd_value2[256] = {0};     //缩减 2048
    uint8_t temp[2304] = {0};               //缩减 4096
    uint32_t report_len = 0;
    char *report = (char*)malloc(sizeof(char) * 1024);

    qs_pb_msg_state_raw_data *m_state_pb_msg_de = (qs_pb_msg_state_raw_data *)malloc(sizeof(qs_pb_msg_state_raw_data));

    for(uint16_t i=0;i<len;i++)   //分帧
    {
        if(value[i] == 0xaa && value[i+1] == 0x55)
        {
            cmd_num ++;   //帧数
        }
    }

    for(uint8_t i=0;i<cmd_num;i++)
    {   
        vTaskDelay(10 / portTICK_PERIOD_MS);
        memcpy(sing_cmd_value2, value, value[3] + 8);
        printf("report_muilt_cmd_parse");
        // for (uint16_t i = 0; i < value[3] + 8; i++)
        // {
        //     printf("%x ", sing_cmd_value2[i]);
        // }
        printf("\n\r");
        len = len - value[3] -8;   //剩余字节数
        memcpy(temp, value + value[3] + 8, len);
        memset(value, 0, len);
        memcpy(value, temp, len);    //将数据往前推一帧
        
        memset(m_state_pb_msg_de, 0, sizeof(qs_pb_msg_state_raw_data));
        qs_ret_code_t ret = qs_pb_state_raw_data_decode((char *)sing_cmd_value2 + 7, sing_cmd_value2[3] - 1, m_state_pb_msg_de);
        if(ret == QS_SUCCESS)
        {
            // printf("%s %d\n",m_state_pb_msg_de->data, m_state_pb_msg_de->data_len);      // 注释 xiugai
            strcpy(report + report_len, (char *)m_state_pb_msg_de->data);
            report_len += strlen((char *)m_state_pb_msg_de->data);
        }
    }
    free(m_state_pb_msg_de);
    return report;
}
void uart_data_parser_task(void *pv)
{
    uart_recbuff = (uint8_t *) malloc(RX_BUF_SIZE * sizeof(uint8_t));
    //uart_recbuff = (uint8_t *) heap_caps_malloc(RX_BUF_SIZE * sizeof(uint8_t), MALLOC_CAP_SPIRAM); 
    memset(uart_recbuff, 0, RX_BUF_SIZE * sizeof(uint8_t));  
    sing_cmd_value = (uint8_t *)malloc(1024 * sizeof(uint8_t));    //缩减1024
    memset(sing_cmd_value, 0, 1024 * sizeof(uint8_t));
    mux_sing_cmd_value = (uint8_t *)malloc(2048 * sizeof(uint8_t));    //缩减2048
    memset(mux_sing_cmd_value, 0, 2048 * sizeof(uint8_t));

    uart1_config(UART1_TXD, UART1_RXD);
    vTaskDelay(500 / portTICK_PERIOD_MS);   //test
    printf("为什么还没有进入uart_data_parser_task\n");
    while (1)
    { 
        // printf("sensor_upgrade_flag = %d\n",sensor_upgrade_flag);
        if(sensor_upgrade_flag != 0)
        {
            if(device_info->ble->flag == 0 || sensor_ota_mode_cnt == 1000)
            {
                sensor_upgrade_flag = 0;
                pause_uart_task = true;
                char set_mode[] = "set mode 4";
                char return_value[50] = {0};
                set_bc(device_info->utc.time_stamp, set_mode, 1, 0, return_value, 100);
                printf("set mode 4 %s\n",return_value);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                sensor_ota_mode_cnt = 0;

            }
            else
            {
                sensor_ota_mode_cnt++;
            }  
        }
        
  
        while(pause_uart_task)
        {
            int rxBytes = uart_read_bytes(UART_NUM_1, uart_recbuff, RX_BUF_SIZE, 20 / portTICK_RATE_MS);
            if (rxBytes > 0) 
            {
                // printf("uart_rx Bytes:%d\n",rxBytes);
                // printf("bc data:// ");
                // for (uint16_t i = 0; i < rxBytes; i++)
                // {
                //     printf("%x ", uart_recbuff[i]);
                // }
                // printf("\n\r");
                if(uart_recbuff[0] == 0xaa && uart_recbuff[1] == 0x55)
                {
                    // if((devic_id_flag == 0)||(uart_recbuff[6]==4))   //xiugai
                    if(devic_id_flag == 0)
                    {
                        memcpy(device_id,&uart_recbuff[9],10);
                        printf("devic_id finish device_id = %s\n",device_id);
                        devic_id_flag = 1;

                        printf("sssset addr 3 \n");    //xiugai
                        char return_value[100] = {0};
                        set_bc(device_info->utc.time_stamp, "set addr 3", 1, 0, return_value, 10);
                        printf("set addr 3 = %s\n",return_value);
                        // set_bc(device_info->utc.time_stamp, "version", 1, 0, device_version, 200);
                        // printf("bc version = %s\n",device_version);         
                        set_bc(device_info->utc.time_stamp, "set mode 4", 0, 0, return_value, 10);
                        // printf("set mode 4 = %s\n",return_value);                
                    }
                    
                    //单帧单指令
                    if(uart_recbuff[3] == rxBytes-8)
                    {
                        // printf(" 串口_单帧数据 \n"); 
                        single_cmd_parse(uart_recbuff + 7, rxBytes - 9, uart_recbuff[6]);
                    }
                    //单帧多指令
                    else
                    {
                        // printf(" 串口_多帧数据 \n"); 
                        muilt_cmd_parse(uart_recbuff, rxBytes);
                    }        
                }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);   
    }
    free(uart_recbuff);
    free(sing_cmd_value);
    free(mux_sing_cmd_value);
    vTaskDelete(NULL);
}

//参数显示任务
void data_display_task(void *pv)
{
    while(1)
    {
        ESP_LOGI(TAG, "/*======111=====================data display===============%d===========*/",sensor_ota_mode_cnt);

        gpio_set_level(LED_RED,(device_info->wifi.flag == 1)?1:0);
        gpio_set_level(LED_GREEN,(device_info->wifi.flag == 1)?0:1);

        ESP_LOGI(TAG, "heap_size:%d,version:%s,id:%s,dataUpOn %d,sleepUping %d,sensorOta %d,current_report:%s",esp_get_free_heap_size(),device_info->ota.running_version,
                                                                                                device_info->id,device_info->data_up_switch,
                                                                                                sleep_up_flag,
                                                                                                sensor_upgrade_flag,
                                                                                                device_info->report);
        ESP_LOGI(TAG, "5s-->num:%d\tstatus:[%d][%d][%d][%d][%d]\theart:%d\tbreath:%d dev-time:%d id:%s",user_5s_sensor_info->sequence, user_5s_sensor_info->status[0],
                                                                                                user_5s_sensor_info->status[1],
                                                                                                user_5s_sensor_info->status[2],
                                                                                                user_5s_sensor_info->status[3],
                                                                                                user_5s_sensor_info->status[4],
                                                                                                user_5s_sensor_info->heartbeat,
                                                                                                user_5s_sensor_info->breathRate,user_5s_sensor_info->timestamp,user_5s_sensor_info->device_id); //test 
        ESP_LOGI(TAG, "60s-->num:%d\ton_off_bed:%d\theart:%d\tbreath:%d\tMmin:%d\tMmean:%d\tNSD:%d\tNpd:%d\tSBP:%d\tDBP:%d", 
                                                                                                user_60s_sensor_info->sequence, 
                                                                                                user_60s_sensor_info->on_off_bed,
                                                                                                user_60s_sensor_info->heartbeat,
                                                                                                user_60s_sensor_info->breath_rate,
                                                                                                user_60s_sensor_info->Mmin,
                                                                                                user_60s_sensor_info->Mmean,
                                                                                                user_60s_sensor_info->NSD,
                                                                                                user_60s_sensor_info->Npd,
                                                                                                user_60s_sensor_info->SBP,
                                                                                                user_60s_sensor_info->DBP);

        ESP_LOGI(TAG, "wifi:%d\tmqtt:%d\tble:%d\ttime:%d\tbc_device_id:%d",device_info->wifi.flag, device_info->aliyun.flag,device_info->ble->flag,device_info->utc.flag,devic_id_flag);
        if(device_info->utc.flag)
        {
            ESP_LOGI(TAG, "%d-%02d-%02d %02d:%02d:%02d  %02d %d", ti.tm_year ,ti.tm_mon, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec , ti.tm_wday, device_info->utc.time_stamp);
        }
        

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

int set_bc(uint32_t time_stamp, char *value, uint8_t switch_return, uint8_t switch_to_aliyun, char *return_value, uint16_t time_ms)
{
    if(sensor_upgrade_flag != 0)
    {
        printf("bc_sensor ota now,please wait\n");
        return 0;
    }

    // printf("cmd:%s\n",value);
    char pb[240] = {0};
    char cmd[512] = {0};
	uint16_t pb_len = sizeof(pb);
	qs_pb_msg_state_raw_data m_pb_msg_en;    //这里理论上要用qs_pb_msg_cli_command，因此导致pb_len需要减去2(结构体多了uint32，4字节，压缩后还多2字节)
    qs_pb_msg_state_raw_data *m_pb_msg_de = (qs_pb_msg_state_raw_data *)malloc(sizeof(qs_pb_msg_state_raw_data));
    memset(m_pb_msg_de, 0, sizeof(qs_pb_msg_state_raw_data));

	// char device_id[] = {"SU20001112"};   //xiugai
	strcpy(m_pb_msg_en.device_id, device_id);
	m_pb_msg_en.timestamp = time_stamp;
	m_pb_msg_en.data_len = strlen(value);
	m_pb_msg_en.data[0] = strlen(value);
	m_pb_msg_en.data[1] = 0x00;
	m_pb_msg_en.data[2] = 0x00;
	m_pb_msg_en.data[3] = 0x00;

    memcpy(m_pb_msg_en.data + 4, value, strlen(value));
    qs_pb_state_raw_data_encode(&m_pb_msg_en, &pb[0], (size_t *)&pb_len);
    // for (uint8_t i = 0; i < pb_len; i++)
	// {
	// 	printf("%x ", pb[i]);
	// }
	// printf("\n\r");

    //组帧
    cmd[0] = 0xAA;
    cmd[1] = 0x55;
    cmd[2] = 0x01;
    cmd[3] = pb_len - 1; 
    cmd[4] = 0x34;
	cmd[5] = 0x33;
	cmd[6] = 0x04;
    memcpy(cmd + 7, pb, pb_len-2);
	uint16_t crc16 = crc16_compute((uint8_t const*)cmd, pb_len-2+7);
	cmd[pb_len-2+7] = crc16 >> 8;
	cmd[pb_len-2+8] = crc16;
	// for (uint8_t i = 0; i < pb_len-2+9; i++)
	// {
	// 	printf("%x ", cmd[i]);
	// }
	// printf("\n\r");
    if(switch_return == 0)
    {
        uart_write_bytes(UART_NUM_1, cmd, pb_len + 7);
    }
    else if(switch_return == 1)
    {
        pause_uart_task = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        uart_flush(UART_NUM_1);
        //vTaskDelay(100 / portTICK_PERIOD_MS);
        uart_write_bytes(UART_NUM_1, cmd, pb_len + 7);
        int rxBytes = uart_read_bytes(UART_NUM_1, uart_recbuff, RX_BUF_SIZE, time_ms / portTICK_RATE_MS);
        if(rxBytes > 0)
        {

            if(strstr(value, "list"))
            {
                char *report = report_muilt_cmd_parse(uart_recbuff, rxBytes);  //返回睡眠报告列表
                printf("in set bc %s\n",report);
                if(return_value!=NULL)
                {
                    strcpy(return_value, (char *)report);
                }
                free(report);
            }
            else
            {
                memset(m_pb_msg_de, 0, sizeof(qs_pb_msg_state_raw_data));
                qs_pb_state_raw_data_decode((char *)uart_recbuff+7, sizeof(uart_recbuff)-9, m_pb_msg_de);
                if(return_value!=NULL)
                {
                    strcpy(return_value, (char *)m_pb_msg_de->data);
                    printf("In set bc return_value: %s\n",return_value);
                }    
                
            }    
        }
        // uart_flush(UART_NUM_1);
        // vTaskDelay(200 / portTICK_PERIOD_MS);
        pause_uart_task = true;
    }
    free(m_pb_msg_de);
    return 0;
}

int sensor_ota_bc(char *value)
{
    int time=0;
    char cmd[256] = {0};
    uint8_t temp[256] = {0};
    uint8_t size = 0;
    uint16_t i = 0;
    if(value[0] == 0x00)
    {
        size = 0x03;
    }
    else if (value[0] == 0x01)
    {
        size = 0x83;
    }
    else if (value[0] == 0x02)
    {
        size = 0x01;
    }
    
    //组帧
    cmd[0] = 0xAA;
    cmd[1] = 0x55;
    cmd[2] = 0x01;
    cmd[3] = size;
    cmd[4] = 0x47;
	cmd[5] = 0x43;
    memcpy(cmd + 6, value, size);
    uint16_t crc16 = crc16_compute((uint8_t const*)cmd, size+6);
	cmd[size+6] = crc16 >> 8;
	cmd[size+7] = crc16;

    // pause_uart_task = false;
    // vTaskDelay(50 / portTICK_PERIOD_MS);
    // uart_flush(UART_NUM_1);
    // vTaskDelay(50 / portTICK_PERIOD_MS);
    uart_write_bytes(UART_NUM_1, cmd, size+8);


    if(size <= 0x10)
    {
        for (i = 0; i < size+8; i++)
            {
                printf("%x ", cmd[i]);
            }
        printf("bc_receive SENSER OTA\n\r");
        time = 8000;
    }
    else{
        printf("bc_receive len %d SENSER OTA\n\r",size+8);
        time = 1000;
    }
    

    int rxBytes = uart_read_bytes(UART_NUM_1, uart_recbuff, RX_BUF_SIZE, time / portTICK_RATE_MS);   //等待传感器应答返回
    if(rxBytes > 0)
        {
            sensor_ota_mode_cnt = 0;

            for (i = 0; i < rxBytes; i++)
            {
                printf("%x ", uart_recbuff[i]);
                
                if(i>0x40)
                {
                    printf("uart_recbuff len %d",rxBytes);
                    break;
                }
            }
            printf("bc_send SENSER OTA\n\r");

            for(i=0;i < rxBytes; i++)
            {
                if(uart_recbuff[i] == 0xAA && uart_recbuff[i+1] == 0x55)
                {
                    temp[0] = 0xBB;
                    temp[1] = 0xCC;
                    memcpy(&temp[2], uart_recbuff +i+6, uart_recbuff[i+3]);

                    if(get_ble_status() && device_info->data_up_switch)
                    {
                        esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                            device_info->ble->conn_id,
                                            device_info->ble->handle,
                                            (uart_recbuff[i+3]+2), temp, false);
                    }
                    break;
                }
            }
            // temp[0] = 0xBB;
            // temp[1] = 0xCC;
            // memcpy(&temp[2], uart_recbuff +6, uart_recbuff[3]);

            // if(get_ble_status() && device_info->data_up_switch)
            // {
            //     esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
            //                                 device_info->ble->conn_id,
            //                                 device_info->ble->handle,
            //                                 (uart_recbuff[3]+2), temp, false);
            // }
            
        }
    else
    {
            temp[0] = 0xBB;
            temp[1] = 0xCC;
            memcpy(&temp[2], device_id, 10);
            temp[12] = 0x4D;
            temp[13] = 0x01;
            temp[14] = 0x03;
            temp[15] = value[1];
            temp[16] = value[2];
            
            if(get_ble_status() && device_info->data_up_switch)
            {
                esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                            device_info->ble->conn_id,
                                            device_info->ble->handle,
                                            17, temp, false);
            }
        printf("bc sensor upgrade no back date\n\r");
    }


    // pause_uart_task = true;

    return 0;
}

//utc 更新完毕 回调
void sntp_update_flag(struct timeval* tv) 
{
    device_info->utc.flag = true;
}

uint32_t find_report_time(char *report_name, uint8_t len)
{
    // printf("%s\n", report_name);
    struct tm stm;  
    int iY, iM, iD, iH, iMin, iS;  
    
    memset(&stm, 0, sizeof(stm));  
    iY =    atoi(report_name + len - 23);
    // printf("iY = %d\n",iY);  
    iM =    atoi(report_name + len - 18); 
    // printf("iM = %d\n",iM);  
    iD =    atoi(report_name + len - 15);
    // printf("iD = %d\n",iD);   
    iH =    atoi(report_name + len - 12);
    // printf("iH = %d\n",iH);   
    iMin =  atoi(report_name + len - 9); 
    // printf("iMin = %d\n",iMin);  
    iS =    atoi(report_name + len - 6);  
    // printf("iS = %d\n",iS);  

    stm.tm_year=iY-1900;  
    stm.tm_mon=iM-1;  
    stm.tm_mday=iD;  
    stm.tm_hour=iH;  
    stm.tm_min=iMin;  
    stm.tm_sec=iS;  

    /*printf("%d-%0d-%0d %0d:%0d:%0d\n", iY, iM, iD, iH, iMin, iS);*/   //标准时间格式例如：2016:08:02 12:12:30
    return (uint32_t)mktime(&stm);
}

void check_report_and_up_to_aliyun(void)
{
    //uint64_t start_time = esp_timer_get_time(); 
    char return_value[1024] = {0};
    char json_buff[512] = {0};
    uint16_t offset = 0;
    char temp[3] = {0};
    char get_report_cmd1[] = "list";
    char get_report_cmd2[] = "report 0";
    uint8_t report_num = 0;
    char report_name[30][30] = {0};  //报告列表
    uint32_t report_time[30] = {0};   //报告对应的时间
    uint8_t report_len[30] = {0};   //报告名长度
    uint32_t current_time;
    nvs_handle nvs_config_handler;

    memset(return_value, 0, 1024);
    printf("get_report_cmd1\n");
    set_bc(device_info->utc.time_stamp, get_report_cmd1, 1, 1, return_value, 1000);
    // printf("cil = list :return_value = \n %s\nstrlen(return_value) = %d\n", return_value, strlen(return_value));

    if(strstr(return_value,"NONE"))
    {
        printf("no report, need't to up to aliyun\n");
    }
    else
    { 
        uint16_t len = strlen(return_value);
        for(uint16_t i=0; i<len; i++)
        {
            report_len[report_num] ++ ;
            // printf("report_len[%d] = %d, return_value[%d] = %c\n", report_num, report_len[report_num], i, return_value[i]);
            if(return_value[i] == '\n')
            {
                report_num++;
            } 
            if (report_len[report_num] >= 28)
            {
                printf("report_len[%d] = %d, return_value[%d] = %c\n", report_num, report_len[report_num], i, return_value[i]);
                report_len[report_num] = 28;
                return_value[i] = '\n';
                report_num++;
                printf("Warning: %d 报告名超过了28个字节 设置为28\n", report_num);
            }
            if(report_num > 30)
            {
                report_num = 30;
                printf("Warning: %d 报告数超过了30个 设置为0\n", report_num);
            }
        }

        printf("report has %d total\n", report_num);

        //分割报告名
        for(uint16_t i=0; i<report_num; i++)
        {
            memcpy(report_name[i], return_value + offset, report_len[i] - 2);
            offset += report_len[i] ;
            //printf("report_name[%d] = %s\n", i, report_name[i]);
            report_time[i] = find_report_time(report_name[i], strlen(report_name[i]));
            //printf("report_time[%d] = %d\n",i, report_time[i]);
        }
        
        printf("current report = %s\n",device_info->report);
        //初始报告全部上报
        if(strstr(device_info->report, "NONE"))
        {
            current_time = 0;
            printf("Report is being updated ...\n");
            
            //发送具体数据
            for(uint16_t i=0; i<report_num; i++)
            {
                printf("%s\n",report_name[i]);
                memset(json_buff, 0, 512);
                //发送报告名
                sprintf(json_buff, "{\"id\":\"%s\",\"ts\":%d,\"type\":4,\"report\":\"%s\",\"data\":\"%s\"}",
                                                                                        device_info->id,
                                                                                        device_info->utc.time_stamp,
                                                                                        report_name[i],
                                                                                        report_name[i]);
                printf("%s\n",json_buff);                                                                       
                //
                if(get_mqtt_status() && device_info->data_up_switch)
                {
                    if(mqtt_send_mutex == true)
                    {
                        mqtt_send_mutex = false;
                        esp_mqtt_client_publish(client, user_sleep_data_publish_topic, (char *)json_buff, strlen((char *)json_buff), 0, 0);
                        //vTaskDelay(1000 / portTICK_PERIOD_MS);
                        mqtt_send_mutex = true;
                    }
                }
                memcpy(temp, report_name[i], 2); 
                //printf("%d\n",atoi(temp));
                sprintf(get_report_cmd2, "report %d",atoi(temp));
                printf("初始化全部上报111111.%s\n",get_report_cmd2);
                if(current_time < report_time[i])
                {
                    //存储最新的报告名
                    ESP_ERROR_CHECK(nvs_open("config_cfg", NVS_READWRITE, &nvs_config_handler));
                    ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "report", report_name[i]));
                    ESP_ERROR_CHECK(nvs_commit(nvs_config_handler));
                    nvs_close(nvs_config_handler);
                    memset(device_info->report,0,128);
                    memcpy(device_info->report, report_name[i], strlen(report_name[i]));
                    printf("recent report= %s\n", device_info->report);
                    // current_time = find_report_time(device_info->report, strlen(device_info->report));
                    current_time = report_time[i];
                }

                if(device_info->data_up_switch)
                {
                    if(sleep_up_flag == 1)
                    {
                        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);   //??隐患：在延时期间，报告上传指令1分钟结束，sleep_up_flag回到0，此时又接到云端上传指令
                    }
                    sleep_up_flag = 1;
                    memset(json_report_name, 0, 32);
                    strcpy(json_report_name, report_name[i]);
                    set_bc(device_info->utc.time_stamp, get_report_cmd2, 0, 0, return_value, 100);   //发送读取report指令
                    vTaskDelay(15*1000 / portTICK_PERIOD_MS);
                    sleep_up_flag = 0;
                }

                //printf("current_time = %d\n",current_time);
                //printf("report_time[%d] = %d\n",i,report_time[i]);
            }  
        }
        //检测有无新报告，并上报
        else
        {
            //printf("为什么没有进入新报告上传！ report_num = %d\n",report_num);
            for(uint16_t i=0; i<report_num; i++)
            {  
                current_time = find_report_time(device_info->report, strlen(device_info->report));
                printf("current_time = %d\n",current_time);
                //printf("report_time[%d] = %d\n",i,report_time[i]);
                if(current_time < report_time[i])
                {
                    sprintf(json_buff, "{\"id\":\"%s\",\"ts\":%d,\"type\":4,\"report\":\"%s\",\"data\":\"%s\"}",
                                                                                        device_info->id,
                                                                                        device_info->utc.time_stamp,
                                                                                        report_name[i],
                                                                                        report_name[i]);
                    printf("%s\n",json_buff);
                    //                                                                    
                    if(get_mqtt_status()&& device_info->data_up_switch)
                    {
                        //if(mqtt_send_mutex == true)
                        {
                            mqtt_send_mutex = false;
                            esp_mqtt_client_publish(client, user_sleep_data_publish_topic, (char *)json_buff, strlen((char *)json_buff), 0, 0);
                            vTaskDelay(1000 / portTICK_PERIOD_MS);
                            mqtt_send_mutex = true;
                        }
                    }

                    memcpy(temp, report_name[i], 2);
                    printf("uploading : %s\n",report_name[i]);
                    //printf("%d\n",atoi(temp));
                    sprintf(get_report_cmd2, "report %d",atoi(temp));
                    printf("存储最新报告名2222222.%s\n",get_report_cmd2);

                    // //存储最新的报告名
                    ESP_ERROR_CHECK(nvs_open("config_cfg", NVS_READWRITE, &nvs_config_handler));
                    ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "report", report_name[i]));
                    ESP_ERROR_CHECK(nvs_commit(nvs_config_handler));
                    nvs_close(nvs_config_handler);
                    memset(device_info->report,0,128);
                    memcpy(device_info->report, report_name[i], strlen(report_name[i]));

                    if(device_info->data_up_switch)
                    {
                        if(sleep_up_flag == 1)
                        {
                            vTaskDelay(5* 1000 / portTICK_PERIOD_MS);
                        }
                        sleep_up_flag = 1;
                        memset(json_report_name, 0, 32);
                        strcpy(json_report_name, report_name[i]);
                        set_bc(device_info->utc.time_stamp, get_report_cmd2, 0, 0, return_value, 100);
                        vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
                        //set_bc(device_info->utc.time_stamp, get_report_cmd2, 0, 0, return_value, 1000);   //发送读取report指令
                        sleep_up_flag = 0;
                    }

                } 
            }
        }
    }
    //int64_t end_time = esp_timer_get_time();
    //printf("check_report_and_up_to_aliyun time = %lld ms\n", (end_time - start_time)/1000);
}

//utc 获取任务
void utc_get_task(void *pv)
{
    static uint8_t utc_first_flag = 1;
    static char time_stamp_to_string[20] = {0};
    static char set_rtc_cmd[100] = {0};
    static char return_value[100] = {0};  
    static char get_version_cmd[] = "version";  
    if(utc_first_flag)
    {
        utc_first_flag=0;

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    //等待WIFI连接
    while(!get_wifi_status())
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    //时间更正
    sntp_set_time_sync_notification_cb(&sntp_update_flag);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp.aliyun.com");
    sntp_init();
    setenv("TZ", "CST-8", 1);
    tzset();
    //等待utc更新完毕
    while(device_info->utc.flag == false)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    //更新博创设备时间
    while(devic_id_flag == 0)      //等待device_id更新赋值
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }


    time(&now);
    localtime_r(&now, &ti);
    device_info->utc.time_stamp = mktime(&ti);

    itoa(device_info->utc.time_stamp, time_stamp_to_string, 10);

    sprintf(set_rtc_cmd, "set rtc %s", time_stamp_to_string);
    printf("set_rtc_cmd\n");
    set_bc(device_info->utc.time_stamp, set_rtc_cmd, 1, 0, return_value, 10);
    set_rtc_flag = 1;
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // printf("bc version = %s\n",return_value);

    printf("get_version_cmd\n");
    set_bc(device_info->utc.time_stamp, get_version_cmd, 1, 0, device_version, 20);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    printf("bc version = %s\n",device_version);
    }
    while(1)
    {
/**
调用sntp_init()会立刻请求服务器同步一次时间。
因此，我们需要主动同步时：
先调用sntp_stop()、再调用sntp_init() 即可立刻同步一次时间。
*/
        
        time(&now);
		
		localtime_r(&now, &ti);
        device_info->utc.time_stamp = mktime(&ti);
		ti.tm_year = ti.tm_year + 1900;
		ti.tm_mon = ti.tm_mon + 1;
		device_info->utc.time[0] = ti.tm_year ;			//年
		device_info->utc.time[1] = ti.tm_mon ;			//月
		device_info->utc.time[2] = ti.tm_mday ;			//日
		device_info->utc.time[3] = ti.tm_hour ;			//时
		device_info->utc.time[4] = ti.tm_min;			//分
		device_info->utc.time[5] = ti.tm_sec ;			//秒

        if(set_rtc_flag == 0)
        {
            while(devic_id_flag == 0)      //等待device_id更新赋值
            {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }

            time(&now);
            localtime_r(&now, &ti);
            device_info->utc.time_stamp = mktime(&ti);
            itoa(device_info->utc.time_stamp, time_stamp_to_string, 10);
            memset(set_rtc_cmd,0,100);
            sprintf(set_rtc_cmd, "set rtc %s", time_stamp_to_string);
            printf("set_rtc_cmd\n");
            set_bc(device_info->utc.time_stamp, set_rtc_cmd, 1, 0, return_value, 20);
            set_rtc_flag = 1;
            vTaskDelay(100 / portTICK_PERIOD_MS);

            memset(device_version,0,32);
            set_bc(device_info->utc.time_stamp, get_version_cmd, 1, 0, device_version, 20);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            printf("bc version = %s\n",device_version);

        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
// static uint8_t real_data_up_task_flag = 0;

void real_data_up_task(void *pv)
{
    uint8_t* temp = (uint8_t*) malloc(512);  //缩减1024
    if (temp == NULL) {
        vTaskDelete(NULL);
        return;
    }
    static uint8_t cnt_5s = UP_RATIO_60S;
    static int32_t ap_min_max = 0;
    static int32_t ap_min_min = 0;
    int32_t ap_min_temp = 0;
    static uint8_t s_5s_cnt=20;
    //while(1)
    {
        if(s_5s_cnt<20)
        {
            s_5s_cnt++;
        }
        if(sleep_up_flag == 1)
        {
            if(s_5s_cnt>11)
            {
                report_cli_up();
                s_5s_cnt=0;
            }
        }
        else
        {
            memset(temp, 0, 512);
            sprintf((char *)temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":1,\"data\":{\"heart\":%d,\"breath\":%d,\"status\":[%d,%d,%d,%d,%d]}}",
                                                                                                            device_info->id,
                                                                                                            device_info->utc.time_stamp,
                                                                                                            user_5s_sensor_info->heartbeat,
                                                                                                            user_5s_sensor_info->breathRate,
                                                                                                            user_5s_sensor_info->status[0],
                                                                                                            user_5s_sensor_info->status[1],
                                                                                                            user_5s_sensor_info->status[2],
                                                                                                            user_5s_sensor_info->status[3],
                                                                                                            user_5s_sensor_info->status[4]);
//睡眠报告上传和传感器升级时时wifi不上传实时数据
            if(get_mqtt_status() && device_info->data_up_switch && sleep_up_flag == 0 && sensor_upgrade_flag == 0)
            {
                if(mqtt_send_mutex == true)
                {
                    mqtt_send_mutex = false;
                    esp_mqtt_client_publish(client, user_5s_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
                    mqtt_send_mutex = true;
                }
            }
//传感器升级时蓝牙不上传实时数据
            if(get_ble_status() && device_info->data_up_switch && sensor_upgrade_flag == 0)
            {
                esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                            device_info->ble->conn_id,
                                            device_info->ble->handle,
                                            strlen((char *)temp), temp, false);
            }
        
            if(cnt_5s >= UP_RATIO_60S)
            {
                cnt_5s = 0;
                if((ap_min_min>user_60s_sensor_info->Mmin)||(ap_min_min==0))
                {
                    ap_min_min = user_60s_sensor_info->Mmin;
                }
                if((ap_min_max<user_60s_sensor_info->Mmin)||(ap_min_max==0))
                {
                    ap_min_max = user_60s_sensor_info->Mmin;
                }
                ap_min_temp = ap_min_min + (ap_min_max - ap_min_min)*0.1;       // 根据最小值和最大值之间的 10% 位置，计算一个新的阈值 (ap_min_temp)
                //printf("AA%d\nBB%d\nCC%d\n",ap_min_min,ap_min_max,ap_min_temp);
                if(ap_min_temp > user_60s_sensor_info->Mmin)    // 如果当前的最小压力值超过了这个阈值，且床垫当前处于“在床”状态 ，则切换床垫状态为“离床” (on_off_bed = 1)。
                {
                    if(user_60s_sensor_info->on_off_bed==0)
                    {
                        user_60s_sensor_info->on_off_bed = 1;
                        //set_bc(device_info->utc.time_stamp, "reboot", 1, 0, NULL, 200);  
                    }
                }                
                memset(temp, 0, 512);
                sprintf((char *)temp,"{\"id\":\"%s\",\"ts\":%d,\"type\":2,\"data\":{\"bed\":%d,\"heart\":%d,\"breath\":%d,\"Mmin\":%d,\"Mmean\":%d,\"NSD\":%d,\"NPD\":%d,\"SBP\":%d,\"DBP\":%d}}",
                                                                                                                device_info->id,
                                                                                                                device_info->utc.time_stamp,
                                                                                                                user_60s_sensor_info->on_off_bed,
                                                                                                                user_60s_sensor_info->heartbeat,
                                                                                                                user_60s_sensor_info->breath_rate,
                                                                                                                user_60s_sensor_info->Mmin,
                                                                                                                user_60s_sensor_info->Mmean,
                                                                                                                user_60s_sensor_info->NSD,
                                                                                                                user_60s_sensor_info->Npd,
                                                                                                                user_60s_sensor_info->SBP,
                                                                                                                user_60s_sensor_info->DBP);

            if(get_mqtt_status() && device_info->data_up_switch && sleep_up_flag == 0 && sensor_upgrade_flag == 0)
            {
                if(mqtt_send_mutex == true)
                {
                    mqtt_send_mutex = false;
                    esp_mqtt_client_publish(client, user_60s_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
                    mqtt_send_mutex = true;
                }
            
            }

                if(get_ble_status() && device_info->data_up_switch && sensor_upgrade_flag == 0)
                {
                    esp_ble_gatts_send_indicate(device_info->ble->gatts_if,
                                                device_info->ble->conn_id,
                                                device_info->ble->handle,
                                                strlen((char *)temp), temp, false);
                }
            }
            cnt_5s++;
        
        }      
        //vTaskDelay(5000 / portTICK_PERIOD_MS);

    }
    free(temp);
    vTaskDelete(NULL); 
}

void report_data_up_task(void *pv)
{
// #if 0//BLE_TEST
//     while(1)
//     {
//         bochuang_test();
//         vTaskDelay(3000 / portTICK_PERIOD_MS);
//     }
// #else        
    //while(device_info->utc.flag == false || get_5s_flag == false)
    {
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    //vTaskDelay(30*1000 / portTICK_PERIOD_MS);
    //while(1)
    {
        if(get_wifi_status() && get_mqtt_status() && ota_now_flag == 0 && sensor_upgrade_flag == 0)
        {

            check_report_and_up_to_aliyun();
        }

    }
// #endif
    vTaskDelete(NULL); 
}


void twd_task(void *arg)
{
    // 为TWDT添加任务，并检查dwt状态看是否添加
    // esp_task_wdt_add(NULL);
    // esp_task_wdt_status(NULL);
 
    while(1){
        // 每2秒重置一次看门狗
        //CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);  // 喂狗。注释此行以测试触发TWDT超时
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); 
}
extern char user_cli_data_publish_topic[100];
void test_task(void *pv)
{
    char device_data[100]={0};
    char temp[2048] = {1};
    while(0)
    {
        char return_value[1024] = {0};
        set_bc(device_info->utc.time_stamp, "report", 1, 0, return_value, 1000);
        // printf("%s\n",return_value);
        sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"%s\"}", 
            device_info->id,
            device_info->utc.time_stamp,
            "list",
            return_value);
        printf("%s\n",temp);
        //esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void test_task1(void)
{
    char device_data[100]={0};
    char temp[2048] = {0};
    //while(1)
    {
        char return_value[1024] = {0};
        set_bc(device_info->utc.time_stamp, "list", 1, 0, return_value, 1000);
        // printf("%s\n",return_value);
        sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"%s\"}", 
            device_info->id,
            device_info->utc.time_stamp,
            "list",
            return_value);
        printf("%s\n",temp);
        //esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
    }
    //vTaskDelete(NULL);
}

TaskHandle_t utc_get_task_handle = NULL;
TaskHandle_t real_data_up_task_handle = NULL;
TaskHandle_t report_data_up_task_handle = NULL;
void Task_scheduling(void *pv)
{
    //static uint8_t s_real_data_up_task_flag = 0;
    //static uint8_t s_report_data_up_task = 0;
    static uint8_t s_5s_Cnt = 119;
    static uint8_t s_1s_Cnt = 5;
    //xTaskCreatePinnedToCore(utc_get_task, "utc_get", 1024*5, NULL, 3, &utc_get_task_handle, 1);
    vTaskDelay(10000 / portTICK_PERIOD_MS);     //10s 初始化时间、版本号、传感器 ：old：5s
    while(1)
    {
        while(get_ota_now_flag()==1)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    //实时数据上报
    if(s_1s_Cnt>4)
    {
        xTaskCreatePinnedToCore(real_data_up_task, "real_data_up", 1024*6, NULL, 4, &real_data_up_task_handle, 1);   
        s_1s_Cnt=0;
        s_5s_Cnt++;
    }
    
    //报告数据上报
    if(get_mode_flag_config() == 0 || s_5s_Cnt>119)
    {
        set_mode_flag_config(2);
        if(device_info->utc.flag == true && get_5s_flag == true)
        {
            s_5s_Cnt=0;
            xTaskCreatePinnedToCore(report_data_up_task, "report_data_up", 1024*10, NULL, 10, &report_data_up_task_handle, 1);//缩减2048*7
            
        }
 
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    s_1s_Cnt++;
    //printf("s_5s_Cnt:%d\n",s_5s_Cnt);
    }
}

void M_Ctr(uint8_t data);
#include "driver/gpio.h"
void key_task(void)
{
    static uint8_t s_oldkey_data = 0xff;
    uint8_t key_data;
    while(1)
    {
        key_data = gpio_get_level(13);
        if(s_oldkey_data!=key_data)
        {
            if(key_data)
            {
                M_Ctr(0);
            }else
            {
                M_Ctr(50);
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}
void app_control_server(void)
{
    //ble 数据处理
    xTaskCreatePinnedToCore(ble_data_parser_task, "ble_task", 1024*6, NULL, 5, NULL, 1);   //1024*2//缩减1024*5
    //一键配网
    xTaskCreatePinnedToCore(one_key_config_wifi_task, "one_key_config_wifi_task", 1024*2, NULL, 6, NULL, 1);   //缩减1024*2
    //uart 数据处理
    xTaskCreatePinnedToCore(uart_data_parser_task, "uart_task", 1024*10, NULL, 5, &uart_handle, 1);//缩减2048*13
    //utc 获取
    xTaskCreatePinnedToCore(utc_get_task, "utc_get", 1024*5, NULL, 3, NULL, 1);      //缩减4096*2   ok
    //终端显示
    xTaskCreatePinnedToCore(data_display_task, "data_display", 1024*2, NULL, 2, NULL, 1);     

    xTaskCreatePinnedToCore(Task_scheduling, "Task_scheduling", 1024, NULL, 6, NULL, 1);//缩减2048*7
    // xTaskCreatePinnedToCore(key_task, "key_task", 1024, NULL, 6, NULL, 1);//缩减2048*7
}

void set_sleep_up_flag(uint8_t data)
{
    sleep_up_flag = data;
}
uint8_t get_sleep_up_flag(void)
{
    return sleep_up_flag;
}
void set_ota_now_flag(uint8_t data)
{
    ota_now_flag = data;
}
uint8_t get_ota_now_flag(void)
{
    return ota_now_flag;
}
//report_cli_data设置指令代码＋数据
void set_report_cli(uint8_t data1,uint8_t data2)
{
    report_cli_data[0] = data1;
    report_cli_data[1] = data2;
}
uint8_t get_devic_id_flag(void)
{
    return devic_id_flag;
}
void sensor_reboot_config(void)
{
    set_rtc_flag = 0;
    devic_id_flag = 0;
}

void set_mode_flag_config(uint8_t data)
{
    set_mode_flag = data;
}

uint8_t get_mode_flag_config(void)
{
    return set_mode_flag;
}