/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2022-06-17 00:44:08
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-07-27 10:37:23
 * @FilePath: /smart-air-bed-board-program/components/common/common.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "common.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "pb_decode.h"
#include "keesoncloud.pb.h"
#include "qs_protobuf.h"
#include "esp_rom_gpio.h"

static const char *TAG = "common";

device_info_t *device_info;
qs_pb_msg_sensor_5sec_info *user_5s_sensor_info;
qs_pb_msg_sensor_1min_info *user_60s_sensor_info;
qs_pb_msg_sleep_apnea_info *user_sa_sensor_info;
char last_saved_ssid[32] = {0};
char last_saved_passwd[64] = {0};
MFPData_t 		g_MFPData_t; 	// MFP数据结构体（间隔时间之类的）

void print_nvs_usage()
{
    nvs_stats_t stats;
    esp_err_t err = nvs_get_stats(NULL, &stats); // NULL 表示默认分区 16KB/32byte = 512 总共512个键值对
    if (err == ESP_OK) {
        printf("NVS usage:\n");
        printf("  Used entries: %d\n", stats.used_entries);		// 已使用键值对
        printf("  Free entries: %d\n", stats.free_entries);
        printf("  Total entries: %d\n", stats.total_entries);
    } else {
        printf("Failed to get NVS stats: %s\n", esp_err_to_name(err));
    }
}

uint8_t get_wifi_status(void)
{
	return device_info->wifi.flag;
}

//0：wifi等待连接；1：wifi连接成功；2：wifi重连；3：wifi重连失败，超过重连次数；
void set_wifi_status(uint8_t value)
{
	device_info->wifi.flag = value;
}

uint8_t get_one_key_config_wifi_status(void)
{
	return device_info->wifi.one_key_config.flag;
}
//0：wifi连接参数未修改；1：wifi连接参数修改
void set_one_key_config_wifi_status(uint8_t value)
{
	device_info->wifi.one_key_config.flag = value;
}

uint8_t get_ble_status(void)
{
	return device_info->ble->flag;
}

void set_ble_status(uint8_t value)
{
	device_info->ble->flag = value;
}

uint8_t get_mqtt_status(void)
{
	return device_info->aliyun.flag;
}

void set_mqtt_status(uint8_t value)
{
	device_info->aliyun.flag = value;
}

static qs_settings_aliyun_t m_aliyun;

static void read_aliyun_config(void)
{
	esp_partition_t *partition;
	
	partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");

	if(partition != NULL)
	{
		esp_partition_read(partition, 0, &m_aliyun, sizeof(qs_settings_aliyun_t));
		memcpy(device_info->id, m_aliyun.device_name, strlen(m_aliyun.device_name));
		//aliyun 参数配置
		memcpy(device_info->aliyun.device_id, m_aliyun.device_name, strlen(m_aliyun.device_name));
		memcpy(device_info->aliyun.product_key, m_aliyun.product_key, strlen(m_aliyun.product_key));
		memcpy(device_info->aliyun.device_secret, m_aliyun.device_secret, strlen(m_aliyun.device_secret));

		printf("Device Name: %s\n", m_aliyun.device_name);
		printf("Product Key: %s\n", m_aliyun.product_key);
		printf("Device Secret: %s\n", m_aliyun.device_secret);
		printf("s11\n");
	}else
	{
		//设备id设置
		memcpy(device_info->id, DEVEICE_ID, strlen(DEVEICE_ID));
		
		//aliyun 参数配置
		memcpy(device_info->aliyun.product_key, PRODUCT_KEY, strlen(PRODUCT_KEY));
		memcpy(device_info->aliyun.device_secret, DEVEICE_SECRET, strlen(DEVEICE_SECRET));
		memcpy(device_info->aliyun.device_id, DEVEICE_ID, strlen(DEVEICE_ID));	
		printf("s22\n");	
	}
	printf("id:%s,key:%s,secret:%s \n",device_info->aliyun.device_id,device_info->aliyun.product_key,device_info->aliyun.device_secret);
}

