#include "../mqtt.h"

static int g_msg_queue_id = -1;

// IPC 초기화
int ipc_init(void) {
    key_t key = ftok(".", 'M');
    if (key == -1) {
        perror("ftok failed");
        return -1;
    }
    
    g_msg_queue_id = msgget(key, IPC_CREAT | 0666);
    if (g_msg_queue_id == -1) {
        perror("msgget failed");
        return -1;
    }
    
    printf("IPC: Message queue initialized (ID: %d)\n", g_msg_queue_id);
    return g_msg_queue_id;
}

// IPC 정리
void ipc_cleanup(int msg_queue_id) {
    if (msg_queue_id != -1) {
        if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1) {
            perror("msgctl IPC_RMID failed");
        } else {
            printf("IPC: Message queue cleaned up\n");
        }
    }
}

// 제어 메시지 전송
int ipc_send_control_message(int msg_queue_id, const char *topic, const char *payload) {
    if (msg_queue_id == -1 || !topic || !payload) {
        return -1;
    }
    
    control_message_t msg;
    msg.msg_type = 1;
    
    // 토픽 복사 (길이 체크)
    strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
    msg.topic[sizeof(msg.topic) - 1] = '\0';
    
    // 페이로드 복사 (길이 체크)
    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
    msg.payload[sizeof(msg.payload) - 1] = '\0';
    
    if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {  // 큐가 가득 찬 경우가 아니면 에러 출력
            perror("IPC: msgsnd failed");
        }
        return -1;
    }
    
    printf("IPC: Control message sent - Topic: %s\n", topic);
    return 0;
}

// 제어 메시지 수신
int ipc_receive_control_message(int msg_queue_id, char *topic, char *payload, size_t payload_size) {
    if (msg_queue_id == -1 || !topic || !payload) {
        return -1;
    }
    
    control_message_t msg;
    ssize_t result = msgrcv(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT);
    if (result == -1) {
        if (errno != ENOMSG) {  // 메시지가 없는 경우가 아니면 에러 출력
            perror("IPC: msgrcv failed");
        }
        return -1;
    }
    
    // 받은 토픽과 페이로드 복사
    strncpy(topic, msg.topic, MAX_TOPIC_LEN - 1);
    topic[MAX_TOPIC_LEN - 1] = '\0';
    
    strncpy(payload, msg.payload, payload_size - 1);
    payload[payload_size - 1] = '\0';
    
    printf("IPC: Control message received - Topic: %s\n", msg.topic);
    return 0;
}