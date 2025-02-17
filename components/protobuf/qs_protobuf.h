/*
 * @Author: wayne cao 
 * @Date: 2021-08-25 14:24:14 
 * @Last Modified by: wayne cao
 * @Last Modified time: 2021-08-25 16:32:26
 * 
 * @Date: 2025-01-15 21:19:15
 * @Last Modified by: fzh
 * @Last Modified time: 2025-01-15 21:19:15
 */
#pragma once
#define QS_PB_CLI_COMMAND_SIZE                      128
#define QS_PB_DEVICE_ID_SIZE                        24
#define QS_PB_NOTIFICATION_TEXT_SIZE                1024
#define QS_PB_RAW_DATA_LEGNTH                       2048
#define QS_PB_BED_CONTRL_SIZE                   512

#define QS_VER_POST_TOPIC_ID                        1
#define QS_TOKEN_POST_TOPIC_ID                      2
#define QS_DOWNLOAD_RANGE_POST_TOPIC_ID             3
#define QS_CLI_COMMAND_TOPIC_ID                     4
#define QS_SLEEP_CYCLE_REPO_TOPIC_ID                5
#define QS_STATE_RAW_DATA_TOPIC_ID                  6
#define QS_HEARTBEAT_RAW_DATA_TOPIC_ID              7
#define QS_BREATHRATE_RAW_DATA_TOPIC_ID             8
#define QS_MOVEMENT_RAW_DATA_TOPIC_ID               9
#define QS_SNORE_RAW_DATA_TOPIC_ID                  10
#define QS_SBP_RAW_DATA_TOPIC_ID                    11
#define QS_DBP_RAW_DATA_TOPIC_ID                    12

#define QS_SENSOR_5SEC_TOPIC_ID                     13
#define QS_SENSOR_1MIN_TOPIC_ID                     14
#define QS_SENSOR_SAE_TOPIC_ID                      15

#define QS_BED_COMMAND_TOPIC_ID                     33
#define QS_BED_STATUS_TOPIC_ID                      34

typedef enum {
  QS_PB_SUCCESS = 0,
  QS_PB_INTERNAL_ERROR = 101,
  QS_PB_TIMEOUT = 102,
  QS_PB_NO_RESOURCE = 103
}enum_command_status;

typedef enum {
  QS_SUCCESS = 0,
  QS_ERROR_INTERNAL = 101,
  QS_TIMEOUT = 102,
  QS_NO_RESOURCE = 103
}qs_ret_code_t;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    int32_t software_major_ver;
    int32_t software_minor_ver;
    int32_t software_revise_ver;
} qs_pb_msg_ver_post;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    int32_t software_major_ver;
    int32_t software_minor_ver;
    int32_t software_revise_ver;
    int32_t firmware_len;
} qs_pb_msg_token_post;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    int32_t firmware_len;
    int32_t firmware_start_offset;
    int32_t firmware_end_offset;
} qs_pb_msg_download_range_post;


typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    char command[QS_PB_CLI_COMMAND_SIZE];   
} qs_pb_msg_cli_command;


typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    int32_t sequence;
    uint8_t status[12];
    int32_t heartbeat;
    int32_t breathRate;
}qs_pb_msg_sensor_5sec_info;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    int32_t sequence;
    int32_t on_off_bed;
    int32_t heartbeat;
    int32_t breath_rate;
    int32_t Mmin;
    int32_t Mmean;
    int32_t NSD;
    int32_t Npd;
    int32_t SBP;
    int32_t DBP;
}qs_pb_msg_sensor_1min_info;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    int32_t sequence;
    int32_t status_flag;
}qs_pb_msg_sleep_apnea_info;


typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    int32_t sequence;
    int32_t back;
}qs_pb_msg_sleep_apnea_back_info;


typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    int32_t cal_result;
    int32_t start_time;
    int32_t total_sleep_time;
    int32_t sleep_efficiency;
    int32_t sleep_quality;
    int32_t turnover_times;
    int32_t sleep_latency;
    int32_t off_bed_times;
    int32_t cRSD;
    int32_t slop1;
    int32_t slop2;
    int32_t oSA_times;
    int32_t Ave_SA_time;
    int32_t Longest_SA_time;
} qs_pb_msg_sleep_cycle_repo;


typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t data[QS_PB_RAW_DATA_LEGNTH];
    int32_t data_len;
} qs_pb_msg_state_raw_data;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t data[QS_PB_RAW_DATA_LEGNTH];
    int32_t data_len;
} qs_pb_msg_heartbeat_raw_data;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t data[QS_PB_RAW_DATA_LEGNTH];
    int32_t data_len;
} qs_pb_msg_breathrate_raw_data;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t data[QS_PB_RAW_DATA_LEGNTH];
    int32_t data_len;
} qs_pb_msg_movement_raw_data;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t data[QS_PB_RAW_DATA_LEGNTH];
    int32_t data_len;
} qs_pb_msg_snore_raw_data;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t data[QS_PB_RAW_DATA_LEGNTH];
    int32_t data_len;
} qs_pb_msg_sbp_raw_data;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t data[QS_PB_RAW_DATA_LEGNTH];
    int32_t data_len;
} qs_pb_msg_dbp_raw_data;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t cmd[QS_PB_BED_CONTRL_SIZE];
} qs_pb_msg_bed_cmd;

typedef struct {
    char device_id[QS_PB_DEVICE_ID_SIZE];
    int32_t timestamp;
    uint8_t stat[QS_PB_BED_CONTRL_SIZE];
    int32_t stat_len;
} qs_pb_msg_bed_status;


qs_ret_code_t qs_pb_ver_post_encode(qs_pb_msg_ver_post *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_ver_post_decode(char *p_encoded_msg, size_t len, qs_pb_msg_ver_post *p_obj);
qs_ret_code_t qs_pb_token_post_encode(qs_pb_msg_token_post *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_token_post_decode(char *p_encoded_msg, size_t len, qs_pb_msg_token_post *p_obj);
qs_ret_code_t qs_pb_download_range_post_encode(qs_pb_msg_download_range_post *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_download_range_post_decode(char *p_encoded_msg, size_t len, qs_pb_msg_download_range_post *p_obj);

qs_ret_code_t qs_pb_cli_command_encode(qs_pb_msg_cli_command *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_cli_command_decode(char *p_encoded_msg, size_t len, qs_pb_msg_cli_command *p_obj);

qs_ret_code_t qs_pb_sensor_5sec_info_encode(qs_pb_msg_sensor_5sec_info *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_sensor_5sec_info_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sensor_5sec_info *p_obj);
qs_ret_code_t qs_pb_sensor_1min_info_encode(qs_pb_msg_sensor_1min_info *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_sensor_1min_info_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sensor_1min_info *p_obj);

qs_ret_code_t qs_pb_sleep_apnea_info_encode(qs_pb_msg_sleep_apnea_info *p_obj, char *p_encoded_msg, size_t *p_len); // xinzeng
qs_ret_code_t qs_pb_sleep_apnea_info_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sleep_apnea_info *p_obj);
qs_ret_code_t qs_pb_sleep_apnea_back_info_encode(qs_pb_msg_sleep_apnea_back_info *p_obj, char *p_encoded_msg, size_t *p_len);// xingzeng
qs_ret_code_t qs_pb_sleep_apnea_back_info_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sleep_apnea_back_info *p_obj);

qs_ret_code_t qs_pb_sleep_cycle_repo_encode(qs_pb_msg_sleep_cycle_repo *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_sleep_cycle_repo_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sleep_cycle_repo *p_obj);

qs_ret_code_t qs_pb_state_raw_data_encode(qs_pb_msg_state_raw_data *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_state_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_state_raw_data *p_obj);
qs_ret_code_t qs_pb_heartbeat_raw_data_encode(qs_pb_msg_heartbeat_raw_data *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_heartbeat_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_heartbeat_raw_data *p_obj);
qs_ret_code_t qs_pb_breathrate_raw_data_encode(qs_pb_msg_breathrate_raw_data *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_breathrate_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_breathrate_raw_data *p_obj);
qs_ret_code_t qs_pb_movement_raw_data_encode(qs_pb_msg_movement_raw_data *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_movement_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_movement_raw_data *p_obj);
qs_ret_code_t qs_pb_snore_raw_data_encode(qs_pb_msg_snore_raw_data *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_snore_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_snore_raw_data *p_obj);
qs_ret_code_t qs_pb_sbp_raw_data_encode(qs_pb_msg_sbp_raw_data *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_sbp_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sbp_raw_data *p_obj);
qs_ret_code_t qs_pb_dbp_raw_data_encode(qs_pb_msg_dbp_raw_data *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_dbp_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_dbp_raw_data *p_obj);

qs_ret_code_t qs_pb_bed_cmd_encode(qs_pb_msg_bed_cmd *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_bed_cmd_decode(char *p_encoded_msg, size_t len, qs_pb_msg_bed_cmd *p_obj);
qs_ret_code_t qs_pb_bed_status_encode(qs_pb_msg_bed_status *p_obj, char *p_encoded_msg, size_t* p_len);
qs_ret_code_t qs_pb_bed_status_decode(char *p_encoded_msg, size_t len, qs_pb_msg_bed_status *p_obj);


