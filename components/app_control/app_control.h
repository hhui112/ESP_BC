/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2022-06-18 09:19:43
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-06-27 12:57:58
 * @FilePath: /smart-air-bed-board-program/components/app_control/app_control.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*
 *                        _oo0oo_
 *                       o8888888o
 *                       88" . "88
 *                       (| -_- |)
 *                       0\  =  /0
 *                     ___/`---'\___
 *                   .' \\|     |// '.
 *                  / \\|||  :  |||// \
 *                 / _||||| -:- |||||- \
 *                |   | \\\  - /// |   |
 *                | \_|  ''\---/''  |_/ |
 *                \  .-\__  '-'  ___/-. /
 *              ___'. .'  /--.--\  `. .'___
 *           ."" '<  `.___\_<|>_/___.' >' "".
 *          | | :  `- \`.;`\ _ /`;.`/ - ` : | |
 *          \  \ `_.   \_ __\ /__ _/   .-` /  /
 *      =====`-.____`.___ \_____/___.-`___.-'=====
 *                        `=---='
 * 
 * 
 *      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 
 *            佛祖保佑     永不宕机     永无BUG
 */

/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2022-06-18 09:19:43
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-06-22 15:14:19
 * @FilePath: /smart-air-bed-board-program/components/app_control/app_control.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef APP_CONTROL_H_
#define APP_CONTROL_H_
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

void app_control_server(void);
int set_bc(uint32_t time_stamp, char *value, uint8_t switch_return, uint8_t switch_to_aliyun, char *return_value, uint16_t time_ms);
uint8_t get_sleep_up_flag(void);
void set_sleep_up_flag(uint8_t data);
void set_ota_now_flag(uint8_t data);
uint8_t get_ota_now_flag(void);
void set_report_cli(uint8_t data1,uint8_t data2);
void set_cli_report_name(char* data,char len);
uint8_t get_devic_id_flag(void);
void sensor_reboot_config(void);
int sensor_ota_bc(char *value);
int mqtt_send_data(uint32_t time_stamp, char *value, uint8_t switch_return, uint8_t switch_to_aliyun, char *return_value, uint16_t time_ms);
void check_report_and_up_to_aliyun(void);   //xinzeng
void set_mode_flag_config(uint8_t data);
uint8_t get_mode_flag_config(void);
extern bool mqtt_send_mutex;
#endif
