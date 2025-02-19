/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2022-06-17 00:44:17
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-07-27 10:38:21
 * @FilePath: /smart-air-bed-board-program/components/common/common.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef COMMON_H_
#define COMMON_H_
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define BLE_TEST                                   0
#define ALIYUN_BURN                                1

#define DATA_UP                                     1
#define OTA_ON_OFF                                  1

#define MY_WIFI_SSID                                "kakakaka"//"keeson-office"//"DS" // xmhdesktop
#define MY_WIFI_PASSWD                              "77885522"//"ksn88888"//"ds654321"  // 87632154
#define DEVEICE_ID                                  "KSPSBED00001057"
#define PRODUCT_KEY                                 "ixvaCaIfGla"
#define DEVEICE_SECRET                              "fd3e5207b753226032a602f2b7c44804"
#define INIT_VERSION                                "BC_ESP_2025_1_2_4"//"PS_20230906_0_0_1"        old :BC_ESP_2023_0_1_5  news: BC_ESP_2025_1_0_1
// #define CINFIG_VERSION                              "settingConfig_001" 

#define UART1_TXD                                   (22)
#define UART1_RXD                                   (26)
#define RX_BUF_SIZE                                 (1024*10)   //缩减1024*8
#define LED_BLUE                                    (19)
#define LED_GREEN                                   (18) 
#define LED_RED                                     (5)   

enum 
{
    wifi_ok = 0,
    wifi_fail,
    mqtt_ok,
    mqtt_fail,
}one_key_config_wifi_event_id_t;


//一键配网参数定义
typedef struct
{
    uint8_t flag;
    QueueHandle_t xQueue;
    char ssid[32];              //wifi名                                              
    char passwd[64];            //wifi密码
}one_key_config_wifi_info_t;

typedef struct
{
    uint8_t value[512];
    uint16_t len;
} data_rec_t;

typedef struct
{
    char topic[512];
    uint32_t topic_len;
    uint8_t data[512];
    uint32_t data_len;
} mmqtt_msg_t;

//wifi信息定义
typedef struct
{
    one_key_config_wifi_info_t one_key_config;         
    char ip_addr[16];                                               //ip地址
    int rssi;                                               //wifi信号强度
    uint8_t flag;                                                     
}wifi_link_info_t;

typedef struct
{
    char running_version[32];
    char upgrade_version[32];
    bool flag;
    char url[100];
}ota_info_t;

typedef struct
{
    char product_key[20];
    char device_id[20];
    char device_secret[50];
    bool flag;
    QueueHandle_t xQueue;
    mmqtt_msg_t msg;
}aliyun_link_info_t;

typedef struct
{
    char product_key[20];
    char device_name[20];
    char device_secret[50];
} qs_settings_aliyun_t;

typedef struct
{
    bool flag;
    int32_t time_stamp;
    uint8_t time[6];
}utc_info_t;

typedef struct
{
    bool flag;
    uint16_t gatts_if;
    uint16_t conn_id;
    uint16_t handle;
    QueueHandle_t xQueue;
    data_rec_t data_rec;
}ble_link_info_t;

typedef struct
{
    wifi_link_info_t wifi;
    ble_link_info_t *ble;
    ota_info_t ota;
    aliyun_link_info_t aliyun;
    utc_info_t utc;
    uint8_t data_up_switch;
    char report[128];
    char id[20];
} device_info_t;

void device_init(void);

uint8_t get_wifi_status(void);
void set_wifi_status(uint8_t value);
uint8_t get_ble_status(void);
void set_ble_status(uint8_t value);
uint8_t get_mqtt_status(void);
void set_mqtt_status(uint8_t value);
uint8_t get_one_key_config_wifi_status(void);
void set_one_key_config_wifi_status(uint8_t value);
#endif