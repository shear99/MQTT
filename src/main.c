#include "./mqtt.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

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
    TopicList sub_topic_list;
    int rc;
    char url[MAX_STRING_LEN];

    // 시그널 핸들러 등록
    signal(SIGINT, signal_handler);

    // 설정 파일 로드
    const char *config_file = (argc > 1) ? argv[1] : "config.conf";
    if (load_config_from_file(&config, config_file) <= 0) {
        printf("Failed to load configuration. Exiting...\n");
        return EXIT_FAILURE;
    }
    print_config(&config);

    // 구독용 토픽 목록 로드
    if (load_topics_from_file(&sub_topic_list, config.topic_file) <= 0) {
        printf("No subscriber topics loaded. Exiting...\n");
        return EXIT_FAILURE;
    }

    // MQTT 브로커 URL 생성
    snprintf(url, sizeof(url), "ssl://%s:%d", config.endpoint, config.port);
    printf("Connecting to: %s\n", url);

    // Subscriber MQTT 클라이언트 생성
    if ((rc = MQTTClient_create(&client, url, config.client_id,
            MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Subscriber: Failed to create client, return code %d\n", rc);
        return EXIT_FAILURE;
    }
    global_client = client;

    // 공용 SSL 옵션 설정
    ssl_opts.trustStore = config.root_ca_file;
    ssl_opts.keyStore = config.cert_file;
    ssl_opts.privateKey = config.private_key_file;
    ssl_opts.enableServerCertAuth = 1;

    // 공용 연결 옵션 설정
    conn_opts.keepAliveInterval = config.keep_alive_interval;
    conn_opts.cleansession = 1;
    conn_opts.ssl = &ssl_opts;

    // fork()를 사용해 퍼블리셔와 구독자 기능 분리
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        // 자식 프로세스: Publisher 역할
        char pub_client_id[MAX_STRING_LEN];
        snprintf(pub_client_id, sizeof(pub_client_id), "%s_pub", config.client_id);
        MQTTClient pub_client;
        if ((rc = MQTTClient_create(&pub_client, url, pub_client_id,
                MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
            printf("Publisher: Failed to create client, return code %d\n", rc);
            exit(EXIT_FAILURE);
        }
        // Publisher 토픽 목록 로드 (pub_topic.txt)
        TopicList pub_topic_list;
        if (load_topics_from_file(&pub_topic_list, "pub_topic.txt") <= 0) {
            printf("Publisher: No topics loaded. Exiting...\n");
            cleanup_resources(&pub_client);
            exit(EXIT_FAILURE);
        }
        // Publisher 콜백 함수 설정 (pubMessageHandler는 /network/pub_message_handler.c에 구현)
        if ((rc = MQTTClient_setCallbacks(pub_client, NULL, connectionLost, pubMessageHandler, NULL)) != MQTTCLIENT_SUCCESS) {
            printf("Publisher: Failed to set callbacks, return code %d\n", rc);
            cleanup_resources(&pub_client);
            exit(EXIT_FAILURE);
        }
        // Publisher 연결
        if ((rc = MQTTClient_connect(pub_client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
            printf("Publisher: Failed to connect, return code %d\n", rc);
            cleanup_resources(&pub_client);
            exit(EXIT_FAILURE);
        }
        printf("Publisher connected successfully\n");

        // 퍼블리셔 메시지 발행 루프
        while (running) {
            for (int i = 0; i < pub_topic_list.count; i++) {
                MQTTClient_message pubmsg = MQTTClient_message_initializer;
                const char *payload = "Test message from publisher";
                pubmsg.payload = (char *)payload;
                pubmsg.payloadlen = (int)strlen(payload);
                pubmsg.qos = config.qos;
                pubmsg.retained = 0;
                MQTTClient_deliveryToken token;
                rc = MQTTClient_publishMessage(pub_client, pub_topic_list.topics[i], &pubmsg, &token);
                if (rc != MQTTCLIENT_SUCCESS) {
                    printf("Publisher: Failed to publish to topic '%s', return code %d\n", pub_topic_list.topics[i], rc);
                } else {
                    printf("Publisher: Published to topic '%s'\n", pub_topic_list.topics[i]);
                }
                sleep(1);
            }
        }
        cleanup_resources(&pub_client);
        exit(EXIT_SUCCESS);
    }
    // 부모 프로세스: Subscriber 역할
    if ((rc = MQTTClient_setCallbacks(client, NULL, connectionLost, messageArrived, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Subscriber: Failed to set callbacks, return code %d\n", rc);
        cleanup_resources(&client);
        return EXIT_FAILURE;
    }
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Subscriber: Failed to connect, return code %d\n", rc);
        cleanup_resources(&client);
        return EXIT_FAILURE;
    }
    printf("Subscriber connected successfully\n");

    // 토픽 구독 시작
    int subscribed_count = subscribe_to_topics(client, &sub_topic_list, config.qos);
    printf("Subscribed to %d out of %d topics\n", subscribed_count, sub_topic_list.count);
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
                subscribe_to_topics(client, &sub_topic_list, config.qos);
            } else {
                printf("Subscriber: Reconnection failed, return code %d\n", rc);
                sleep(5);
            }
        }
    }
    printf("Cleaning up resources...\n");
    cleanup_resources(&client);
    return EXIT_SUCCESS;
}