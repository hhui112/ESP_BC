#ifndef USE_MFP_H_
#define USE_MFP_H_

#define MFP_TX_QUEUE_MAX_ITEMS 30
#define MFP_TX_DATA_MAX_LEN    32

typedef struct {
    uint8_t data[MFP_TX_DATA_MAX_LEN];
    uint8_t len;
    uint8_t repeat;
} mfp_tx_request_t;


void mfp_tx_queue_init(void);
bool mfp_queue_push(const uint8_t *data, uint8_t len, uint8_t repeat);
void mfp_tx_queue_clear(void);
bool mfp_tx_queue_is_empty(void);
void mfp_queue_pop_send(void);
void prepare_mfp_NORMAL_KET(uint32_t keys,uint8_t repeat);	
void prepare_mfp_SOFT_START(uint32_t keys,uint8_t pwm, uint8_t tmr,uint8_t repeat); 

#endif