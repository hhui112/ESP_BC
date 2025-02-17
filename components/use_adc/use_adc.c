/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2021-10-07 23:03:40
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-07-27 10:34:56
 * @FilePath: /smart-air-bed-board-program/components/use_adc/use_adc.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "use_adc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "common.h"

static const char *TAG = "adc";
#define DEFAULT_VREF 1100 //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES 256  //Multisampling
#define PRESSURE_SENSOR_NUM 6

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel1 = ADC_CHANNEL_5; //GPIo36
static const adc_channel_t channel2 = ADC_CHANNEL_4; //GPIo39
static const adc_channel_t channel3 = ADC_CHANNEL_7; //GPIO34
static const adc_channel_t channel4 = ADC_CHANNEL_6; //GPIO35
static const adc_channel_t channel5 = ADC_CHANNEL_3; //GPIO32
static const adc_channel_t channel6 = ADC_CHANNEL_0; //GPIO33

static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;


void bps_adc_task(void *pv)
{
    //double vol_ntc, rt_ntc;
    //uint32_t voltage;
    uint32_t adc_reading_bps[PRESSURE_SENSOR_NUM];
    uint8_t pressure_sensor_current;
    for (;;)
    {
        for (pressure_sensor_current = 0; pressure_sensor_current < PRESSURE_SENSOR_NUM; pressure_sensor_current++)
        {
            adc_reading_bps[pressure_sensor_current] = 0;
        }

        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
            adc_reading_bps[0] += adc1_get_raw((adc1_channel_t)channel1);
            adc_reading_bps[1] += adc1_get_raw((adc1_channel_t)channel2);
            adc_reading_bps[2] += adc1_get_raw((adc1_channel_t)channel3);
            adc_reading_bps[3] += adc1_get_raw((adc1_channel_t)channel4);
            adc_reading_bps[4] += adc1_get_raw((adc1_channel_t)channel5);
            adc_reading_bps[5] += adc1_get_raw((adc1_channel_t)channel6);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void initialize_adc(void)
{
    //Configure ADC
    adc1_config_width(width);
    adc1_config_channel_atten(channel1, atten);
    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    xTaskCreatePinnedToCore(bps_adc_task, "BPS_ADC_TASK", 2048, NULL, 6, NULL, 1);
}
