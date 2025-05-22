#include "./mqtt.h"

// 전역 변수
static MQTTClient global_client = NULL;
static volatile int running = 1;
static int msg_queue_id = -1;

// 시그널 핸들러 (Ctrl+C 처리)
void signal_handler(int signal) {
    if (signal == SIGINT) {
        printf("\nReceived SIGINT, shutting down gracefully...\n");
        running = 0;
        if (global_client) {
            cleanup_resources(&global_client);
        }
        // IPC 정리
        ipc_cleanup(msg_queue_id);
        exit(0);
    }
}

// 수정된 messageArrived 콜백 (Subscriber용) - 기존 구조 활용
int messageArrived_subscriber(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("Subscriber: Message arrived on topic '%s': %.*s\n", 
           topicName, message->payloadlen, (char*)message->payload);
    
    // 기존 토픽 파싱 함수 활용
    ParsedTopic topic_info = parse_topic_hierarchy(topicName);
    ParsedMessage msg_info = parse_message_payload(message->payload, message->payloadlen);
    
    // control 토픽인지 확인 (prefix가 "control"인지)
    if (topic_info.is_valid && strcmp(topic_info.prefix, "control") == 0) {
        // 페이로드를 문자열로 변환
        char payload[MAX_STRING_LEN] = {0};
        int payload_len = message->payloadlen;
        if (payload_len >= MAX_STRING_LEN) {
            payload_len = MAX_STRING_LEN - 1;
        }
        memcpy(payload, message->payload, payload_len);
        payload[payload_len] = '\0';
        
        // IPC를 통해 Publisher에게 제어 명령 전달
        if (ipc_send_control_message(msg_queue_id, topicName, payload) != 0) {
            printf("Subscriber: Failed to send control message via IPC\n");
        }
    }
    
    // 기존 메시지 정보 출력 함수 활용
    print_message_info(&topic_info, &msg_info);
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// Publisher 프로세스 함수
void run_publisher_process(const MQTTConfig *config, const char *url) {
    char pub_client_id[MAX_STRING_LEN];
    snprintf(pub_client_id, sizeof(pub_client_id), "%s_pub", config->client_id);
    
    MQTTClient pub_client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    int rc;
    
    // Publisher 클라이언트 생성
    if ((rc = MQTTClient_create(&pub_client, url, pub_client_id,
            MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Publisher: Failed to create client, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    // SSL 옵션 설정
    ssl_opts.trustStore = config->root_ca_file;
    ssl_opts.keyStore = config->cert_file;
    ssl_opts.privateKey = config->private_key_file;
    ssl_opts.enableServerCertAuth = 1;

    // 연결 옵션 설정
    conn_opts.keepAliveInterval = config->keep_alive_interval;
    conn_opts.cleansession = 1;
    conn_opts.ssl = &ssl_opts;

    // Publisher 콜백 함수 설정 (기존 pubMessageHandler 활용)
    if ((rc = MQTTClient_setCallbacks(pub_client, NULL, connectionLost, pubMessageHandler, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Publisher: Failed to set callbacks, return code %d\n", rc);
        cleanup_resources(&pub_client);
        exit(EXIT_FAILURE);
    }
    
    // Publisher에 set_pub_client 설정
    set_pub_client(pub_client);

    // Publisher 연결
    if ((rc = MQTTClient_connect(pub_client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Publisher: Failed to connect, return code %d\n", rc);
        cleanup_resources(&pub_client);
        exit(EXIT_FAILURE);
    }
    printf("Publisher connected successfully\n");

    // 이벤트 기반 Publisher 루프
    while (running) {
        char received_topic[MAX_TOPIC_LEN];
        char received_payload[MAX_STRING_LEN];
        
        // IPC에서 제어 명령 수신
        if (ipc_receive_control_message(msg_queue_id, received_topic, received_payload, sizeof(received_payload)) == 0) {
            printf("Publisher: Processing control command for topic '%s'\n", received_topic);
            
            // 기존 토픽 파싱 함수 활용
            ParsedTopic topic_info = parse_topic_hierarchy(received_topic);
            if (topic_info.is_valid) {
                // 기존 handle 함수들 활용
                if (strcmp(topic_info.target_device, "led") == 0) {
                    handle_led(topic_info.command);
                } else if (strcmp(topic_info.target_device, "buzzer") == 0) {
                    handle_buzzer(topic_info.command);
                } else if (strcmp(topic_info.target_device, "s_segment") == 0) {
                    // s_segment는 payload 값을 사용
                    if (strlen(received_payload) > 0) {
                        handle_s_segment(received_payload);
                    } else {
                        handle_s_segment(topic_info.command);
                    }
                } else if (strcmp(topic_info.target_device, "photoresistor") == 0) {
                    handle_photoresistor(topic_info.command);
                } else {
                    printf("Publisher: Unknown device: %s\n", topic_info.target_device);
                    
                    // 알 수 없는 디바이스에 대한 에러 응답
                    char error_topic[MAX_TOPIC_LEN];
                    char error_result[MAX_STRING_LEN];
                    
                    snprintf(error_topic, sizeof(error_topic), "status/%s/%s/return", 
                             topic_info.device_id, topic_info.target_device);
                    snprintf(error_result, sizeof(error_result), 
                             "{\"error\":\"unknown device\",\"device\":\"%s\",\"timestamp\":%ld}", 
                             topic_info.target_device, time(NULL));
                    
                    send_result_to_topic(error_topic, error_result);
                }
            } else {
                printf("Publisher: Invalid topic format: %s\n", received_topic);
            }
        }
        
        usleep(100000); // 100ms 대기
    }
    
    cleanup_resources(&pub_client);
    exit(EXIT_SUCCESS);
}

// Subscriber 프로세스 함수
int run_subscriber_process(const MQTTConfig *config, const char *url, const TopicList *sub_topic_list) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    int rc;
    
    // Subscriber 클라이언트 생성
    if ((rc = MQTTClient_create(&client, url, config->client_id,
            MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Subscriber: Failed to create client, return code %d\n", rc);
        return EXIT_FAILURE;
    }
    global_client = client;

    // SSL 옵션 설정
    ssl_opts.trustStore = config->root_ca_file;
    ssl_opts.keyStore = config->cert_file;
    ssl_opts.privateKey = config->private_key_file;
    ssl_opts.enableServerCertAuth = 1;

    // 연결 옵션 설정
    conn_opts.keepAliveInterval = config->keep_alive_interval;
    conn_opts.cleansession = 1;
    conn_opts.ssl = &ssl_opts;
    
    // 콜백 함수 설정 (기존 함수명 변경)
    if ((rc = MQTTClient_setCallbacks(client, NULL, connectionLost, messageArrived_subscriber, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Subscriber: Failed to set callbacks, return code %d\n", rc);
        cleanup_resources(&client);
        return EXIT_FAILURE;
    }
    
    // Subscriber 연결
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Subscriber: Failed to connect, return code %d\n", rc);
        cleanup_resources(&client);
        return EXIT_FAILURE;
    }
    printf("Subscriber connected successfully\n");

    // 토픽 구독 시작
    int subscribed_count = subscribe_to_topics(client, sub_topic_list, config->qos);
    printf("Subscribed to %d out of %d topics\n", subscribed_count, sub_topic_list->count);
    
    if (subscribed_count == 0) {
        printf("No topics were successfully subscribed. Exiting...\n");
        cleanup_resources(&client);
        return EXIT_FAILURE;
    }
    
    printf("Waiting for messages... (Press Ctrl+C to exit)\n");

    // 메시지 수신 대기 루프
    while (running) {
        sleep(1);
        if (!MQTTClient_isConnected(client)) {
            printf("Subscriber: Connection lost, attempting reconnection...\n");
            if ((rc = MQTTClient_connect(client, &conn_opts)) == MQTTCLIENT_SUCCESS) {
                printf("Subscriber: Reconnected successfully\n");
                subscribe_to_topics(client, sub_topic_list, config->qos);
            } else {
                printf("Subscriber: Reconnection failed, return code %d\n", rc);
                sleep(5);
            }
        }
    }
    
    printf("Cleaning up subscriber resources...\n");
    cleanup_resources(&client);
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    MQTTConfig config;
    TopicList sub_topic_list;
    char url[MAX_STRING_LEN];

    // 시그널 핸들러 등록
    signal(SIGINT, signal_handler);

    // IPC 초기화
    msg_queue_id = ipc_init();
    if (msg_queue_id == -1) {
        printf("Failed to initialize IPC. Exiting...\n");
        return EXIT_FAILURE;
    }

    // 설정 파일 로드
    const char *config_file = (argc > 1) ? argv[1] : "config.conf";
    if (load_config_from_file(&config, config_file) <= 0) {
        printf("Failed to load configuration. Exiting...\n");
        ipc_cleanup(msg_queue_id);
        return EXIT_FAILURE;
    }
    print_config(&config);

    // 구독용 토픽 목록 로드
    if (load_topics_from_file(&sub_topic_list, config.topic_file) <= 0) {
        printf("No subscriber topics loaded. Exiting...\n");
        ipc_cleanup(msg_queue_id);
        return EXIT_FAILURE;
    }

    // MQTT 브로커 URL 생성
    snprintf(url, sizeof(url), "ssl://%s:%d", config.endpoint, config.port);
    printf("Connecting to: %s\n", url);

    // fork()를 사용해 Publisher와 Subscriber 분리
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        ipc_cleanup(msg_queue_id);
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        // 자식 프로세스: Publisher 역할
        run_publisher_process(&config, url);
    } else {
        // 부모 프로세스: Subscriber 역할
        int result = run_subscriber_process(&config, url, &sub_topic_list);
        
        // 자식 프로세스 종료 대기
        printf("Waiting for publisher process to terminate...\n");
        wait(NULL);
        
        ipc_cleanup(msg_queue_id);
        return result;
    }
}