/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2021-10-07 23:03:40
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-06-27 14:05:33
 * @FilePath: /smart-air-bed-board-program/components/use_wifi/use_wifi.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "use_wifi.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "common.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include <lwip/netdb.h>
#include "aiot_mqtt_sign.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include <errno.h>
#include "freertos/semphr.h"
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_attr.h"
#include "use_ota.h"
#include "app_control.h"
#include "esp_task_wdt.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MAXIMUM_RETRY_WIFI (5)
#define MAXIMUM_RETRY_SNTP (60)

#define TUYA_MQTT_BROKER_PORT 1883
/**
 * aiotMqttSign 最后一参：与 clientId 里 securemode 一致。
 * 1 = securemode=2（与你提供的控制台 JSON 一致）；0 = securemode=3（文档「TCP 直连」常用）。
 */
#define TUYA_MQTT_SIGN_SECUREMODE_TLS_STYLE 1

// topic

char ota_infor_publish_topic[64] = {0};     // 缩减为64
char user_5s_data_publish_topic[64] = {0};
char user_60s_data_publish_topic[64] = {0};
char user_sa_data_publish_topic[64] = {0};
char user_sleep_data_publish_topic[64] = {0};
char user_cli_data_subscribe_topic[64] = {0};
char user_cli_data_publish_topic[64] = {0};
char ota_upgrade_subscribe_topic[64] = {0};
char mqtt_connect_aliyun_url[64] = {0};
/** mqtts://host:端口（文档：8883 自签 CA / 1883+TLS 等与控制台一致），生命周期需覆盖 MQTT 客户端存续期 */
static char s_mqtt_uri[128];
char mc_cli_data_subscribe_topic[64] = {0};
char mc_cli_data_publish_topic[64] = {0};
static const char s_mqtt_ca_cert[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIEBTCCAu2gAwIBAgIUOiMDlRFu6/Snmcl3ajKz0WcXM5swDQYJKoZIhvcNAQEL\n"
"BQAwgZAxCzAJBgNVBAYTAkNOMREwDwYDVQQIDAhaaGVKaWFuZzEQMA4GA1UEBwwH\n"
"SmlhWGluZzEPMA0GA1UECgwGS2Vlc29uMQwwCgYDVQQLDANEJlIxGTAXBgNVBAMM\n"
"EGlvdC5zbWFydGJlZC5pbmsxIjAgBgkqhkiG9w0BCQEWE3NxbC5zb25nQGtlZXNv\n"
"bi5jb20wIBcNMjYwNjA4MDAyMzE3WhgPMjEyNjA1MTUwMDIzMTdaMIGQMQswCQYD\n"
"VQQGEwJDTjERMA8GA1UECAwIWmhlSmlhbmcxEDAOBgNVBAcMB0ppYVhpbmcxDzAN\n"
"BgNVBAoMBktlZXNvbjEMMAoGA1UECwwDRCZSMRkwFwYDVQQDDBBpb3Quc21hcnRi\n"
"ZWQuaW5rMSIwIAYJKoZIhvcNAQkBFhNzcWwuc29uZ0BrZWVzb24uY29tMIIBIjAN\n"
"BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAicRN0yZQuG+m6DgivDWDa3O02xJf\n"
"oqE+ho0V1O8lrWahS42lrRKbLWQRyKaXfNseNQklJg+J2AueDt4oAfzAhQg3Pn1/\n"
"SufwwtujTLPpfXNqgBbm6tF9DlVL+ML66FdOFXcUCT3uRD599pPIP99u4dS02OsV\n"
"7O5drbn6GQTFWm26gjwLN12fOf0BgsjnQ1ugQ44YJR7kzIRny47kl3zzoLj2WMm2\n"
"0Mf3R0OuKXZ/WuUcxZSMwNMLMeToFIjoKFbB24w5vBv2YkduB/F6w2QzPFK9zAf4\n"
"wf4zn+UDbIHkD4WVbLwUG/d4xEECjpAYCP+qCB1DL8tbiZxjDF9svGr7dQIDAQAB\n"
"o1MwUTAdBgNVHQ4EFgQU8wf5AZDvjUrJIHsK9eZTJpu79bIwHwYDVR0jBBgwFoAU\n"
"8wf5AZDvjUrJIHsK9eZTJpu79bIwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0B\n"
"AQsFAAOCAQEAh33JCkUzAyiUMLhOKWQmhkso5T/n1WsShTL5saNI5E2+k5VvOUqU\n"
"o3s+IWc5Jg7biH05GyvXPk+laAkgP1ImsnTijNznzJRxJqGVOYSNViyfB1VxcdwS\n"
"0WH/qSk/ZZxPNcS1rlRloLVmtEwoQo/EHN0hndHE3qNnmQanokHSfuRRsTltjObf\n"
"+kaU/ewFoAaRMe80hfNSqSBylR+euetT31YQ7dZZSf5qecOFEOPHauNOpsHGa7d4\n"
"Wv+4RO+c5jrfCXFMUdzPqUJpTuLCFWL9wH6xnBr4Aletklik+AJz56cTzB4jWDHa\n"
"CxXCO4hhMTdGsPU3m5KzWO+bq0mo1Gr+BQ==\n"
"-----END CERTIFICATE-----\n";

extern char last_saved_ssid[32];
extern char last_saved_passwd[64];

static const char *TAG = "wifi station";
int s_retry_num = 0;
static int s_mqtt_retry_num = 0;
static uint8_t s_sntp_started = 0;
static uint8_t s_mqtt_started_after_sntp = 0;
static int s_mqtt_sub_cli_msg_id = -1;
static EventGroupHandle_t s_wifi_event_group;
extern device_info_t *device_info;
char *mqtt_json_send;
char mqtt_device_send_time[] = "1412574889";
cJSON *pJsonRoot;
cJSON *pCmdAdress;
esp_mqtt_client_handle_t client;
uint8_t wifi_link_event_id;

wifi_config_t wifi_config = {
    .sta = {
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        .threshold.rssi = -80,                      // 信号强度阈值：-80db
        .scan_method = WIFI_ALL_CHANNEL_SCAN,       // 强制全信道扫描
        .pmf_cfg = {
            .capable = true,
            .required = false},
    },
};

/* 获 IP 后立即 SNTP（IP 直连，避免 DNS 延迟）；同步成功回调里启动 MQTT */
static void on_sntp_synced(struct timeval *tv)
{
    (void)tv;
    if (device_info != NULL) {
        device_info->utc.flag = true;
    }
    if (!s_mqtt_started_after_sntp) {
        s_mqtt_started_after_sntp = 1;
        use_wifi_mqtt_init_and_start_after_time_sync();
    }
    ESP_LOGI(TAG, "SNTP sync ok, MQTT init triggered");
}

static void start_sntp_once(void)
{
    if (s_sntp_started) {
        return;
    }
    s_sntp_started = 1;

    sntp_set_time_sync_notification_cb(on_sntp_synced);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    ip_addr_t ntp_addr;
    ipaddr_aton("203.107.6.88", &ntp_addr); /* ntp.aliyun.com */
    sntp_setserver(0, &ntp_addr);

    ip_addr_t ntp_addr2;
    ipaddr_aton("114.118.7.163", &ntp_addr2); /* ntp1.aliyun.com 备用 */
    sntp_setserver(1, &ntp_addr2);

    setenv("TZ", "CST-8", 1);
    tzset();
    sntp_init();
    ESP_LOGI(TAG, "SNTP started on GOT_IP (IP direct)");
}

/* 解析 OTA upgrade 下行并启动 HTTPS 下载（兼容 data / params 字段） */
static void mqtt_handle_ota_upgrade_payload(const char *payload)
{
    cJSON *root = NULL;
    cJSON *data_obj = NULL;
    cJSON *ver_item = NULL;
    cJSON *url_item = NULL;

    if (payload == NULL || payload[0] == '\0') {
        ESP_LOGE(TAG, "OTA upgrade payload empty");
        return;
    }

    root = cJSON_Parse(payload);
    if (root == NULL) {
        ESP_LOGE(TAG, "OTA upgrade JSON parse failed");
        return;
    }

    data_obj = cJSON_GetObjectItem(root, "data");
    if (data_obj == NULL) {
        data_obj = cJSON_GetObjectItem(root, "params");
    }
    if (data_obj == NULL || !cJSON_IsObject(data_obj)) {
        ESP_LOGE(TAG, "OTA upgrade JSON missing data/params");
        cJSON_Delete(root);
        return;
    }

    ver_item = cJSON_GetObjectItem(data_obj, "version");
    url_item = cJSON_GetObjectItem(data_obj, "url");
    if (ver_item == NULL || !cJSON_IsString(ver_item) || ver_item->valuestring == NULL) {
        ESP_LOGE(TAG, "OTA upgrade JSON missing version");
        cJSON_Delete(root);
        return;
    }
    if (url_item == NULL || !cJSON_IsString(url_item) || url_item->valuestring == NULL) {
        ESP_LOGE(TAG, "OTA upgrade JSON missing url");
        cJSON_Delete(root);
        return;
    }

    memset(device_info->ota.upgrade_version, 0, sizeof(device_info->ota.upgrade_version));
    strncpy(device_info->ota.upgrade_version, ver_item->valuestring,
            sizeof(device_info->ota.upgrade_version) - 1);
    memset(device_info->ota.url, 0, sizeof(device_info->ota.url));
    strncpy(device_info->ota.url, url_item->valuestring, sizeof(device_info->ota.url) - 1);

    ESP_LOGI(TAG, "OTA upgrade push: %s -> %s",device_info->ota.running_version, device_info->ota.upgrade_version);
    cJSON_Delete(root);
    ota_start();
}

//wifi操作函数
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    static uint32_t last_disconnect_time = 0;
    //开始连接wifi
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    //wifi断开
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "WiFi disconnected, reason: %d", ((wifi_event_sta_disconnected_t *)event_data)->reason);
        ESP_LOGI(TAG, "Connecting to SSID: %s", (const char *)wifi_config.sta.ssid);
        ESP_LOGI(TAG, "With Password: %s", (const char *)wifi_config.sta.password);

        esp_wifi_connect();
        set_wifi_status(WIFI_STATUS_RECONNECTING);
        if (++s_retry_num > 120) {
            ESP_LOGE(TAG, "WiFi reconnect timeout, rebooting...");
            esp_restart();
        }
        ESP_LOGI(TAG, "retry to connect to the AP,num %d",s_retry_num);
    }

    // 新增：丢失 IP 地址，IP 地址重置为 0 则断开
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP)
    {
        ESP_LOGW(TAG, "Lost IP address. Trying to reconnect...");
        esp_wifi_disconnect();
    }
    //wifi连接/获取ip地址成功
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        ESP_LOGI(TAG, "connected");
        set_wifi_status(WIFI_STATUS_CONNECTED);
        start_sntp_once();
        /* 首次 MQTT 在 SNTP 回调 on_sntp_synced 中创建并 start；
         * WiFi 重连后 client 已存在，此处恢复连接。
         * OTA 下载期间 MQTT 已 stop，禁止此处 start，避免双 TLS。 */
        if (client != NULL && get_ota_now_flag() == 0) {
            esp_err_t merr = esp_mqtt_client_start(client);
            if (merr != ESP_OK && merr != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(TAG, "esp_mqtt_client_start on GOT_IP: %s", esp_err_to_name(merr));
            }
        }
        if (get_one_key_config_wifi_status() ||
            strcmp((char *)last_saved_ssid, (char *)device_info->wifi.one_key_config.ssid) != 0 ||
            strcmp((char *)last_saved_passwd, (char *)device_info->wifi.one_key_config.passwd) != 0)
        {
            wifi_link_event_id = wifi_ok;
            BaseType_t xStatus = xQueueSend(device_info->wifi.one_key_config.xQueue, &wifi_link_event_id, 0);
            if (xStatus != pdPASS)
            {
                printf("Could not send to the queue.\r\n");
            }

            nvs_handle nvs_config_handler;
            ESP_ERROR_CHECK(nvs_open("config_cfg", NVS_READWRITE, &nvs_config_handler));
            ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "wifiSsid", device_info->wifi.one_key_config.ssid));
		    ESP_ERROR_CHECK(nvs_set_str(nvs_config_handler, "wifiPasswd", device_info->wifi.one_key_config.passwd));
            ESP_ERROR_CHECK(nvs_commit(nvs_config_handler));
	        nvs_close(nvs_config_handler);
            strncpy(last_saved_ssid, (char *)device_info->wifi.one_key_config.ssid, sizeof(last_saved_ssid));
            strncpy(last_saved_passwd, (char *)device_info->wifi.one_key_config.passwd, sizeof(last_saved_passwd));
            printf("last_saved_ssid = %s\r\n",last_saved_ssid);
        }
    }
}


