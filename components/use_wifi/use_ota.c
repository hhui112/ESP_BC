/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2021-10-07 23:03:40
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-06-22 00:03:29
 * @FilePath: /smart-air-bed-board-program/components/use_wifi/use_ota.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "use_ota.h"
#include "common.h"
#include "mqtt_client.h"
#include "app_control.h"

static const char *TAG = "ota";
extern device_info_t *device_info;
extern esp_mqtt_client_handle_t client;
extern char ota_infor_publish_topic[64];

static bool s_mqtt_paused_for_ota = false;

/* OTA HTTPS 下载前暂停 MQTT TLS，释放内部 RAM 给第二路 mbedTLS */
static void ota_mqtt_pause_for_download(void)
{
    if (client == NULL) {
        return;
    }
    esp_mqtt_client_stop(client);
    set_mqtt_status(0);
    s_mqtt_paused_for_ota = true;
    vTaskDelay(pdMS_TO_TICKS(300));
}

static void ota_mqtt_resume_after_download(void)
{
    if (!s_mqtt_paused_for_ota || client == NULL) {
        return;
    }
    s_mqtt_paused_for_ota = false;
    if (!get_wifi_status()) {
        return;
    }
    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "resume MQTT failed: %s", esp_err_to_name(err));
    }
}

//http操作函数
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ERROR) {
        ESP_LOGE(TAG, "HTTPS OTA HTTP error");
    }
    return ESP_OK;
}
//校验是否需要升级(版本比较)
esp_err_t device_firmware_version_check(void)
{
    char running_version[4] = {0};
    char upgrade_version[4] = {0};
    running_version[0] = device_info->ota.running_version[12];
    running_version[1] = device_info->ota.running_version[14];
    running_version[2] = device_info->ota.running_version[16];
    upgrade_version[0] = device_info->ota.upgrade_version[12];
    upgrade_version[1] = device_info->ota.upgrade_version[14];
    upgrade_version[2] = device_info->ota.upgrade_version[16];
    if (atoi(running_version) == atoi(upgrade_version))
    {
        ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
        //return ESP_FAIL;
    }
    return ESP_OK;
}
void upgrade_nvs_infor(void)
{
    char ota_version[128] = {0};
    sprintf(ota_version,"{\"id\": \"1\",\"params\": {\"version\": \"%s\",\"module\":\"default\"}}",device_info->ota.upgrade_version);
    esp_mqtt_client_publish(client, ota_infor_publish_topic, (char *)ota_version, strlen((char *)ota_version), 1, 0);

    nvs_handle ota_handlel;
    esp_err_t err;
    err = nvs_open("config_cfg", NVS_READWRITE, &ota_handlel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle, error: %s", esp_err_to_name(err));
        return; 
    }
    err = nvs_set_u8(ota_handlel, "otaFlag", 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set otaFlag, error: %s", esp_err_to_name(err));
        return; 
    }
    err = nvs_set_str(ota_handlel, "version", device_info->ota.upgrade_version);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set version, error: %s", esp_err_to_name(err));
        return; 
    }
    err = nvs_commit(ota_handlel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes, error: %s", esp_err_to_name(err));
        return; 
    }
    nvs_close(ota_handlel);

    /* xiugai
        ESP_ERROR_CHECK(nvs_open("config_cfg", NVS_READWRITE, &ota_handlel));
        ESP_ERROR_CHECK(nvs_set_u8(ota_handlel, "otaFlag", 1));
        ESP_ERROR_CHECK(nvs_set_str(ota_handlel, "version", device_info->ota.upgrade_version));
        ESP_ERROR_CHECK(nvs_commit(ota_handlel));
        nvs_close(ota_handlel);
    */
}

void advanced_ota_example_task(void *pvParameter)
{
    esp_err_t ota_finish_err = ESP_OK;
    esp_https_ota_handle_t https_ota_handle = NULL;

    ESP_LOGI(TAG, "OTA start: %s -> %s", device_info->ota.running_version,
             device_info->ota.upgrade_version);
    ota_mqtt_pause_for_download();

    esp_http_client_config_t config = {
        .url = device_info->ota.url,
        .event_handler = _http_event_handler,
        .cert_pem = (char *)ca_root_cert,
        .timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed: %s", esp_err_to_name(err));
        set_ota_now_flag(0);
        ota_mqtt_resume_after_download();
        vTaskDelete(NULL);
        return;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = device_firmware_version_check();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    while (1)
    {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
            ESP_LOGD(TAG, "https_ota_perform err");
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true)
    {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");   // 检查ota完整性
    }else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        set_ota_now_flag(0);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            upgrade_nvs_infor();
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        }else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            ota_mqtt_resume_after_download();
            vTaskDelete(NULL);
        }
    }
ota_end:
    set_ota_now_flag(0);
    if (https_ota_handle != NULL) {
        esp_https_ota_abort(https_ota_handle);
    }
    ota_mqtt_resume_after_download();
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    vTaskDelete(NULL);
}

void ota_start(void)
{
    if (get_ota_now_flag()) {
        ESP_LOGW(TAG, "OTA already in progress, ignore duplicate upgrade");
        return;
    }
    set_ota_now_flag(1);
    xTaskCreate(advanced_ota_example_task, "advanced_ota_example_task", 1024 * 8, NULL, 1, NULL);
}

