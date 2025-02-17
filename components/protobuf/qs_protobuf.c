/*
 * @Author: wayne cao 
 * @Date: 2021-08-25 14:24:14 
 * @Last Modified by: wayne cao
 * @Last Modified time: 2021-08-25 16:32:26
 * 
 * @Date: 2025-01-15 20:30:15
 * @Last Modified by: fzh
 * @Last Modified time: 2025-01-15 20:30:15
 */
#include <stdint.h>
#include <string.h>
#include "pb_encode.h"
#include "pb_decode.h"
#include "keesoncloud.pb.h"

#include "qs_protobuf.h"

typedef struct
{
    int      count;
    void     *p_data;
} cursor2_t;

static bool string_read(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    int i = 0;
    cursor2_t *p_cursor_self = (cursor2_t *)*arg;
    char *p_val = (char *)p_cursor_self->p_data;
    while (stream->bytes_left) {
        uint64_t value;
        if (!pb_decode_varint(stream, &value))
            return false;
        *(p_val + i) = value;
        i++;
        p_cursor_self->count++;
    }
    return true;
}
static bool string_write(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    char *str = (char *)*arg;
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}
static bool bytes_write(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint8_t *p_data = (uint8_t *)*arg;
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    return pb_encode_string(stream, (uint8_t*)p_data + 4, *(uint32_t *)p_data);
}

static bool bytes_read(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    int i = 0;
    cursor2_t *p_cursor_self = (cursor2_t *)*arg;
    char *p_val = (char *)p_cursor_self->p_data;
    while (stream->bytes_left) {
        uint8_t byte;
        if (!pb_read(stream, (pb_byte_t *)&byte, 1))
            return false;
        *(p_val + i) = byte;
        i++;
        p_cursor_self->count++;
    }
    return true;
}

#define STRING_DECODE(_PARAM1, _PARAM2, _MSG, _OBJ_PTR) \
cursor2_t cursor_##_PARAM1;                             \
cursor_##_PARAM1.count = 0;                             \
cursor_##_PARAM1.p_data = _OBJ_PTR->_PARAM1;            \
_MSG._PARAM2.funcs.decode = string_read;                 \
_MSG._PARAM2.arg = &cursor_##_PARAM1;

#define STRING_ENCODE(_PARAM1, _PARAM2, _MSG, _OBJ_PTR) \
_MSG._PARAM2.funcs.encode = string_write;               \
_MSG._PARAM2.arg = _OBJ_PTR->_PARAM1;

#define BYTES_ENCODE(_PARAM1, _PARAM2, _MSG, _OBJ_PTR) \
_MSG._PARAM2.funcs.encode = bytes_write;               \
_MSG._PARAM2.arg = _OBJ_PTR->_PARAM1;

#define BYTES_DECODE(_PARAM1, _PARAM2, _MSG, _OBJ_PTR) \
cursor2_t cursor_##_PARAM1;                             \
cursor_##_PARAM1.count = 0;                             \
cursor_##_PARAM1.p_data = _OBJ_PTR->_PARAM1;            \
_MSG._PARAM2.funcs.decode = bytes_read;                 \
_MSG._PARAM2.arg = &cursor_##_PARAM1;