//设置默认参数
void param_config_init(void)
{
	//参数初始化
	device_info = (device_info_t *)malloc(sizeof(device_info_t));
	memset(device_info, 0, sizeof(device_info_t));

	device_info->ble = (ble_link_info_t *)malloc(sizeof(ble_link_info_t));
	memset(device_info->ble, 0, sizeof(ble_link_info_t));
	device_info->ble->xQueue = xQueueCreate(4, 512);   //缩减10 512
	if (device_info->ble->xQueue == NULL)
		ESP_LOGW(TAG,"ble.xQueue create error!");

	device_info->mqtt_key = (mqtt_key_info_t *)malloc(sizeof(mqtt_key_info_t));
	memset(device_info->mqtt_key, 0, sizeof(mqtt_key_info_t));
	device_info->mqtt_key->xQueue = xQueueCreate(10, 64);   //缩减10 512
	if (device_info->mqtt_key->xQueue == NULL)
		ESP_LOGW(TAG,"mqtt_key.xQueue create error!");

	device_info->aliyun.xQueue = xQueueCreate(10, sizeof(mmqtt_msg_t));
	if (device_info->aliyun.xQueue == NULL)
		ESP_LOGW(TAG,"aliyun.xQueue create error!");

	device_info->wifi.one_key_config.xQueue = xQueueCreate(10, 10);
	if (device_info->wifi.one_key_config.xQueue == NULL)
		ESP_LOGW(TAG,"one_key_config.xQueue create error!");

	// 打鼾干预初始化为
	device_info->snore = (snore_intervention_t *) malloc(sizeof(snore_intervention_t));
	memset(device_info->snore, 0, sizeof(snore_intervention_t));		
	device_info->snore->snore_parameters.up_hold_time_s = UP_HOLD_TIME_S;
	device_info->snore->snore_parameters.threshold_5s = THRESHOLD_5S;
	device_info->snore->snore_parameters.threshold = THRESHOLD;
	device_info->snore->snore_parameters.pwm = SNORING_PWM;
	device_info->snore->snore_parameters.tmr = SNORING_TMR;
	device_info->snore->snore_state.block_size = 24;
	//固件版本设置
	memcpy(device_info->ota.running_version, INIT_VERSION, strlen(INIT_VERSION));

#if ALIYUN_BURN
	read_aliyun_config();
#else
	//设备id设置
	memcpy(device_info->id, DEVEICE_ID, strlen(DEVEICE_ID));
	
	//aliyun 参数配置
	memcpy(device_info->aliyun.product_key, PRODUCT_KEY, strlen(PRODUCT_KEY));
	memcpy(device_info->aliyun.device_secret, DEVEICE_SECRET, strlen(DEVEICE_SECRET));
	memcpy(device_info->aliyun.device_id, DEVEICE_ID, strlen(DEVEICE_ID));
#endif

	//数据上传开关设置
	device_info->data_up_switch = DATA_UP;
	device_info->ota.flag = OTA_ON_OFF;


	//默认wifi设置
	memcpy(device_info->wifi.one_key_config.ssid, MY_WIFI_SSID, strlen(MY_WIFI_SSID));
	memcpy(device_info->wifi.one_key_config.passwd, MY_WIFI_PASSWD, strlen(MY_WIFI_PASSWD));

	//5s数据
	user_5s_sensor_info = (qs_pb_msg_sensor_5sec_info *)malloc(sizeof(qs_pb_msg_sensor_5sec_info));
    memset(user_5s_sensor_info, 0, sizeof(qs_pb_msg_sensor_5sec_info));

	//60s数据
	user_60s_sensor_info = (qs_pb_msg_sensor_1min_info *)malloc(sizeof(qs_pb_msg_sensor_1min_info));
    memset(user_60s_sensor_info, 0, sizeof(qs_pb_msg_sensor_1min_info));

	//SA数据
	user_sa_sensor_info = (qs_pb_msg_sleep_apnea_info *)malloc(sizeof(qs_pb_msg_sleep_apnea_info));
	memset(user_sa_sensor_info, 0, sizeof(qs_pb_msg_sleep_apnea_info));
}

