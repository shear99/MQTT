#ifndef MQTT_SUBSCRIBER_H
#define MQTT_SUBSCRIBER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MQTTClient.h"
#include <cjson/cJSON.h>

// 기본 설정값들
#define MAX_TOPICS 100
#define MAX_TOPIC_LEN 256
#define MAX_STRING_LEN 512
#define MAX_PAYLOAD_SIZE (1024 * 1024)  // 1MB

// 토픽 저장 구조체
typedef struct {
    char topics[MAX_TOPICS][MAX_TOPIC_LEN];
    int count;
} TopicList;

// MQTT 설정 구조체
typedef struct {
    char endpoint[MAX_STRING_LEN];
    int port;
    char cert_file[MAX_STRING_LEN];
    char private_key_file[MAX_STRING_LEN];
    char root_ca_file[MAX_STRING_LEN];
    char client_id[MAX_STRING_LEN];
    char topic_file[MAX_STRING_LEN];
    int qos;
    int keep_alive_interval;
    int timeout;
} MQTTConfig;

// 파싱된 토픽 정보 구조체
typedef struct {
    char prefix[64];
    char device_id[64];
    char target_device[64];
    char command[64];
    int is_valid;
} ParsedTopic;

// 파싱된 메시지 정보 구조체
typedef struct {
    char message[MAX_STRING_LEN];
    char value[MAX_STRING_LEN];
    char status[64];
    int has_message;
    int has_value;
    int has_status;
    int is_json;
} ParsedMessage;

// topic_manager.c 함수들
int load_config_from_file(MQTTConfig *config, const char *filename);
int load_topics_from_file(TopicList *topic_list, const char *filename);
int validate_topic_format(const char *topic);
int subscribe_to_topics(MQTTClient client, TopicList *topic_list, int qos);

// message_handler.c 함수들
ParsedTopic parse_topic_hierarchy(const char *topic_name);
ParsedMessage parse_message_payload(const char *payload, int payload_len);
void print_message_info(const ParsedTopic *topic_info, const ParsedMessage *msg_info);
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void connectionLost(void *context, char *cause);

// publisher 관련 코드
int pubMessageHandler(void *context, char *topicName, int topicLen, MQTTClient_message *message);

// control/led.c
void handle_led(const char *command);
// control/buzzer.c
void handle_buzzer(const char *command);
// control/s_segment.c
void handle_s_segment(const char *command);
// control/photoresistor.c
void handle_photoresistor(const char *command);

// 유틸리티 함수들
void print_config(const MQTTConfig *config);
void cleanup_resources(MQTTClient *client);

#endif // MQTT_SUBSCRIBER_H