//mqtt操作函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    // printf("Event dispatched from event loop base=%s, event_id=%d \n", base, event_id);
    //  获取MQTT客户端结构体指针
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    mmqtt_msg_t *msg = NULL;
    cJSON *firstItem;
    cJSON *secondItem;
    cJSON *thirdItem;
    char temp[2048] = {0};
    uint8_t cmd_bin[64] = {0}; // 存放MQTT_key数据
    // 通过事件ID来分别处理对应的事件
    switch (event->event_id)
    {
    // 建立连接成功
    case MQTT_EVENT_CONNECTED:
        printf("MQTT_client cnnnect ok. \n");
        s_mqtt_retry_num = 0;
        if(get_one_key_config_wifi_status())
        {
           
            wifi_link_event_id = mqtt_ok;
            BaseType_t xStatus = xQueueSend(device_info->wifi.one_key_config.xQueue, &wifi_link_event_id, 0);
            if (xStatus != pdPASS)
            {
                printf("Could not send to the queue.\r\n");
            }
            set_one_key_config_wifi_status(0);  
        }
        set_mqtt_status(1);
        sprintf(user_5s_data_publish_topic, "/%s/%s/user/5s/put", device_info->aliyun.product_key, device_info->aliyun.device_id);
        sprintf(user_60s_data_publish_topic, "/%s/%s/user/60s/put", device_info->aliyun.product_key, device_info->aliyun.device_id);
        sprintf(user_sa_data_publish_topic, "/%s/%s/user/sa/put", device_info->aliyun.product_key, device_info->aliyun.device_id);
        sprintf(user_sleep_data_publish_topic, "/%s/%s/user/sleep/put", device_info->aliyun.product_key, device_info->aliyun.device_id);
        
        sprintf(user_cli_data_subscribe_topic, "/%s/%s/user/cli/get", device_info->aliyun.product_key, device_info->aliyun.device_id); 
        sprintf(user_cli_data_publish_topic, "/%s/%s/user/cli/put", device_info->aliyun.product_key, device_info->aliyun.device_id);
        
        sprintf(ota_upgrade_subscribe_topic,  "/ota/device/upgrade/%s/%s",device_info->aliyun.product_key, device_info->aliyun.device_id);
        sprintf(ota_infor_publish_topic,  "/ota/device/inform/%s/%s",device_info->aliyun.product_key, device_info->aliyun.device_id);

        sprintf(mc_cli_data_subscribe_topic, "/%s/%s/user/cli/get", device_info->aliyun.product_key, device_info->aliyun.device_id); // mcli 合并到cil中了  
        sprintf(mc_cli_data_publish_topic, "/%s/%s/user/mccli/put", device_info->aliyun.product_key, device_info->aliyun.device_id);
        printf("%s\n", user_5s_data_publish_topic);
        printf("%s\n", user_60s_data_publish_topic);
        printf("%s\n", user_sa_data_publish_topic);
        printf("%s\n", user_sleep_data_publish_topic);
        printf("%s\n", ota_infor_publish_topic);
        printf("%s\n", ota_upgrade_subscribe_topic);
        printf("%s\n", mc_cli_data_subscribe_topic);
        printf("%s\n", mc_cli_data_publish_topic);
        printf("%s\n", user_cli_data_subscribe_topic);
        s_mqtt_sub_cli_msg_id = esp_mqtt_client_subscribe(client, user_cli_data_subscribe_topic, 0);
        esp_mqtt_client_subscribe(client, ota_upgrade_subscribe_topic, 0);
        //esp_mqtt_client_subscribe(client, mc_cli_data_subscribe_topic, 0);
        /* 每次 MQTT 连上都上报 OTA 版本，便于云端匹配升级任务并主动推送 upgrade */
        sprintf(temp, "{\"id\": \"1\",\"params\": {\"version\": \"%s\",\"module\":\"default\"}}",
                device_info->ota.running_version);
        ESP_LOGI(TAG, "OTA inform publish: version=%s", device_info->ota.running_version);
        esp_mqtt_client_publish(client, ota_infor_publish_topic,
                (char *)temp, strlen((char *)temp), 1, 0);
        printf("MQTT_client1");
        break;
    // 客户端断开连接 10s自动尝试重连
    case MQTT_EVENT_DISCONNECTED:
        printf("MQTT_client have disconnected. s_mqtt_retry_num= %d\n",s_mqtt_retry_num);
        set_mqtt_status(0);
        if(get_one_key_config_wifi_status())
        {
            wifi_link_event_id = mqtt_fail;
            BaseType_t xStatus = xQueueSend(device_info->wifi.one_key_config.xQueue, &wifi_link_event_id, 0);
            if (xStatus != pdPASS)
            {
                printf("Could not send to the queue.\r\n");
            }
            // set_one_key_config_wifi_status(0); // 不需要 1. MQTT 断开不能说明 WiFi 参数是有效的,设置为0代表不用保存wifi密码导致
        }
        s_mqtt_retry_num++;
        if (s_mqtt_retry_num > 60) {  // 每次断开 +1，超出120次重启（每10s触发一次，相当于10分钟）
            ESP_LOGE(TAG, "MQTT reconnect timeout, rebooting...");
            esp_restart();
        }
        //断开wifi，重新连接
        // if (get_wifi_status())
        // {
        //     ESP_ERROR_CHECK(esp_wifi_disconnect());
        // }
        // set_wifi_status(0);
        // ESP_ERROR_CHECK(esp_wifi_stop());
        // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        // s_retry_num = 0;
        // ESP_ERROR_CHECK(esp_wifi_start());
        printf("MQTT_client2");
        break;
    // 主题订阅成功
    case MQTT_EVENT_SUBSCRIBED:
        if (event->msg_id == s_mqtt_sub_cli_msg_id) {
            ESP_LOGI(TAG, "cli subscribe ok msg_id=%d", event->msg_id);
        }
        break;
    // 取消订阅
    case MQTT_EVENT_UNSUBSCRIBED:
        // printf("mqtt unsubscribe ok. msg_id = %d \n",event->msg_id);
        // printf("MQTT_client4");
        break;
    //  主题发布成功
    case MQTT_EVENT_PUBLISHED:
        break;
    // 已收到订阅的主题消息
    case MQTT_EVENT_DATA:
    /*
{
"id":"KSPSBED00000142",
"ts":123456789,
"type":"sensorCli",
"cmd":"version"  //"report 29 2023-12-16 01:52:30 014"   "get param"
}
{
"id":"KSPSBED00000142",
"ts":123456789,
"type":"deviceCli",
"cmd":"currentReport"
}
    */
        printf("mqtt received topic: %.*s \n", event->topic_len, event->topic);
        if (event->topic_len <= 0 || event->data_len <= 0) {
            break;
        }
        if ((size_t)event->topic_len >= sizeof(((mmqtt_msg_t *)0)->topic) ||
            (size_t)event->data_len >= sizeof(((mmqtt_msg_t *)0)->data)) {
            ESP_LOGW(TAG, "MQTT msg too large, topic_len=%d data_len=%d",
                     event->topic_len, event->data_len);
            break;
        }
        if (event->total_data_len > 0 && event->data_len != event->total_data_len) {
            ESP_LOGW(TAG, "MQTT fragmented msg ignored, total=%d chunk=%d",
                     event->total_data_len, event->data_len);
            break;
        }
        msg = (mmqtt_msg_t *)malloc(sizeof(mmqtt_msg_t));
        if (msg == NULL) {
            printf("Error: Memory allocation failed for mmqtt_msg_t!\n");
            break;
        }
        memset(msg, 0, sizeof(mmqtt_msg_t));
        memcpy(msg->topic, event->topic, (size_t)event->topic_len);
        memcpy(msg->data, event->data, (size_t)event->data_len);
        msg->topic[event->topic_len] = '\0';
        msg->data[event->data_len] = '\0';
        msg->topic_len = (uint32_t)event->topic_len;
        msg->data_len = (uint32_t)event->data_len;

        if (msg&&strstr(msg->topic, user_cli_data_subscribe_topic))
        {
            firstItem = cJSON_Parse((char *)msg->data);
            printf("%s\n", msg->data);
            if (firstItem)
            {
                secondItem = cJSON_GetObjectItem(firstItem, "id");
                if (secondItem && strstr(secondItem->valuestring, device_info->id))
                {
                    secondItem = cJSON_GetObjectItem(firstItem, "type");

                    if(secondItem && strstr(secondItem->valuestring, "sensorCli"))
                    {
                        if(get_devic_id_flag() == 0)
                        {
                            sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"no sensor id,wait two min again\"}", 
                                device_info->id,
                                device_info->utc.time_stamp,
                                secondItem->valuestring);
                            esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);      
                        }
                        else
                        {
                            secondItem = cJSON_GetObjectItem(firstItem, "cmd");   
                            if(secondItem && strstr(secondItem->valuestring, "report "))
                            {
                                if(get_sleep_up_flag() == 1)
                                {
                                    sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"report uping now,wait two min again\"}", 
                                    device_info->id,
                                    device_info->utc.time_stamp,
                                    secondItem->valuestring);
                                    esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);

                                }
                                else{

                                    //发送报告名test
                                    sprintf(temp, "{\"id\":\"%s\",\"ts\":%d,\"type\":4,\"report\":\"%s\",\"data\":\"%s\"}",
                                                                                        device_info->id,
                                                                                        device_info->utc.time_stamp,
                                                                                        &secondItem->valuestring[7],
                                                                                        &secondItem->valuestring[7]);
                                    printf("%s\n",temp);
                                    esp_mqtt_client_publish(client, user_sleep_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);     

                                    
                                    uint8_t report_num =  atoi(&secondItem->valuestring[7]);
                                    printf("report_num %d\n",report_num);
                                    set_sleep_up_flag(1);
                                    set_cli_report_name(&secondItem->valuestring[7],strlen((char *)&secondItem->valuestring[7]));                                   
                                    set_report_cli(2,report_num);
                                }

                            }
                            else if (secondItem&&strstr(secondItem->valuestring, "reboot"))
                            {
                                char return_value[1024] = {0};
                                set_bc(device_info->utc.time_stamp, secondItem->valuestring, 1, 0, return_value, 1000);
                                sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"%s\"}", 
                                    device_info->id,
                                    device_info->utc.time_stamp,
                                    secondItem->valuestring,
                                    return_value);
                                printf("%s\n",temp);
                                esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
                                if(strncmp(return_value, "ok",2) == 0)
                                {
                                    sensor_reboot_config();
                                }
                            }
                            else if (secondItem&&strstr(secondItem->valuestring, "set mode 0"))     // xinzeng：如果是set mode 0强制生成报告，则将报告上传到阿里云
                            {
                                char return_value[1024] = {0};
                                set_bc(device_info->utc.time_stamp, secondItem->valuestring, 1, 0, return_value, 1000);
                                sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"%s\"}", 
                                    device_info->id,
                                    device_info->utc.time_stamp,
                                    secondItem->valuestring,
                                    return_value);
                                printf("%s\n",temp);
                                esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
                                // printf("什么意思？\n");
                               if(strncmp(return_value, "ok",2) == 0)
                                {
                                    vTaskDelay(2000 / portTICK_PERIOD_MS);      // 等待2S 报告生成
                                    set_mode_flag_config(0);
                                }                            
                            }
                            else
                            {
                                char return_value[1024] = {0};
                                int ddss;
                                int64_t start_time = esp_timer_get_time(); 
                                set_bc(device_info->utc.time_stamp, secondItem->valuestring, 1, 0, return_value, 1000);
                                int64_t end_time = esp_timer_get_time();
                                printf("set bc time = %lld ms\n", (end_time - start_time)/1000);
                                // printf("%s\n",return_value);
                                sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"%s\"}", 
                                    device_info->id,
                                    device_info->utc.time_stamp,
                                    secondItem->valuestring,
                                    return_value);
                                printf("%s\n",temp);
                                ddss=esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
                                printf("ddss=%d\n",ddss);
                           }

                        }
                    }
                    else if (secondItem&&strstr(secondItem->valuestring, "deviceCli"))
                    {
                        secondItem = cJSON_GetObjectItem(firstItem, "cmd");
                       if(secondItem && strstr(secondItem->valuestring, "dataUpSwitch"))
                       {
                            device_info->data_up_switch = (secondItem->valuestring[13] == '0')? 0:1;

                            nvs_handle nvs_config_handler;
                            ESP_ERROR_CHECK(nvs_open("config_cfg", NVS_READWRITE, &nvs_config_handler));
                            ESP_ERROR_CHECK(nvs_set_u8(nvs_config_handler, "dataUpSwitch", device_info->data_up_switch));
                            ESP_ERROR_CHECK(nvs_commit(nvs_config_handler));
                            nvs_close(nvs_config_handler);
                            sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":%d}", 
                                device_info->id,
                                device_info->utc.time_stamp,
                                secondItem->valuestring,
                                device_info->data_up_switch);
                            printf("%s\n",temp);
                            esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
                       }
                       else if (secondItem && strstr(secondItem->valuestring, "currentReport"))
                       {
                            sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"%s\"}", 
                                device_info->id,
                                device_info->utc.time_stamp,
                                secondItem->valuestring,
                                device_info->report);
                            printf("%s\n",temp);
                            esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);
                       }
                       
                    }else if(secondItem && strstr(secondItem->valuestring, "mcCli"))
                    {
                        // printf("mqtt mqtt received to: mcCli\n");
                        if(get_devic_id_flag() == 0)
                        {
                            sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"no sensor id,wait two min again\"}", 
                                device_info->id,
                                device_info->utc.time_stamp,
                                secondItem->valuestring);
                            esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);      
                        }else
                        {
                            secondItem = cJSON_GetObjectItem(firstItem, "cmd");
                            // printf("I mqtt mc cil:secondItem = %s\n",secondItem->valuestring);
                            if (secondItem) 
                            {
                                const char *cmd_str = secondItem->valuestring;  
                                int cmd_len = 0;
                                // 每2个字符转换为一个字节
                                while (*cmd_str && *(cmd_str + 1) && (cmd_len < 64)) {
                                    char byte_str[3] = { cmd_str[0], cmd_str[1], '\0' };
                                    cmd_bin[cmd_len++] = (uint8_t)strtol(byte_str, NULL, 16);
                                    cmd_str += 2;
                                }
                                // 将转换后的数据通过队列发送出去
                                if (cmd_len > 0 && cmd_bin[0] == 0xAA) 
                                {
                                    // for(int i=0;i<cmd_len;i++){printf("%02X ",cmd_bin[i]);}printf("\n\n");
                                    if (xQueueSend(device_info->mqtt_key->xQueue, cmd_bin, 0) != pdPASS) {
                                        printf("Queue send failed.\r\n");
                                    }
                                }else{
                                    printf("mqtt head error\n");
                                }
                            }
                        }
                    }                    
                }
                else{
                    printf("cJSON_Parse id wrong\n");
                }
                cJSON_Delete(firstItem);    
            }
            else{
                printf("cJSON_Parse wrong\n");
            }   
        }   
 /*     else if (msg&&strstr(msg->topic, mc_cli_data_subscribe_topic))
 
        {
            firstItem = cJSON_Parse((char *)msg->data);
            printf("%s\n", msg->data);
            if (firstItem)
            {
                secondItem = cJSON_GetObjectItem(firstItem, "id");
                if (secondItem && strstr(secondItem->valuestring, device_info->id))
                {
                    secondItem = cJSON_GetObjectItem(firstItem, "type");
                    if(secondItem && strstr(secondItem->valuestring, "1"))
                    {
                        if(get_devic_id_flag() == 0)
                        {
                            sprintf(temp,"{\"id\":\"%s\",\"ts\":%d,\"cmd\":%s,\"back\":\"no sensor id,wait two min again\"}", 
                                device_info->id,
                                device_info->utc.time_stamp,
                                secondItem->valuestring);
                            esp_mqtt_client_publish(client, user_cli_data_publish_topic, (char *)temp, strlen((char *)temp), 0, 0);      
                        }else
                        {
                            secondItem = cJSON_GetObjectItem(firstItem, "cmd");
                            printf("I mqtt mc cil:secondItem = %s\n",secondItem->valuestring);
                            if (secondItem) 
                            {
                                const char *cmd_str = secondItem->valuestring;  
                                int cmd_len = 0;
                                // 每2个字符转换为一个字节
                                while (*cmd_str && *(cmd_str + 1) && (cmd_len < 64)) {
                                    char byte_str[3] = { cmd_str[0], cmd_str[1], '\0' };
                                    cmd_bin[cmd_len++] = (uint8_t)strtol(byte_str, NULL, 16);
                                    cmd_str += 2;
                                }
                                // 将转换后的数据通过队列发送出去
                                if (cmd_len > 0 && cmd_bin[0] == 0xAA) 
                                {
                                    for(int i=0;i<cmd_len;i++){printf("%02X ",cmd_bin[i]);}printf("\n\n");

                                    if (xQueueSend(device_info->mqtt_key->xQueue, cmd_bin, 0) != pdPASS) {
                                        printf("Queue send failed.\r\n");
                                    }
                                }else{
                                    printf("mqtt head error\n");
                                }
                            }
                        }
                    }
                  
                }
                else{
                    printf("cJSON_Parse id wrong\n");
                }
                cJSON_Delete(firstItem);    
            }
            else{
                printf("cJSON_Parse wrong\n");
            }   
        }
*/  
        else if (msg && strstr(msg->topic, ota_upgrade_subscribe_topic))
        {
            mqtt_handle_ota_upgrade_payload((char *)msg->data);
        }
        free(msg);
        printf("MQTT_EVENT_DATA \n");
        break;
    // 客户端遇到错误
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        printf("Other event id:%d \n", event->event_id);
        break;
    }
}