void config_store_to_flash(void)
{
	char out_value[300] = {0};
	size_t len;
	
	/*=======================================配置出厂设备信息============================*/
	nvs_handle nvs_config_handler;
	
	ESP_ERROR_CHECK(nvs_open("config_cfg", NVS_READWRITE, &nvs_config_handler));
	//nvs_erase_all(nvs_config_handler);
	
	nvs_get_str(nvs_config_handler, "configFlag", out_value, &len);
	
	// nvs_get_str(nvs_config_handler, "id", out_value, &len);
	// printf("id = %s \n",out_value);
	// if(strstr(out_value, DEVEICE_ID))
	if(strstr(out_value, "config_1bc"))    //esp32 flash中已经存在相应参数，读取参数
	{
		ESP_LOGI(TAG, "read nvs config now... \n");
		len = sizeof(out_value);
		ESP_ERROR_CHECK(nvs_get_str(nvs_config_handler, "wifiSsid", out_value, &len));
		memcpy(device_info->wifi.one_key_config.ssid, out_value, len);
		len = sizeof(out_value);
		ESP_ERROR_CHECK(nvs_get_str(nvs_config_handler, "wifiPasswd", out_value, &len));
		memcpy(device_info->wifi.one_key_config.passwd, out_value, len);

		size_t snore_param_size = sizeof(snore_parameters_t);
        esp_err_t err = nvs_get_blob(nvs_config_handler, "snore_param",  &device_info->snore->snore_parameters, &snore_param_size);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "no snore_param in flash, using default values");
        }
        else
        {
            ESP_LOGI(TAG, "snore param loaded from flash");
        }
		// memcpy(device_info->wifi.one_key_config.ssid, "kakakaka", 9);
		// memcpy(device_info->wifi.one_key_config.passwd, "77885522", 9);

		// 读取成功后同步到last_saved变量
		strncpy(last_saved_ssid, device_info->wifi.one_key_config.ssid, sizeof(last_saved_ssid));
		strncpy(last_saved_passwd, device_info->wifi.one_key_config.passwd, sizeof(last_saved_passwd));
        printf("In common last_saved_ssid = %s\r\n",last_saved_ssid);
		ESP_LOGI(TAG, "ssid= %s, passwd = %s", device_info->wifi.one_key_config.ssid, device_info->wifi.one_key_config.passwd); 
//阿里云三元素每次从.bin文件读取，不再从flash读取
// #if ALIYUN_BURN		
// 		len = sizeof(out_value);
// 		ESP_ERROR_CHECK(nvs_get_str(nvs_config_handler, "id", out_value, &len));
// 		memcpy(device_info->id, out_value, len);
// 		memcpy(device_info->aliyun.device_id, device_info->id, strlen(device_info->id));

