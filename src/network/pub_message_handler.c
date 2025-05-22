#include "../mqtt.h"

static MQTTClient g_pub_client = NULL;

void set_pub_client(MQTTClient client) {
    g_pub_client = client;
}

// 토픽으로 결과 메시지 발행
void send_result_to_topic(const char *topic, const char *value) {
    if (!g_pub_client || !topic || !value) return;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void *)value;
    pubmsg.payloadlen = (int)strlen(value);
    pubmsg.qos = 1;
    pubmsg.retained = 0;
    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(g_pub_client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Publisher: Failed to publish result to topic '%s', return code %d\n", topic, rc);
    } else {
        printf("Publisher: Sent result '%s' to topic '%s'\n", value, topic);
    }
}

// publisher는 수신 메시지 처리 필요 없음
int pubMessageHandler(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    return 1;
}