void aliyun_mqtt_server(void)
{
    char clientid[150] = {0};
    char username[64] = {0};
    char password[65] = {0};
    //个人阿里云
    // sprintf(mqtt_connect_aliyun_url, "%s.iot-as-mqtt.cn-shanghai.aliyuncs.com", device_info->aliyun.product_key);    //product_host
    //企业阿里云
    // strcpy(mqtt_connect_aliyun_url, "iot-060a3upv.mqtt.iothub.aliyuncs.com");    //iot-060a3upv.mqtt.iothub.aliyuncs.com   192.168.142.63
    strncpy(mqtt_connect_aliyun_url, "iot.smartbed.ink", sizeof(mqtt_connect_aliyun_url) - 1);
    mqtt_connect_aliyun_url[sizeof(mqtt_connect_aliyun_url) - 1] = '\0';
    aiotMqttSign(device_info->aliyun.product_key, device_info->aliyun.device_id, device_info->aliyun.device_secret,
        clientid, username, password, TUYA_MQTT_SIGN_SECUREMODE_TLS_STYLE);
    // aiotMqttSign(device_info->aliyun.product_key, device_info->aliyun.device_id, device_info->aliyun.device_secret, clientid, username, password);

    ESP_LOGI(TAG, "mqtt broker host=%s", mqtt_connect_aliyun_url);
    ESP_LOGI(TAG, "mqtt clientid=%s", clientid);
    ESP_LOGI(TAG, "mqtt username=%s", username);
    
    if (snprintf(s_mqtt_uri, sizeof(s_mqtt_uri), "mqtts://%s:%d", mqtt_connect_aliyun_url, TUYA_MQTT_BROKER_PORT)
        >= (int)sizeof(s_mqtt_uri)) 
    {
        ESP_LOGE(TAG, "mqtt uri 过长");
    }
    ESP_LOGI(TAG, "mqtt uri=%s (TLS，使用 iot.smartbed.ink CA)", s_mqtt_uri);

    // 1、定义一个MQTT客户端配置结构体，输入MQTT的url
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = s_mqtt_uri,
        .client_id = clientid,
        .username = username,
        .password = password,
        .cert_pem = s_mqtt_ca_cert,
        .buffer_size = 1024,
        .task_stack = 1024 * 20,
        .message_retransmit_timeout = 25000,
        /* 涂鸦文档建议 keepalive 取 300s 以上（范围 30–1200），减轻平台空闲超时主动断连 */
        .keepalive = 300,
        };

    // 2、通过esp_mqtt_client_init获取一个MQTT客户端结构体指针，参数是MQTT客户端配置结构体
    client = esp_mqtt_client_init(&mqtt_cfg);

    // 3、注册MQTT事件
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
}

