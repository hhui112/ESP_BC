#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "common.h"
#include "freertos/semphr.h"
#include "use_mfp.h"
#include "use_uart.h"

static SemaphoreHandle_t mfp_queue_mutex = NULL;

static mfp_tx_request_t tx_queue[MFP_TX_QUEUE_MAX_ITEMS];
static uint8_t head = 0;
static uint8_t tail = 0;
static uint8_t count = 0;

/*
* @brief  初始化队列
*/
void mfp_tx_queue_init(void)
{
    head = 0;
    tail = 0;
    count = 0;

    if (mfp_queue_mutex == NULL) {
        mfp_queue_mutex = xSemaphoreCreateMutex();
    }
}

/*
* @brief  发送请求到队列
* @param  data: 发送的数据 len: 数据长度 repeat: 重复次数
* @return true: 成功 false: 失败
*/
bool mfp_queue_push(const uint8_t *data, uint8_t len, uint8_t repeat)
{
    if (xSemaphoreTake(mfp_queue_mutex, portMAX_DELAY) == pdTRUE) {
        if (count >= MFP_TX_QUEUE_MAX_ITEMS || len > MFP_TX_DATA_MAX_LEN) {
            printf("[MFP_TX_QUEUE] Queue FULL! count=%d\n", count);
            xSemaphoreGive(mfp_queue_mutex);
            return false;
        }

        memcpy(tx_queue[tail].data, data, len);
        tx_queue[tail].len = len;
        tx_queue[tail].repeat = repeat;

        tail = (tail + 1) % MFP_TX_QUEUE_MAX_ITEMS;
        count++;

        xSemaphoreGive(mfp_queue_mutex);
        return true;
    }
    return false;
}

/*
* @brief  清空队列
*/
void mfp_tx_queue_clear(void)
{
    if (xSemaphoreTake(mfp_queue_mutex, portMAX_DELAY) == pdTRUE) {
        head = 0;
        tail = 0;
        count = 0;
        xSemaphoreGive(mfp_queue_mutex);
    }
}

bool mfp_tx_queue_is_empty(void)
{
    return (count == 0);
}


/*
* @brief  发送队列中的数据
*/
void mfp_queue_pop_send(void)
{
    if (xSemaphoreTake(mfp_queue_mutex, portMAX_DELAY) == pdTRUE) {
        if (count == 0) {
            xSemaphoreGive(mfp_queue_mutex);
            return;
        }
        
        mfp_tx_request_t *req = &tx_queue[head];
        
        if (req->repeat > 0) {
            uart_mfp_send(req->data, req->len);

            req->repeat--;
            if (req->repeat == 0) {
                head = (head + 1) % MFP_TX_QUEUE_MAX_ITEMS;
                count--;
            }
        }
        xSemaphoreGive(mfp_queue_mutex);
    }
}



/*
void mfp_queue_pop_send(void)
{
    printf("[%d] pop_send start, count=%d\n", xTaskGetTickCount(), count);
    
    if (xSemaphoreTake(mfp_queue_mutex, portMAX_DELAY) == pdTRUE) {
        printf("[%d] Got mutex\n", xTaskGetTickCount());
        
        if (count == 0) {
            printf("[%d] Queue empty!\n", xTaskGetTickCount());
            xSemaphoreGive(mfp_queue_mutex);
            return;
        }
        
        mfp_tx_request_t *req = &tx_queue[head];
        printf("[%d] head=%d, repeat=%d, len=%d\n", 
               xTaskGetTickCount(), head, req->repeat, req->len);
        
        if (req->repeat > 0) {
            uart_mfp_send(req->data, req->len);
            req->repeat--;
            
            if (req->repeat == 0) {
                head = (head + 1) % MFP_TX_QUEUE_MAX_ITEMS;
                count--;
                printf("[%d] Item done, new count=%d\n", xTaskGetTickCount(), count);
            }
        }
        xSemaphoreGive(mfp_queue_mutex);
        printf("[%d] Mutex released\n", xTaskGetTickCount());
    }
}
*/

void prepare_mfp_NORMAL_KET(uint32_t keys,uint8_t repeat) 
{
    uint8_t uartTxbuff[20];
    memset(uartTxbuff,0,sizeof(uartTxbuff));
    int i = 0;
    uartTxbuff[i++] = 0x05;
    uartTxbuff[i++] = 0x01;
    uartTxbuff[i++] = (keys) & 0xFF;
    uartTxbuff[i++] = (keys >> 8) & 0xFF;
    uartTxbuff[i++] = (keys >> 16) & 0xFF;
    uartTxbuff[i++] = (keys >> 24) & 0xFF;  
    uartTxbuff[i++] = 0x00;
    uint8_t checksum = syncCalcCheckSum_mfpqueue(uartTxbuff, i);
    uartTxbuff[i++] = checksum;
    // LOG_I("SUM = %d\n", uartTxbuff[i-1]);
	
    if (!mfp_queue_push(uartTxbuff, i, repeat))
    {
        printf("MFP QUERE ERR! \r\n");
    }
}	


void prepare_mfp_SOFT_START(uint32_t keys,uint8_t pwm, uint8_t tmr,uint8_t repeat) 
{
    uint8_t uartTxbuff[20];
    memset(uartTxbuff,0,sizeof(uartTxbuff));
    int i = 0;
    uartTxbuff[i++] = 0x07;
    uartTxbuff[i++] = 0x01;
    uartTxbuff[i++] = (keys) & 0xFF;        
    uartTxbuff[i++] = (keys >> 8) & 0xFF;
    uartTxbuff[i++] = (keys >> 16) & 0xFF;
    uartTxbuff[i++] = (keys >> 24) & 0xFF;  
    uartTxbuff[i++] = 0x00;
    uartTxbuff[i++] = pwm;
    uartTxbuff[i++] = tmr;
    uint8_t checksum = syncCalcCheckSum_mfpqueue(uartTxbuff, i);
    uartTxbuff[i++] = checksum;
    // LOG_I("SUM = %d\n", uartTxbuff[i-1]);

    if (!mfp_queue_push(uartTxbuff, i, repeat))
    {
        printf("MFP QUERE ERR! \r\n");
    }
}
