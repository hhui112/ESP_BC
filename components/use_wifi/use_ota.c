/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2021-10-07 23:03:40
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-06-22 00:03:29
 * @FilePath: /smart-air-bed-board-program/components/use_wifi/use_ota.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
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
extern char ota_progress_publish_topic[64];
extern char user_airbag_publish_topic[64];

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

bool ota_mqtt_is_paused(void)
{
    return s_mqtt_paused_for_ota;
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
    ESP_LOGI(TAG, "url: %s", device_info->ota.url);
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

    {
        int image_size = -1;
        int last_pct = -1;
        int read_len = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        image_size = esp_https_ota_get_image_size(https_ota_handle);
#endif
        ESP_LOGI(TAG, "开始下载板端固件，目标大小=%d bytes", image_size);

        while (1) {
            err = esp_https_ota_perform(https_ota_handle);
            read_len = esp_https_ota_get_image_len_read(https_ota_handle);

            if (image_size > 0) {
                int pct = (int)(((int64_t)read_len * 100) / image_size);
                if (pct > 100) {
                    pct = 100;
                }
                if (pct != last_pct) {
                    last_pct = pct;
                    ESP_LOGI(TAG, "板端 OTA 进度 %3d%%  (%d/%d)", pct, read_len, image_size);
                }
            } else if ((read_len >> 16) != last_pct) {
                /* 无 Content-Length 时按 64KB 步进打印 */
                last_pct = read_len >> 16;
                ESP_LOGI(TAG, "板端 OTA 已下载 %d bytes", read_len);
            }

            if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                break;
            }
        }
        ESP_LOGI(TAG, "板端下载结束: err=%s read=%d/%d",
                 esp_err_to_name(err), read_len, image_size);
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
            ESP_LOGI(TAG, "板端 OTA 成功 100%%，即将重启");
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
    if (xTaskCreate(advanced_ota_example_task, "advanced_ota_example_task",
                    1024 * 8, NULL, 1, NULL) != pdPASS) {
        set_ota_now_flag(0);
        ESP_LOGE(TAG, "create board OTA task failed");
    }
}

#define OTA_MODULE_DEFAULT  "default"
#define OTA_MODULE_SENSOR   "sensor"
#define SENSOR_FW_MAX_BYTES (1024U * 1024U)

static void ota_report_progress(const char *module, const char *step, const char *desc)
{
    char buf[280];

    if (step == NULL || ota_progress_publish_topic[0] == '\0') {
        return;
    }
    if (module == NULL || module[0] == '\0') {
        module = OTA_MODULE_DEFAULT;
    }
    snprintf(buf, sizeof(buf),
             "{\"id\":\"1\",\"params\":{\"step\":\"%s\",\"desc\":\"%s\",\"module\":\"%s\"}}",
             step, (desc && desc[0]) ? desc : "", module);
    ESP_LOGI(TAG, "ota progress module=%s step=%s %s", module, step, desc ? desc : "");
    if (client != NULL && get_mqtt_status()) {
        esp_mqtt_client_publish(client, ota_progress_publish_topic, buf,
                                (int)strlen(buf), 1, 0);
    }
}

static void ota_report_inform(const char *module, const char *version)
{
    char buf[192];

    if (version == NULL || version[0] == '\0' || ota_infor_publish_topic[0] == '\0') {
        return;
    }
    if (module == NULL || module[0] == '\0') {
        module = OTA_MODULE_DEFAULT;
    }
    snprintf(buf, sizeof(buf),
             "{\"id\":\"1\",\"params\":{\"version\":\"%s\",\"module\":\"%s\"}}",
             version, module);
    ESP_LOGI(TAG, "ota inform module=%s ver=%s", module, version);
    if (client != NULL && get_mqtt_status()) {
        esp_mqtt_client_publish(client, ota_infor_publish_topic, buf,
                                (int)strlen(buf), 1, 0);
    }
}

static bool sensor_ver_to_cloud_format(const char *in, char *out, size_t out_len)
{
    int a = 0, b = 0, c = 0, board = 0;
    const char *prefix = "SU2";

    if (in == NULL || out == NULL || out_len < 16U) {
        return false;
    }
    if (strncmp(in, "SU3", 3) == 0) {
        prefix = "SU3";
    } else if (strncmp(in, "SU2", 3) != 0) {
        return false;
    }
    if (in[3] == '_') {
        strncpy(out, in, out_len - 1U);
        out[out_len - 1U] = '\0';
        return true;
    }
    if ((in[3] == '-') &&
        sscanf(in + 4, "%d.%d.%d,Board:%d", &a, &b, &c, &board) == 4) {
        snprintf(out, out_len, "%s_%d_%d_%d_Board_%d", prefix, a, b, c, board);
        return true;
    }
    return false;
}

