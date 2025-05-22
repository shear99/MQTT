#include "./mqtt.h"
#include <signal.h>

// 전역 변수 (시그널 핸들러에서 사용)
static MQTTClient global_client = NULL;
static volatile int running = 1;

// 시그널 핸들러 (Ctrl+C 처리)
void signal_handler(int signal) {
    if (signal == SIGINT) {
        printf("\nReceived SIGINT, shutting down gracefully...\n");
        running = 0;
        if (global_client) {
            cleanup_resources(&global_client);
        }
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    MQTTConfig config;
    TopicList topic_list;
    int rc;
    char url[MAX_STRING_LEN];
    
    // 시그널 핸들러 등록
    signal(SIGINT, signal_handler);
    
    printf("==================================================\n");
    printf("MQTT Device Control Subscriber (Modular Version)\n");
    printf("Expected topic format: device_id/target_device/command\n");
    printf("Example: raspberry_001/led/on\n");
    printf("==================================================\n\n");
    
    // 설정 파일에서 설정값 로드
    const char *config_file = (argc > 1) ? argv[1] : "config.conf";
    if (load_config_from_file(&config, config_file) <= 0) {
        printf("Failed to load configuration. Using default config file: config.conf\n");
        return EXIT_FAILURE;
    }
    
    // 설정 정보 출력
    print_config(&config);
    
    // 토픽 파일에서 토픽 목록 로드
    if (load_topics_from_file(&topic_list, config.topic_file) <= 0) {
        printf("No topics loaded. Exiting...\n");
        return EXIT_FAILURE;
    }
    
    // MQTT 브로커 URL 생성
    snprintf(url, sizeof(url), "ssl://%s:%d", config.endpoint, config.port);
    printf("Connecting to: %s\n", url);
    
    // MQTT 클라이언트 생성
    if ((rc = MQTTClient_create(&client, url, config.client_id,
        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to create client, return code %d\n", rc);
        return EXIT_FAILURE;
    }
    
    // 전역 변수에 클라이언트 저장 (시그널 핸들러용)
    global_client = client;
    
    // 콜백 함수 설정
    if ((rc = MQTTClient_setCallbacks(client, NULL, connectionLost, messageArrived, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to set callbacks, return code %d\n", rc);
        cleanup_resources(&client);
        return EXIT_FAILURE;
    }
    
    // SSL 옵션 설정
    ssl_opts.trustStore = config.root_ca_file;
    ssl_opts.keyStore = config.cert_file;
    ssl_opts.privateKey = config.private_key_file;
    ssl_opts.enableServerCertAuth = 1;
    
    // 연결 옵션 설정
    conn_opts.keepAliveInterval = config.keep_alive_interval;
    conn_opts.cleansession = 1;
    conn_opts.ssl = &ssl_opts;
    
    printf("Attempting to connect to AWS IoT Core...\n");
    
    // MQTT 브로커에 연결
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to AWS IoT Core, return code %d\n", rc);
        
        // 연결 실패 원인 분석
        switch(rc) {
            case MQTTCLIENT_FAILURE:
                printf("Reason: General failure\n");
                break;
            case MQTTCLIENT_DISCONNECTED:
                printf("Reason: Client disconnected\n");
                break;
            case MQTTCLIENT_MAX_MESSAGES_INFLIGHT:
                printf("Reason: Too many messages in flight\n");
                break;
            case MQTTCLIENT_BAD_UTF8_STRING:
                printf("Reason: Bad UTF8 string\n");
                break;
            case MQTTCLIENT_NULL_PARAMETER:
                printf("Reason: NULL parameter\n");
                break;
            case MQTTCLIENT_TOPICNAME_TRUNCATED:
                printf("Reason: Topic name truncated\n");
                break;
            case MQTTCLIENT_BAD_STRUCTURE:
                printf("Reason: Bad structure\n");
                break;
            case MQTTCLIENT_BAD_QOS:
                printf("Reason: Bad QoS\n");
                break;
            case MQTTCLIENT_SSL_NOT_SUPPORTED:
                printf("Reason: SSL not supported\n");
                break;
            default:
                printf("Reason: Unknown error code %d\n", rc);
                break;
        }
        
        cleanup_resources(&client);
        return EXIT_FAILURE;
    }
    
    printf("✓ Successfully connected to AWS IoT Core\n\n");
    
    // 모든 토픽 구독
    int subscribed_count = subscribe_to_topics(client, &topic_list, config.qos);
    printf("\n✓ Successfully subscribed to %d out of %d topics\n", 
           subscribed_count, topic_list.count);
    
    if (subscribed_count == 0) {
        printf("No topics were successfully subscribed. Exiting...\n");
        cleanup_resources(&client);
        return EXIT_FAILURE;
    }
    
    printf("\n🎯 Waiting for messages... (Press Ctrl+C to exit)\n");
    printf("📡 Listening for device control commands...\n");
    printf("==================================================\n");
    
    // 메시지 수신 대기 루프
    while (running) {
        sleep(1);
        
        // 연결 상태 확인 (선택적)
        if (!MQTTClient_isConnected(client)) {
            printf("❌ Connection lost, attempting to reconnect...\n");
            
            // 재연결 시도
            if ((rc = MQTTClient_connect(client, &conn_opts)) == MQTTCLIENT_SUCCESS) {
                printf("✓ Reconnected successfully\n");
                // 토픽 재구독
                subscribe_to_topics(client, &topic_list, config.qos);
            } else {
                printf("❌ Reconnection failed, return code %d\n", rc);
                sleep(5); // 5초 대기 후 재시도
            }
        }
    }
    
    // 정상 종료
    printf("\n🔄 Cleaning up resources...\n");
    cleanup_resources(&client);
    printf("✓ Program terminated successfully\n");
    
    return EXIT_SUCCESS;
}