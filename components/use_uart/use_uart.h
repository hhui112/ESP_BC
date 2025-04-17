/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2021-10-07 23:03:40
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-06-22 00:02:16
 * @FilePath: /smart-air-bed-board-program/components/use_uart/use_uart.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef USE_UART_H_
#define USE_UART_H_

extern union SyncCommunicationData_t g_Sync_TX;
extern union SyncCommunicationData_t g_Sync_RX;

void uart1_config(int txd_pin, int rxd_pin);
void uart2_config(void);
void mfp_gpio_config(void);
uint16_t Modbus_Crc_Compute(const uint8_t *buf, uint16_t bufLen);
uint16_t crc16_check(const uint8_t *buf, uint16_t bufLen);
unsigned char rxCalcCheckSum(void);
unsigned char syncCalcCheckSum(void);
void mfp_dateSend(void);
void  Debug_printf_buff(uint8_t *buff ,uint16_t len);

/*
void spi_init(void);
void spi_send_receive_data(uint8_t *tx_data, uint8_t *rx_data, size_t length);
void spi_send_data(uint8_t *data, size_t length);
*/
#endif