#ifndef AIOT_MQTT_AIGN_H_
#define AIOT_MQTT_AIGN_H_







int aiotMqttSign(const char *productKey, const char *deviceName, const char *deviceSecret,
                    char clientId[150], char username[64], char password[65]);




#endif 