void use_wifi_mqtt_init_and_start_after_time_sync(void)
{
    /* 涂鸦 aiotMqttSign 使用 gettimeofday 毫秒时间戳；SNTP 前签名会导致 timestamp 错误、连接后很快被踢 */
    if (client != NULL) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }
    aliyun_mqtt_server();
    if (client == NULL) {
        ESP_LOGE(TAG, "MQTT client init failed");
        return;
    }
    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start: %s", esp_err_to_name(err));
    }
}


void initialize_wifi(void)
{

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    memcpy(wifi_config.sta.ssid, (uint8_t *)device_info->wifi.one_key_config.ssid, 32);
    memcpy(wifi_config.sta.password, (uint8_t *)device_info->wifi.one_key_config.passwd, 64);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
#if defined(CONFIG_BT_ENABLED) && CONFIG_BT_ENABLED
    /*
     * WiFi 与蓝牙同时使能时，IDF 要求必须开启 WiFi Modem Sleep，禁止 WIFI_PS_NONE，
     * 否则会打印 “Should enable WiFi modem sleep...” 并在 pm_set_sleep_type 里 abort。
     * MIN_MODEM 为共存场景下常用折中，仍保留较长 keepalive 减轻 MQTT 空闲断连。
     */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_LOGI(TAG, "wifi ps=WIFI_PS_MIN_MODEM（BT 已启用，不可用 PS_NONE）");
#else
    /* 无蓝牙时可关闭 STA 省电，略利于 MQTT 长连接稳定 */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(TAG, "wifi ps=WIFI_PS_NONE（未启用 BT）");
#endif
    ESP_LOGI(TAG, "wifi_init_sta finished.（获 IP 后 SNTP→MQTT，见 start_sntp_once/on_sntp_synced）");
}
