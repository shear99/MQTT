#include "mqtt_subscriber.h"

// 토픽 계층 구조 파싱
ParsedTopic parse_topic_hierarchy(const char *topic_name) {
    ParsedTopic result;
    memset(&result, 0, sizeof(ParsedTopic));
    
    if (!topic_name) {
        result.is_valid = 0;
        return result;
    }
    
    // 토픽명을 복사하여 안전하게 파싱
    int topic_len = strlen(topic_name);
    char *topic_copy = malloc(topic_len + 1);
    if (!topic_copy) {
        result.is_valid = 0;
        return result;
    }
    strcpy(topic_copy, topic_name);
    
    // 토픽 hierarchy 파싱 (device_id/target_device/command)
    char *device_id = strtok(topic_copy, "/");
    char *target_device = strtok(NULL, "/");
    char *command = strtok(NULL, "/");
    
    if (device_id && target_device && command) {
        strncpy(result.device_id, device_id, sizeof(result.device_id) - 1);
        strncpy(result.target_device, target_device, sizeof(result.target_device) - 1);
        strncpy(result.command, command, sizeof(result.command) - 1);
        result.is_valid = 1;
    } else {
        result.is_valid = 0;
    }
    
    free(topic_copy);
    return result;
}

// 메시지 페이로드 파싱
ParsedMessage parse_message_payload(const char *payload, int payload_len) {
    ParsedMessage result;
    memset(&result, 0, sizeof(ParsedMessage));
    
    if (!payload || payload_len <= 0) {
        return result;
    }
    
    // 페이로드 크기 체크
    if (payload_len > MAX_PAYLOAD_SIZE) {
        printf("Warning: Payload too large (%d bytes), skipping parsing\n", payload_len);
        return result;
    }
    
    // JSON 파싱을 위해 페이로드 문자열을 널 종료
    char *json_string = malloc(payload_len + 1);
    if (!json_string) {
        printf("Failed to allocate memory for JSON string\n");
        return result;
    }
    
    memcpy(json_string, payload, payload_len);
    json_string[payload_len] = '\0';
    
    // JSON 파싱 시도
    cJSON *root = cJSON_Parse(json_string);
    if (root != NULL) {
        result.is_json = 1;
        
        // "message" 필드 확인
        cJSON *message_item = cJSON_GetObjectItemCaseSensitive(root, "message");
        if (cJSON_IsString(message_item) && (message_item->valuestring != NULL)) {
            strncpy(result.message, message_item->valuestring, sizeof(result.message) - 1);
            result.has_message = 1;
        }
        
        // "value" 필드 확인
        cJSON *value_item = cJSON_GetObjectItemCaseSensitive(root, "value");
        if (cJSON_IsString(value_item) && (value_item->valuestring != NULL)) {
            strncpy(result.value, value_item->valuestring, sizeof(result.value) - 1);
            result.has_value = 1;
        } else if (cJSON_IsNumber(value_item)) {
            snprintf(result.value, sizeof(result.value), "%d", value_item->valueint);
            result.has_value = 1;
        }
        
        // "status" 필드 확인
        cJSON *status_item = cJSON_GetObjectItemCaseSensitive(root, "status");
        if (cJSON_IsString(status_item) && (status_item->valuestring != NULL)) {
            strncpy(result.status, status_item->valuestring, sizeof(result.status) - 1);
            result.has_status = 1;
        }
        
        cJSON_Delete(root);
    } else {
        // JSON이 아닌 경우 원본 텍스트로 처리
        result.is_json = 0;
        strncpy(result.message, json_string, sizeof(result.message) - 1);
        result.has_message = 1;
    }
    
    free(json_string);
    return result;
}

// 메시지 정보 출력
void print_message_info(const ParsedTopic *topic_info, const ParsedMessage *msg_info) {
    printf("\n=== Message received ===\n");
    
    if (topic_info->is_valid) {
        printf("Device ID: %s\n", topic_info->device_id);
        printf("Target Device: %s\n", topic_info->target_device);
        printf("Command: %s\n", topic_info->command);
        printf("Processing: Control '%s' on device '%s' with command '%s'\n", 
               topic_info->target_device, topic_info->device_id, topic_info->command);
    } else {
        printf("Warning: Topic does not follow expected format (device_id/target_device/command)\n");
        printf("Expected format example: raspberry_001/led/on\n");
    }
    
    if (msg_info->is_json) {
        printf("Payload Type: JSON\n");
        if (msg_info->has_message) {
            printf("Message: %s\n", msg_info->message);
        }
        if (msg_info->has_value) {
            printf("Value: %s\n", msg_info->value);
        }
        if (msg_info->has_status) {
            printf("Status: %s\n", msg_info->status);
        }
    } else {
        printf("Payload Type: Plain Text\n");
        if (msg_info->has_message) {
            printf("Content: %s\n", msg_info->message);
        }
    }
    
    printf("========================\n");
}

// 메시지 수신 콜백 함수
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    // NULL 체크
    if (!message || !topicName) {
        printf("Error: NULL message or topic received\n");
        return 1;
    }
    
    printf("Topic: %s\n", topicName);
    
    // 토픽 파싱
    ParsedTopic topic_info = parse_topic_hierarchy(topicName);
    
    // 메시지 파싱
    ParsedMessage msg_info;
    memset(&msg_info, 0, sizeof(ParsedMessage));
    
    if (message->payloadlen > 0 && message->payload) {
        msg_info = parse_message_payload((char*)message->payload, message->payloadlen);
        printf("Raw payload (%d bytes): %.*s\n", 
               message->payloadlen, message->payloadlen, (char*)message->payload);
    } else {
        printf("Empty payload received\n");
    }
    
    // 정보 출력
    print_message_info(&topic_info, &msg_info);
    
    // 여기에 실제 디바이스 제어 로직을 추가할 수 있음
    if (topic_info.is_valid) {
        // 예: LED 제어
        if (strcmp(topic_info.target_device, "led") == 0) {
            if (strcmp(topic_info.command, "on") == 0) {
                printf("Action: LED ON command processed for device %s\n", topic_info.device_id);
            } else if (strcmp(topic_info.command, "off") == 0) {
                printf("Action: LED OFF command processed for device %s\n", topic_info.device_id);
            }
        }
        // 예: 센서 읽기
        else if (strcmp(topic_info.target_device, "sensor") == 0) {
            if (strcmp(topic_info.command, "read") == 0) {
                printf("Action: Sensor read command processed for device %s\n", topic_info.device_id);
            }
        }
    }
    
    return 1; // 성공적으로 처리됨
}

// 연결 해제 콜백 함수
void connectionLost(void *context, char *cause) {
    printf("\n!!! Connection lost !!!\n");
    if (cause) {
        printf("Cause: %s\n", cause);
    }
    printf("Attempting to reconnect...\n");
}

// 리소스 정리 함수
void cleanup_resources(MQTTClient *client) {
    if (client && *client) {
        MQTTClient_disconnect(*client, 10000);
        MQTTClient_destroy(client);
        *client = NULL;
    }
}