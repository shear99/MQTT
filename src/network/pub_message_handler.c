#include "../mqtt.h"

// Publisher 콜백 함수 구현
int pubMessageHandler(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    // Publisher에서는 수신한 메시지를 간단히 출력 (필요시 추가 처리 가능)
    printf("Publisher Callback: Received message on topic: %s\n", topicName);
    return 1;
}
