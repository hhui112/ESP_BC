/*
 * @Author: zhong chenjian zhongcj@softide.cn
 * @Date: 2021-10-07 23:03:40
 * @LastEditors: zhong chenjian zhongcj@softide.cn
 * @LastEditTime: 2022-06-22 00:02:34
 * @FilePath: /smart-air-bed-board-program/components/use_uart/use_uart.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "use_uart.h"
#include "common.h"

/**
 * This is an example which echos any data it receives on UART1 back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: UART1
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below
 */

#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_TXD (19)
#define ECHO_TEST_RXD (25)


#define BUF_SIZE (1024)
#define ECHO_UART_PORT_NUM      (2)
#define ECHO_UART_BAUD_RATE     (115200)            // 38400
#define ECHO_TASK_STACK_SIZE    (4096)


void uart1_config(int txd_pin, int rxd_pin)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, BUF_SIZE, BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    uart_set_pin(UART_NUM_1, txd_pin, rxd_pin, ECHO_TEST_RTS, ECHO_TEST_CTS);
}



void mfp_gpio_config(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << UART_CTR),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(UART_CTR, 1);  
}

static uint16_t crcTb[] = {
        0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
        0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
        0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
        0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
        0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
        0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
        0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
        0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
        0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
        0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
        0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
        0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
        0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
        0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
        0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
        0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
        0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
        0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
        0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
        0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
        0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
        0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
        0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
        0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
        0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
        0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
        0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
        0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
        0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
        0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
        0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
        0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
};

// 在这里的crc16_check = Modbus_Crc_Compute
uint16_t crc16_check(const uint8_t *buf, uint16_t bufLen) {
    uint8_t num;
    uint16_t modbus16 = UINT16_MAX;

    for (uint16_t index = 0; index < bufLen; ++index) {
        num = (uint8_t) (modbus16 & UINT32_MAX);
        modbus16 = (uint16_t) (((uint32_t) modbus16 >> 8) ^ crcTb[(num ^ buf[index]) & UINT8_MAX]);
    }
    return modbus16;
}

unsigned char rxCalcCheckSum(void)
{
	uint16_t i;
	uint8_t sum;
	sum = 0xff;
	for (i = 0; i < ((uint16_t)2 + (uint16_t)g_Sync_RX.Syncdata.length); i++)
	{
		sum -= g_Sync_RX.rawData[i];
	}
	return sum;
}
unsigned char syncCalcCheckSum(void)
{
	uint16_t i;
	uint8_t sum;
	sum = 0xff;
	for (i = 0; i < ((uint16_t)2 + (uint16_t)g_Sync_TX.Syncdata.length); i++)
	{
		sum -= g_Sync_TX.rawData[i];
	}
	//printf("sum = %d \r\n",sum);
	return sum;
}


void  Debug_printf_buff(uint8_t *buff ,uint16_t len)
{
	int i = 0;
	printf("\r\n");
	for(i = 0; i < len ;i++)
	{
		printf("%02X ",buff[i]);
	}
	printf("\r\n");
}


/*  
*   封装的 mfp发送函数，使用全局变量进行发送，需将要发送的数据填入全局变量中
*   uart_write_bytes(ECHO_UART_PORT_NUM, g_Sync_TX.rawData, g_Sync_TX.Syncdata.length+3);
*/
void mfp_dateSend(void)
{   
    printf("mfp_dateSend\n");
    Debug_printf_buff(g_Sync_TX.rawData,g_Sync_TX.Syncdata.length+3);
    uart_flush(ECHO_UART_PORT_NUM);
    gpio_set_level(UART_CTR, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    uart_write_bytes(ECHO_UART_PORT_NUM, g_Sync_TX.rawData, g_Sync_TX.Syncdata.length+3);
    uart_wait_tx_done(ECHO_UART_PORT_NUM, pdMS_TO_TICKS(10));
    gpio_set_level(UART_CTR, 1);
}

/*  SPI设置 万一以后要用

#define SPI_HOST1    HSPI_HOST
#define SPI_DMA_CH_AUTO SPI_DMA_CH_AUTO
#define SPI_CLK_SPEED 1000000  // 1 MHz SPI 速率
#define SPI_MOSI_PIN   12  // DO (Data Out)
#define SPI_MISO_PIN   13  // DI (Data In)
#define SPI_CLK_PIN    14  // CLK (Clock)
#define SPI_CS_PIN     15  // Chip Select

spi_device_handle_t spi_handle;
static bool spi_initialized = false;

void spi_deinit() {
    esp_err_t ret = spi_bus_free(SPI_HOST1);
    if (ret != ESP_OK) {
        ESP_LOGE("SPI", "Failed to free SPI bus: %s", esp_err_to_name(ret));
    }
    spi_initialized = false;
}
    
void spi_init(void) {
    if (spi_initialized) {
        return;  // 如果 SPI 已经初始化，直接返回
    }

    esp_err_t ret;

    // 配置 SPI 总线
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPI_MOSI_PIN,
        .miso_io_num = SPI_MISO_PIN,
        .sclk_io_num = SPI_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };

    // 配置 SPI 设备
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = SPI_CLK_SPEED,     // 1 MHz clock speed
        .duty_cycle_pos = 128,               // 50% duty cycle
        .mode = 3,                           // SPI Mode 2: CPOL = 1, CPHA = 0
        .spics_io_num = SPI_CS_PIN,          // CS (Chip Select) pin
        .cs_ena_posttrans = 3,               // Keep the CS low for 3 cycles after transaction
        .queue_size = 3
    };

    // 初始化 SPI 总线
    ret = spi_bus_initialize(SPI_HOST1, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("SPI", "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }

    // 将设备添加到 SPI 总线上
    ret = spi_bus_add_device(SPI_HOST1, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("SPI", "Failed to add SPI device: %s", esp_err_to_name(ret));
        return;
    }

    spi_initialized = true;  // 设置标志，标记 SPI 已初始化
    ESP_LOGI("SPI", "SPI bus initialized successfully");
}


// SPI 数据发送与接收
void spi_send_receive_data(uint8_t *tx_data, uint8_t *rx_data, size_t length) {
    esp_err_t ret;

    // uint8_t tx_data_copy[length];
    // memcpy(tx_data_copy, tx_data, length); 
    
    spi_transaction_t t = {
        .length = length * 8,  // 数据长度（位数）
        .tx_buffer = tx_data,  // 发送数据的缓冲区
        .rx_buffer = rx_data,  // 接收数据的缓冲区
        .flags = 0,            // 清除 SPI_TRANS_USE_TXDATA 标志
    };

    ret = spi_device_transmit(spi_handle, &t);  // 执行 SPI 传输
    // ESP_ERROR_CHECK(ret);
    if (ret != ESP_OK) {
        ESP_LOGE("SPI", "SPI transmission failed, error code: %d", ret);
        return;
    }
 


}


// SPI 数据发送函数
void spi_send_data(uint8_t *data, size_t length) {
    esp_err_t ret;
    spi_transaction_t t = {
        .length = length * 8,  // 数据长度（位数）
        .tx_buffer = data,     // 发送数据的缓冲区
        .rx_buffer = NULL,     // 不接收数据
    };

    ret = spi_device_transmit(spi_handle, &t);  // 执行 SPI 传输
    ESP_ERROR_CHECK(ret);
}
*/
