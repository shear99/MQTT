#include "mqtt_subscriber.h"

// 설정 파일에서 설정값 읽어오기
int load_config_from_file(MQTTConfig *config, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Cannot open config file '%s'\n", filename);
        return -1;
    }
    
    char line[MAX_STRING_LEN];
    int loaded_count = 0;
    
    // 기본값 설정
    memset(config, 0, sizeof(MQTTConfig));
    
    while (fgets(line, sizeof(line), file)) {
        // 개행 문자 제거
        line[strcspn(line, "\n")] = '\0';
        
        // 빈 줄이나 주석 줄 건너뛰기
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        // key=value 형태로 파싱
        char *delimiter = strchr(line, '=');
        if (delimiter == NULL) {
            continue;
        }
        
        *delimiter = '\0';
        char *key = line;
        char *value = delimiter + 1;
        
        // 앞뒤 공백 제거
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        // 설정값 저장
        if (strcmp(key, "endpoint") == 0) {
            strncpy(config->endpoint, value, MAX_STRING_LEN - 1);
            loaded_count++;
        } else if (strcmp(key, "port") == 0) {
            config->port = atoi(value);
            loaded_count++;
        } else if (strcmp(key, "cert_file") == 0) {
            strncpy(config->cert_file, value, MAX_STRING_LEN - 1);
            loaded_count++;
        } else if (strcmp(key, "private_key_file") == 0) {
            strncpy(config->private_key_file, value, MAX_STRING_LEN - 1);
            loaded_count++;
        } else if (strcmp(key, "root_ca_file") == 0) {
            strncpy(config->root_ca_file, value, MAX_STRING_LEN - 1);
            loaded_count++;
        } else if (strcmp(key, "client_id") == 0) {
            strncpy(config->client_id, value, MAX_STRING_LEN - 1);
            loaded_count++;
        } else if (strcmp(key, "topic_file") == 0) {
            strncpy(config->topic_file, value, MAX_STRING_LEN - 1);
            loaded_count++;
        } else if (strcmp(key, "qos") == 0) {
            config->qos = atoi(value);
            loaded_count++;
        } else if (strcmp(key, "keep_alive_interval") == 0) {
            config->keep_alive_interval = atoi(value);
            loaded_count++;
        } else if (strcmp(key, "timeout") == 0) {
            config->timeout = atoi(value);
            loaded_count++;
        }
    }
    
    fclose(file);
    printf("Loaded %d configuration parameters from '%s'\n", loaded_count, filename);
    return loaded_count;
}

// 토픽 파일에서 토픽 목록 읽어오기
int load_topics_from_file(TopicList *topic_list, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Cannot open topic file '%s'\n", filename);
        return -1;
    }
    
    topic_list->count = 0;
    char line[MAX_TOPIC_LEN];
    
    while (fgets(line, sizeof(line), file) && topic_list->count < MAX_TOPICS) {
        // 개행 문자 제거
        line[strcspn(line, "\n")] = '\0';
        
        // 빈 줄이나 #으로 시작하는 주석 줄 건너뛰기
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        // 토픽 유효성 검사
        if (!validate_topic_format(line)) {
            printf("Warning: Invalid topic format skipped: %s\n", line);
            continue;
        }
        
        // 토픽 복사
        strncpy(topic_list->topics[topic_list->count], line, MAX_TOPIC_LEN - 1);
        topic_list->topics[topic_list->count][MAX_TOPIC_LEN - 1] = '\0';
        topic_list->count++;
        
        printf("Loaded topic: %s\n", line);
    }
    
    fclose(file);
    printf("Total %d topics loaded from file\n", topic_list->count);
    return topic_list->count;
}

// 토픽 형식 유효성 검사
int validate_topic_format(const char *topic) {
    if (!topic || strlen(topic) == 0) {
        return 0;
    }
    
    // 최대 길이 체크
    if (strlen(topic) >= MAX_TOPIC_LEN) {
        return 0;
    }
    
    // 기본적인 MQTT 토픽 규칙 검사
    // 1. NULL 문자 포함 불가
    // 2. UTF-8 문자여야 함 (여기서는 기본 ASCII 체크)
    for (int i = 0; topic[i] != '\0'; i++) {
        char c = topic[i];
        // 제어 문자 체크 (0-31, 127)
        if (c < 32 && c != 9) { // 탭(9)은 허용
            return 0;
        }
    }
    
    // 와일드카드 위치 검사 (+ 와 # 는 특정 위치에만 올 수 있음)
    char *plus_pos = strchr(topic, '+');
    char *hash_pos = strchr(topic, '#');
    
    if (plus_pos != NULL) {
        // + 앞뒤에는 / 또는 문자열 끝이어야 함
        if (plus_pos != topic && *(plus_pos - 1) != '/') {
            return 0;
        }
        if (*(plus_pos + 1) != '\0' && *(plus_pos + 1) != '/') {
            return 0;
        }
    }
    
    if (hash_pos != NULL) {
        // # 는 토픽의 마지막에만 올 수 있음
        if (*(hash_pos + 1) != '\0') {
            return 0;
        }
        // # 앞에는 / 또는 문자열 시작이어야 함
        if (hash_pos != topic && *(hash_pos - 1) != '/') {
            return 0;
        }
    }
    
    return 1; // 유효한 토픽
}

// 여러 토픽 구독 함수
int subscribe_to_topics(MQTTClient client, TopicList *topic_list, int qos) {
    int success_count = 0;
    
    for (int i = 0; i < topic_list->count; i++) {
        int rc = MQTTClient_subscribe(client, topic_list->topics[i], qos);
        if (rc != MQTTCLIENT_SUCCESS) {
            printf("Failed to subscribe to topic '%s', return code %d\n", 
                   topic_list->topics[i], rc);
        } else {
            printf("Successfully subscribed to topic: %s\n", topic_list->topics[i]);
            success_count++;
        }
    }
    
    return success_count;
}

// 설정 정보 출력
void print_config(const MQTTConfig *config) {
    printf("\n=== MQTT Configuration ===\n");
    printf("Endpoint: %s:%d\n", config->endpoint, config->port);
    printf("Client ID: %s\n", config->client_id);
    printf("Topic File: %s\n", config->topic_file);
    printf("QoS: %d\n", config->qos);
    printf("Keep Alive: %d seconds\n", config->keep_alive_interval);
    printf("Timeout: %d ms\n", config->timeout);
    printf("Certificates:\n");
    printf("  - Root CA: %s\n", config->root_ca_file);
    printf("  - Client Cert: %s\n", config->cert_file);
    printf("  - Private Key: %s\n", config->private_key_file);
    printf("===========================\n\n");
}