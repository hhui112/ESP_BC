/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2021-10-07 23:03:40
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-06-22 00:02:57
 * @FilePath: /smart-air-bed-board-program/components/use_pwm/use_pwm.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "use_pwm.h"
#include <stdio.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "soc/rtc.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "common.h"
#include "driver/gpio.h"
#define PUMB_NUM 22

void pwm_init(void)
{
  mcpwm_pin_config_t pin_config = {
      .mcpwm0a_out_num = PUMB_NUM,
  };
  mcpwm_set_pin(MCPWM_UNIT_0, &pin_config);
  mcpwm_config_t pwm_config;
  pwm_config.frequency = 1000; //frequency = 1000Hz
  pwm_config.cmpr_a =0;       //duty cycle of PWMxA = 60.0%
  //pwm_config.cmpr_b = 50.0;       //duty cycle of PWMxb = 50.0%
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_NUM_15);

  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

    mcpwm_sync_config_t sync_conf = {
        .sync_sig = MCPWM_SELECT_TIMER0_SYNC,
        .timer_val = 500,
        .count_direction = MCPWM_TIMER_DIRECTION_UP,
    };
    // ESP_ERROR_CHECK(mcpwm_sync_configure(MCPWM_UNIT_0, MCPWM_TIMER_0, &sync_conf));  
    // vTaskDelay(pdMS_TO_TICKS(10));
    // ESP_ERROR_CHECK(mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_NUM_23));
    // vTaskDelay(pdMS_TO_TICKS(2000));
     ESP_ERROR_CHECK(mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0));

      // mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
     mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0);
     mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
}
#define GPIO_INPUT_IO_0     12
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_0)
void Key_Gpio_init(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};  
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;

    gpio_config_t gpio_two = {
        .intr_type=GPIO_INTR_DISABLE,
        .mode=GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << 13,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en =GPIO_PULLUP_DISABLE 
    };
   
    gpio_config(&gpio_two);

}

void M_Ctr(uint8_t data)
{

  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, data);
}

/*
void Taskss(void)
{
  uint8_t temp;
    temp = gpio_get_level(GPIO_INPUT_IO_0);

}
*/