// 		len = sizeof(out_value);
// 		ESP_ERROR_CHECK(nvs_get_str(nvs_config_handler, "iotKey", out_value, &len));
// 		memcpy(device_info->aliyun.product_key, out_value, len);
// 		len = sizeof(out_value);
// 		ESP_ERROR_CHECK(nvs_get_str(nvs_config_handler, "iotSecret", out_value, &len));
// 		memcpy(device_info->aliyun.device_secret, out_value, len);
// #endif

		ESP_ERROR_CHECK(nvs_get_u8(nvs_config_handler, "dataUpSwitch", &device_info->data_up_switch));

		len = sizeof(out_value);
		ESP_ERROR_CHECK(nvs_get_u8(nvs_config_handler, "otaFlag", &device_info->ota.flag));
		ESP_ERROR_CHECK(nvs_get_str(nvs_config_handler, "version", out_value, &len));
		memcpy(device_info->ota.running_version, out_value, len);
		len = sizeof(out_value);

		err = nvs_get_str(nvs_config_handler, "report", out_value, &len);
		if (nvs_config_handler == NULL) {ESP_LOGE("NVS", "NVS handler is not initialized!");}	
		if (err != ESP_OK) {ESP_LOGE("NVS", "Error getting 'report' string: %s", esp_err_to_name(err));} 
		else {ESP_LOGI("NVS", "String length: %d, Retrieved string: %s", len, out_value);}
		//ESP_ERROR_CHECK(nvs_get_str(nvs_config_handler, "report", out_value, &len));
	
		memcpy(device_info->report, out_value, len);
		//report test
		// strcpy(device_info->report,"29 2023-12-31 13:13:51 559");

		ESP_LOGI(TAG, "report= %s", device_info->report); 
		printf("snore: up_hold_time_s = %d, threshold_5s = %d, threshold = %d, pwm = %d, tmr = %d\n",
																									device_info->snore->snore_parameters.up_hold_time_s,
																									device_info->snore->snore_parameters.threshold_5s,
																									device_info->snore->snore_parameters.threshold,
																									device_info->snore->snore_parameters.pwm,
																									device_info->snore->snore_parameters.tmr);

		ESP_LOGI(TAG, "read nvs config ok... \n");
	}
	else
	{
		ESP_LOGI(TAG, "nvs config init now... \n");    //esp32 flash中不存在相应参数，将默认参数存入
		ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "wifiSsid", device_info->wifi.one_key_config.ssid));
		ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "wifiPasswd", device_info->wifi.one_key_config.passwd));


		ESP_LOGI(TAG, "write default snore param to flash...");
        ESP_ERROR_CHECK(nvs_set_blob(nvs_config_handler, "snore_param",  &device_info->snore->snore_parameters, sizeof(snore_parameters_t)));
		printf("snore: up_hold_time_s = %d, threshold_5s = %d, threshold = %d, pwm = %d, tmr = %d\n",
																									device_info->snore->snore_parameters.up_hold_time_s,
																									device_info->snore->snore_parameters.threshold_5s,
																									device_info->snore->snore_parameters.threshold,
																									device_info->snore->snore_parameters.pwm,
																									device_info->snore->snore_parameters.tmr);
//阿里云三元素不再转存到flash
		// ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "id", device_info->id));
		// ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "iotKey", device_info->aliyun.product_key));
		// ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "iotSecret", device_info->aliyun.device_secret));

		ESP_ERROR_CHECK(nvs_set_u8(nvs_config_handler, "dataUpSwitch", device_info->data_up_switch));

		ESP_ERROR_CHECK(nvs_set_u8(nvs_config_handler, "otaFlag", device_info->ota.flag));
    	ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "version", device_info->ota.running_version));

		ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "report", "NONE"));
		memcpy(device_info->report, "NONE", 5);
		ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "configFlag", "config_1bc"));
		printf("nvs config update ok. \n");
	}
	ESP_ERROR_CHECK(nvs_commit(nvs_config_handler));
	print_nvs_usage();
	nvs_close(nvs_config_handler);

	strncpy(last_saved_ssid, device_info->wifi.one_key_config.ssid, sizeof(last_saved_ssid));
	strncpy(last_saved_passwd, device_info->wifi.one_key_config.passwd, sizeof(last_saved_passwd));
	ESP_LOGI(TAG, "ssid= %s, passwd = %s", device_info->wifi.one_key_config.ssid, device_info->wifi.one_key_config.passwd);
	ESP_LOGI(TAG, "otaFlag= %d, version = %s", device_info->ota.flag, device_info->ota.running_version);
}

static void led_init(void)
{
	esp_rom_gpio_pad_select_gpio(LED_BLUE);    
	gpio_set_direction(LED_BLUE, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_BLUE, 1);

	esp_rom_gpio_pad_select_gpio(LED_GREEN);    
	gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_GREEN, 1);

	esp_rom_gpio_pad_select_gpio(LED_RED);    
	gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_RED, 0);
}

void device_init(void)
{
	param_config_init();
	config_store_to_flash();
	led_init();
	print_nvs_usage();
	//set_one_key_config_wifi_status(1);
}