static void sensor_ota_report_ok(const char *version)
{
    ota_report_progress(OTA_MODULE_SENSOR, "100", "sensor ota success");
    ota_report_inform(OTA_MODULE_SENSOR,
                      version && version[0] ? version : device_info->ota.upgrade_version);
}

static void sensor_ota_report_fail(const char *step, const char *desc)
{
    ota_report_progress(OTA_MODULE_SENSOR, step, desc);
}

void sensor_ota_report_version_on_mqtt(void)
{
    const char *cli_ver = get_sensor_version();
    char cloud_ver[48];

    if (cli_ver == NULL || cli_ver[0] == '\0') {
        return;
    }
    if (sensor_ver_to_cloud_format(cli_ver, cloud_ver, sizeof(cloud_ver))) {
        ota_report_inform(OTA_MODULE_SENSOR, cloud_ver);
    } else {
        ota_report_inform(OTA_MODULE_SENSOR, cli_ver);
    }
}

/* wait_mqtt：utc 任务里可等最多 20s；MQTT 回调里必须为 false，禁止阻塞事件循环 */
static void airbag_publish_versions(bool wait_mqtt)
{
    char buf[320];
    char cloud_ver[48];
    const char *esp_ver;
    const char *cli_ver;
    const char *su2_ver = "NULL";
    int i;

    if (wait_mqtt) {
        for (i = 0; i < 40; i++) {
            if (get_mqtt_status() && user_airbag_publish_topic[0] != '\0') {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    if (client == NULL || !get_mqtt_status() || user_airbag_publish_topic[0] == '\0') {
        ESP_LOGW(TAG, "airbag version skip: mqtt not ready");
        return;
    }

    esp_ver = device_info->ota.running_version;
    if (esp_ver == NULL || esp_ver[0] == '\0') {
        esp_ver = INIT_VERSION;
    }
    cli_ver = get_sensor_version();
    if (cli_ver != NULL && cli_ver[0] != '\0') {
        if (sensor_ver_to_cloud_format(cli_ver, cloud_ver, sizeof(cloud_ver))) {
            su2_ver = cloud_ver;
        } else {
            su2_ver = cli_ver;
        }
    }

    /* 自定义 user/airbag/put：板端必报；SU2 未读到则 su_version 为 NULL */
    snprintf(buf, sizeof(buf),
             "{\"id\":\"%s\",\"ts\":%d,\"value\":{"
             "\"bc_module\":\"default\",\"bc_version\":\"%s\","
             "\"su_module\":\"sensor\",\"su_version\":\"%s\"}}",
             device_info->id,
             device_info->utc.time_stamp,
             esp_ver,
             su2_ver);
    ESP_LOGI(TAG, "airbag version: %s", buf);
    esp_mqtt_client_publish(client, user_airbag_publish_topic, buf,
                            (int)strlen(buf), 1, 0);
}

void airbag_report_versions(void)
{
    airbag_publish_versions(true);
}

void airbag_report_versions_on_mqtt(void)
{
    airbag_publish_versions(false);
}

bool ota_upgrade_is_sensor_fw(const char *version)
{
    if (version == NULL || version[0] == '\0') {
        return false;
    }
    return (strncmp(version, "SU2", 3) == 0 || strncmp(version, "SU3", 3) == 0);
}

static esp_err_t sensor_fw_http_download(const char *url, uint8_t **out_buf, size_t *out_len)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = _http_event_handler,
        .cert_pem = (char *)ca_root_cert,
        .timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client_http;
    int content_len;
    int status;
    int total = 0;
    int n;
    uint8_t *buf = NULL;
    esp_err_t err;

    *out_buf = NULL;
    *out_len = 0;

    client_http = esp_http_client_init(&cfg);
    if (client_http == NULL) {
        return ESP_FAIL;
    }
    err = esp_http_client_open(client_http, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sensor fw http open fail: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client_http);
        return err;
    }
    content_len = esp_http_client_fetch_headers(client_http);
    status = esp_http_client_get_status_code(client_http);
    printf("ota: sensor fw HTTP status=%d len=%d\n", status, content_len);
    if (status != 200 || content_len <= 0 || (size_t)content_len > SENSOR_FW_MAX_BYTES) {
        ESP_LOGE(TAG, "sensor fw bad response status=%d len=%d", status, content_len);
        esp_http_client_close(client_http);
        esp_http_client_cleanup(client_http);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "开始下载传感器固件，目标大小=%d bytes", content_len);

    buf = (uint8_t *)heap_caps_malloc((size_t)content_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        buf = (uint8_t *)malloc((size_t)content_len);
    }
    if (buf == NULL) {
        ESP_LOGE(TAG, "sensor fw malloc %d fail", content_len);
        esp_http_client_close(client_http);
        esp_http_client_cleanup(client_http);
        return ESP_ERR_NO_MEM;
    }

    {
        int last_pct = -1;
        while (total < content_len) {
            int pct;
            n = esp_http_client_read(client_http, (char *)buf + total, content_len - total);
            if (n < 0) {
                ESP_LOGE(TAG, "sensor fw read err");
                free(buf);
                esp_http_client_close(client_http);
                esp_http_client_cleanup(client_http);
                return ESP_FAIL;
            }
            if (n == 0) {
                break;
            }
            total += n;
            pct = (int)(((int64_t)total * 100) / content_len);
            if (pct > 100) {
                pct = 100;
            }
            if (pct != last_pct) {
                last_pct = pct;
                ESP_LOGI(TAG, "传感器下载进度 %3d%%  (%d/%d)", pct, total, content_len);
            }
        }
    }
    esp_http_client_close(client_http);
    esp_http_client_cleanup(client_http);

    if (total != content_len) {
        ESP_LOGE(TAG, "sensor fw short read %d/%d", total, content_len);
        free(buf);
        return ESP_FAIL;
    }

    *out_buf = buf;
    *out_len = (size_t)total;
    return ESP_OK;
}

static void sensor_ota_download_task(void *pv)
{
    const char *cur_ver;
    uint8_t *buf = NULL;
    size_t len = 0;
    esp_err_t err;
    int wait_i;
    (void)pv;

    ota_report_progress(OTA_MODULE_SENSOR, "5", "wait sensor");
    for (wait_i = 0; wait_i < 120; wait_i++) {
        if (get_devic_id_flag() && get_sleep_up_flag() == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (!get_devic_id_flag()) {
        ESP_LOGE(TAG, "sensor OTA abort: sensor not ready");
        sensor_ota_report_fail("-1", "sensor not ready");
        set_ota_now_flag(0);
        vTaskDelete(NULL);
        return;
    }
    if (get_sleep_up_flag()) {
        ESP_LOGE(TAG, "sensor OTA abort: sleep report busy");
        sensor_ota_report_fail("-1", "sleep report busy");
        set_ota_now_flag(0);
        vTaskDelete(NULL);
        return;
    }

    cur_ver = get_sensor_version();
    printf("ota: sensor OTA %s -> %s (cloud push, always flash)\n",
           (cur_ver && cur_ver[0]) ? cur_ver : "?",
           device_info->ota.upgrade_version);

    ota_report_progress(OTA_MODULE_SENSOR, "10", "downloading");
    ota_mqtt_pause_for_download();
    err = sensor_fw_http_download(device_info->ota.url, &buf, &len);
    ota_mqtt_resume_after_download();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sensor fw download failed");
        sensor_ota_report_fail("-2", "download failed");
        set_ota_now_flag(0);
        vTaskDelete(NULL);
        return;
    }
    printf("ota: sensor fw downloaded len=%u\n", (unsigned)len);
    for (wait_i = 0; wait_i < 20; wait_i++) {
        if (get_mqtt_status()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ota_report_progress(OTA_MODULE_SENSOR, "50", "download ok");

    for (wait_i = 0; wait_i < 60; wait_i++) {
        if (get_sleep_up_flag() == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (get_sleep_up_flag()) {
        ESP_LOGE(TAG, "sensor OTA abort: sleep report busy before flash");
        free(buf);
        sensor_ota_report_fail("-1", "sleep report busy");
        set_ota_now_flag(0);
        vTaskDelete(NULL);
        return;
    }

    ota_report_progress(OTA_MODULE_SENSOR, "60", "uart flashing");
    err = sensor_ota_flash(buf, len);
    free(buf);

    if (err == ESP_OK) {
        set_sensor_version(device_info->ota.upgrade_version);
        vTaskDelay(pdMS_TO_TICKS(2000));
        sensor_ota_report_ok(device_info->ota.upgrade_version);
    } else {
        ESP_LOGE(TAG, "sensor uart flash failed");
        sensor_ota_report_fail("-4", "uart flash failed");
    }

    set_ota_now_flag(0);
    vTaskDelete(NULL);
}

void sensor_ota_start(void)
{
    if (get_ota_now_flag()) {
        ESP_LOGW(TAG, "OTA already in progress, ignore sensor upgrade");
        return;
    }
    if (device_info->ota.url[0] == '\0' || device_info->ota.upgrade_version[0] == '\0') {
        ESP_LOGE(TAG, "sensor OTA missing url/version");
        sensor_ota_report_fail("-1", "missing url or version");
        return;
    }
    set_ota_now_flag(1);
    if (xTaskCreate(sensor_ota_download_task, "sensor_ota_dl",
                    1024 * 10, NULL, 5, NULL) != pdPASS) {
        set_ota_now_flag(0);
        ESP_LOGE(TAG, "create sensor OTA task failed");
        sensor_ota_report_fail("-1", "create task failed");
    }
}