qs_ret_code_t qs_pb_ver_post_encode(qs_pb_msg_ver_post *p_obj, char *p_encoded_msg, size_t* p_len)
{
    VersionPostMessage msg = VersionPostMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.softwareMajorVersion = p_obj->software_major_ver;
    msg.softwareMinorVersion = p_obj->software_minor_ver;
    msg.softwareReviseVersion = p_obj->software_revise_ver;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, VersionPostMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_ver_post_decode(char *p_encoded_msg, size_t len, qs_pb_msg_ver_post *p_obj)
{
    VersionPostMessage msg = VersionPostMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, VersionPostMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->software_major_ver = msg.softwareMajorVersion;
    p_obj->software_minor_ver = msg.softwareMinorVersion;
    p_obj->software_revise_ver = msg.softwareReviseVersion;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_token_post_encode(qs_pb_msg_token_post *p_obj, char *p_encoded_msg, size_t* p_len)
{
    TokenPostMessage msg = TokenPostMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.softwareMajorVersion = p_obj->software_major_ver;
    msg.softwareMinorVersion = p_obj->software_minor_ver;
    msg.softwareReviseVersion = p_obj->software_revise_ver;
    msg.firmwareLenth = p_obj->firmware_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, TokenPostMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_token_post_decode(char *p_encoded_msg, size_t len, qs_pb_msg_token_post *p_obj)
{
    TokenPostMessage msg = TokenPostMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, TokenPostMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->software_major_ver = msg.softwareMajorVersion;
    p_obj->software_minor_ver = msg.softwareMinorVersion;
    p_obj->software_revise_ver = msg.softwareReviseVersion;
    p_obj->firmware_len = msg.firmwareLenth;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_download_range_post_encode(qs_pb_msg_download_range_post *p_obj, char *p_encoded_msg, size_t* p_len)
{
    DownloadRangePostMessage msg = DownloadRangePostMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.firmwareLenth = p_obj->firmware_len;
    msg.firmwareStartOffset = p_obj->firmware_start_offset;
    msg.firmwareEndOffset = p_obj->firmware_end_offset;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, DownloadRangePostMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_download_range_post_decode(char *p_encoded_msg, size_t len, qs_pb_msg_download_range_post *p_obj)
{
    DownloadRangePostMessage msg = DownloadRangePostMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, DownloadRangePostMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->firmware_len = msg.firmwareLenth;
    p_obj->firmware_start_offset = msg.firmwareStartOffset;
    p_obj->firmware_end_offset = msg.firmwareEndOffset;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}


qs_ret_code_t qs_pb_cli_command_encode(qs_pb_msg_cli_command *p_obj, char *p_encoded_msg, size_t* p_len)
{
    CliCommand msg = CliCommand_init_zero;
    msg.timestamp = p_obj->timestamp;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    STRING_ENCODE(command, command, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, CliCommand_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_cli_command_decode(char *p_encoded_msg, size_t len, qs_pb_msg_cli_command *p_obj)
{
    CliCommand msg = CliCommand_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj); 
    STRING_DECODE(command, command, msg, p_obj);
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, CliCommand_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}


qs_ret_code_t qs_pb_sleep_cycle_repo_encode(qs_pb_msg_sleep_cycle_repo *p_obj, char *p_encoded_msg, size_t* p_len)
{
    SleepCycleRepoMessage msg = SleepCycleRepoMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.calResult = p_obj->cal_result;
    msg.startTime = p_obj->start_time;
    msg.totalSleepTime = p_obj->total_sleep_time;
    msg.offBedTimes = p_obj->off_bed_times;
    msg.turnoverTimes = p_obj->turnover_times;
    msg.oSATimes = p_obj->oSA_times;
    msg.slop1 = p_obj->slop1;
    msg.slop2 = p_obj->slop2;
    msg.cRSD = p_obj->cRSD;
    msg.sleepEfficiency = p_obj->sleep_efficiency;
    msg.sleepLatency = p_obj->sleep_latency;
    msg.sleepQuality = p_obj->sleep_quality;
    msg.Ave_SA_time = p_obj->Ave_SA_time;
    msg.Longest_SA_time = p_obj->Longest_SA_time;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, SleepCycleRepoMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_sleep_cycle_repo_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sleep_cycle_repo *p_obj)
{
    SleepCycleRepoMessage msg = SleepCycleRepoMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, SleepCycleRepoMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->cal_result = msg.calResult;
    p_obj->start_time = msg.startTime;
    p_obj->total_sleep_time = msg.totalSleepTime;
    p_obj->off_bed_times = msg.offBedTimes;
    p_obj->turnover_times = msg.turnoverTimes;
    p_obj->oSA_times = msg.oSATimes;
    p_obj->slop1 = msg.slop1;
    p_obj->slop2 = msg.slop2;
    p_obj->cRSD = msg.cRSD;
    p_obj->sleep_efficiency = msg.sleepEfficiency;
    p_obj->sleep_latency = msg.sleepLatency;
    p_obj->sleep_quality = msg.sleepQuality;
    p_obj->Ave_SA_time = msg.Ave_SA_time;
    p_obj->Longest_SA_time = msg.Longest_SA_time;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}


//////////////////////////////////////sleep cycle raw data report///////////////////////////////////////////////////
qs_ret_code_t qs_pb_state_raw_data_encode(qs_pb_msg_state_raw_data *p_obj, char *p_encoded_msg, size_t* p_len)
{
    StateRawDataMessage msg = StateRawDataMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.data_len = p_obj->data_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(data, data, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, StateRawDataMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_state_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_state_raw_data *p_obj)
{
    StateRawDataMessage msg = StateRawDataMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj); 
    BYTES_DECODE(data, data, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, StateRawDataMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->data_len = msg.data_len;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}


qs_ret_code_t qs_pb_sensor_5sec_info_encode(qs_pb_msg_sensor_5sec_info *p_obj, char *p_encoded_msg, size_t* p_len)
{
    Sensor5secMessage msg = Sensor5secMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
//qs_delay_ms(1);  //QS_LOG_INFO("test1");
    msg.sequence = p_obj->sequence;
//QS_LOG_INFO("test2");
    msg.heartbeat = p_obj->heartbeat;
//qs_delay_ms(1);  //QS_LOG_INFO("test3");
    msg.breathRate = p_obj->breathRate;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(status, status, msg, p_obj); 
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, Sensor5secMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_sensor_5sec_info_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sensor_5sec_info *p_obj)
{
    Sensor5secMessage msg = Sensor5secMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj); 
    BYTES_DECODE(status, status, msg, p_obj);
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, Sensor5secMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->sequence = msg.sequence;
    p_obj->heartbeat = msg.heartbeat;
    p_obj->breathRate = msg.breathRate;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_sensor_1min_info_encode(qs_pb_msg_sensor_1min_info *p_obj, char *p_encoded_msg, size_t* p_len)
{
    Sensor1minMessage msg = Sensor1minMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
//qs_delay_ms(1);  //QS_LOG_INFO("test2");
    msg.sequence = p_obj->sequence;
//qs_delay_ms(1);  //QS_LOG_INFO("test3");
    msg.onOffBed = p_obj->on_off_bed;
//qs_delay_ms(1);  //QS_LOG_INFO("test4");
    msg.heartbeat = p_obj->heartbeat;
//qs_delay_ms(1);  //QS_LOG_INFO("test5");
    msg.breathRate = p_obj->breath_rate;
//qs_delay_ms(1);  //QS_LOG_INFO("test6");
    msg.Mmin = p_obj->Mmin;
//qs_delay_ms(1);  //QS_LOG_INFO("test7");
    msg.Mmean = p_obj->Mmean;
//qs_delay_ms(1);  //QS_LOG_INFO("test8");
    msg.NSD = p_obj->NSD;
//qs_delay_ms(1);  //QS_LOG_INFO("test9");
    msg.Npd = p_obj->Npd;
//qs_delay_ms(1);  //QS_LOG_INFO("test10");
    msg.SBP = p_obj->SBP;
//qs_delay_ms(1);  //QS_LOG_INFO("test11");
    msg.DBP = p_obj->DBP;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, Sensor1minMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_sensor_1min_info_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sensor_1min_info *p_obj)
{
    Sensor1minMessage msg = Sensor1minMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, Sensor1minMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->sequence = msg.sequence;
    p_obj->on_off_bed = msg.onOffBed;
    p_obj->heartbeat = msg.heartbeat;
    p_obj->breath_rate = msg.breathRate;
    p_obj->Mmin = msg.Mmin;
    p_obj->Mmean = msg.Mmean;
    p_obj->NSD = msg.NSD;
    p_obj->Npd = msg.Npd;
    p_obj->SBP = msg.SBP;
    p_obj->DBP = msg.DBP;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_sleep_apnea_info_encode(qs_pb_msg_sleep_apnea_info *p_obj, char *p_encoded_msg, size_t *p_len)
{
    SleepApneaMessage msg = SleepApneaMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.sequence = p_obj->sequence;
    msg.status_flag = p_obj->status_flag;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, SleepApneaMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_sleep_apnea_info_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sleep_apnea_info *p_obj)
{
    SleepApneaMessage msg = SleepApneaMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, SleepApneaMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->sequence = msg.sequence;
    p_obj->status_flag = msg.status_flag;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_sleep_apnea_back_info_encode(qs_pb_msg_sleep_apnea_back_info *p_obj, char *p_encoded_msg, size_t *p_len)
{
    SleepApneaBackMessage msg = SleepApneaBackMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.sequence = p_obj->sequence;
    msg.back = p_obj->back;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, SleepApneaBackMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_sleep_apnea_back_info_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sleep_apnea_back_info *p_obj)
{
    SleepApneaBackMessage msg = SleepApneaBackMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, SleepApneaBackMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->sequence = msg.sequence;
    p_obj->back = msg.back;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_heartbeat_raw_data_encode(qs_pb_msg_heartbeat_raw_data *p_obj, char *p_encoded_msg, size_t* p_len)
{
    HeartbeatRawDataMessage msg = HeartbeatRawDataMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.data_len = p_obj->data_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(data, data, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, HeartbeatRawDataMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_heartbeat_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_heartbeat_raw_data *p_obj)
{
    HeartbeatRawDataMessage msg = HeartbeatRawDataMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    BYTES_DECODE(data, data, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, HeartbeatRawDataMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->data_len = msg.data_len;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_breathrate_raw_data_encode(qs_pb_msg_breathrate_raw_data *p_obj, char *p_encoded_msg, size_t* p_len)
{
    BreathRawDataMessage msg = BreathRawDataMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.data_len = p_obj->data_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(data, data, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, BreathRawDataMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_breathrate_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_breathrate_raw_data *p_obj)
{
    BreathRawDataMessage msg = BreathRawDataMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    BYTES_DECODE(data, data, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, BreathRawDataMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->data_len = msg.data_len;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_movement_raw_data_encode(qs_pb_msg_movement_raw_data *p_obj, char *p_encoded_msg, size_t* p_len)
{
    MovementRawDataMessage msg = MovementRawDataMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.data_len = p_obj->data_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(data, data, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, MovementRawDataMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_movement_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_movement_raw_data *p_obj)
{
    MovementRawDataMessage msg = MovementRawDataMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    BYTES_DECODE(data, data, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, MovementRawDataMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->data_len = msg.data_len;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_snore_raw_data_encode(qs_pb_msg_snore_raw_data *p_obj, char *p_encoded_msg, size_t* p_len)
{
    SnoreRawDataMessage msg = SnoreRawDataMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.data_len = p_obj->data_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(data, data, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, SnoreRawDataMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_snore_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_snore_raw_data *p_obj)
{
    SnoreRawDataMessage msg = SnoreRawDataMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    BYTES_DECODE(data, data, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, SnoreRawDataMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->data_len = msg.data_len;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_sbp_raw_data_encode(qs_pb_msg_sbp_raw_data *p_obj, char *p_encoded_msg, size_t* p_len)
{
    SbpRawDataMessage msg = SbpRawDataMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.data_len = p_obj->data_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(data, data, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, SbpRawDataMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_sbp_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_sbp_raw_data *p_obj)
{
    SbpRawDataMessage msg = SbpRawDataMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    BYTES_DECODE(data, data, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, SbpRawDataMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->data_len = msg.data_len;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_dbp_raw_data_encode(qs_pb_msg_dbp_raw_data *p_obj, char *p_encoded_msg, size_t* p_len)
{
    DbpRawDataMessage msg = DbpRawDataMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.data_len = p_obj->data_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(data, data, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, DbpRawDataMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_dbp_raw_data_decode(char *p_encoded_msg, size_t len, qs_pb_msg_dbp_raw_data *p_obj)
{
    DbpRawDataMessage msg = DbpRawDataMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    BYTES_DECODE(data, data, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, DbpRawDataMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->data_len = msg.data_len;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_bed_cmd_encode(qs_pb_msg_bed_cmd *p_obj, char *p_encoded_msg, size_t* p_len)
{
    BedCommandMessage msg = BedCommandMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(cmd, cmd, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, BedCommandMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_bed_cmd_decode(char *p_encoded_msg, size_t len, qs_pb_msg_bed_cmd *p_obj)
{
    BedCommandMessage msg = BedCommandMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    BYTES_DECODE(cmd, cmd, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, BedCommandMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}

qs_ret_code_t qs_pb_bed_status_encode(qs_pb_msg_bed_status *p_obj, char *p_encoded_msg, size_t* p_len)
{
    BedStatusMessage msg = BedStatusMessage_init_zero;
    msg.timestamp = p_obj->timestamp;
    msg.stat_len = p_obj->stat_len;
    STRING_ENCODE(device_id, deviceID, msg, p_obj);
    BYTES_ENCODE(stat, stat, msg, p_obj);
    pb_ostream_t ostream = pb_ostream_from_buffer((pb_byte_t *)p_encoded_msg, *p_len);
    bool status = pb_encode(&ostream, BedStatusMessage_fields, &msg);
    *p_len = ostream.bytes_written;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
qs_ret_code_t qs_pb_bed_status_decode(char *p_encoded_msg, size_t len, qs_pb_msg_bed_status *p_obj)
{
    BedStatusMessage msg = BedStatusMessage_init_zero;
    STRING_DECODE(device_id, deviceID, msg, p_obj);
    BYTES_DECODE(stat, stat, msg, p_obj); 
    pb_istream_t istream = pb_istream_from_buffer((pb_byte_t *)p_encoded_msg, len);
    bool status = pb_decode(&istream, BedStatusMessage_fields, &msg);
    p_obj->timestamp = msg.timestamp;
    p_obj->stat_len = msg.stat_len;
    return status ? QS_SUCCESS : QS_ERROR_INTERNAL;
}
