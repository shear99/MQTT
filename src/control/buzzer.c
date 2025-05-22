#include "../mqtt.h"

// 버저 제어 함수
void handle_buzzer(const char *command) {
    printf("[BUZZER] Command received: %s\n", command);
    
    // 실제 제어 로직
    if (strcmp(command, "on") == 0) {
        buzzer_control(1);
        printf("[BUZZER] Turned ON\n");
    } else if (strcmp(command, "off") == 0) {
        buzzer_control(0);
        printf("[BUZZER] Turned OFF\n");
    } else if (strcmp(command, "beep") == 0) {
        // 짧은 비프음
        buzzer_control(1);
        usleep(500000); // 0.5초
        buzzer_control(0);
        printf("[BUZZER] Beeped\n");
    } else {
        printf("[BUZZER] Invalid command: %s\n", command);
    }
    
    char topic[MAX_TOPIC_LEN];
    char result[MAX_STRING_LEN];
    
    snprintf(topic, sizeof(topic), "status/raspberry_001/buzzer/return");
    
    if (strcmp(command, "on") == 0 || strcmp(command, "off") == 0 || strcmp(command, "beep") == 0) {
        snprintf(result, sizeof(result), 
                 "{\"device\":\"buzzer\",\"command\":\"%s\",\"status\":\"success\",\"timestamp\":%ld}", 
                 command, time(NULL));
    } else {
        snprintf(result, sizeof(result), 
                 "{\"device\":\"buzzer\",\"command\":\"%s\",\"status\":\"error\",\"message\":\"invalid command\",\"timestamp\":%ld}", 
                 command, time(NULL));
    }
    
    send_result_to_topic(topic, result);
}

// 실제 부저 제어 함수 (하드웨어 인터페이스)
void buzzer_control(int on_off) {
    // TODO: 실제 GPIO 제어 코드 구현
    printf("[HW] Buzzer GPIO control: %s\n", on_off ? "HIGH" : "LOW");
    
    // 실제 구현 시 추가될 코드:
    // - GPIO 라이브러리 초기화
    // - GPIO 핀 설정 (예: BCM 12번 핀)
    // - GPIO 핀 출력 설정
    // - GPIO 핀에 HIGH/LOW 신호 출력
    // - PWM을 사용한 주파수 제어 (선택적)
    
    // 임시로 시뮬레이션
    if (on_off) {
        // 부저 켜기
        // gpio_write(BUZZER_PIN, GPIO_HIGH);
        // 또는 PWM 설정: pwm_set_frequency(BUZZER_PIN, 1000); // 1kHz
    } else {
        // 부저 끄기
        // gpio_write(BUZZER_PIN, GPIO_LOW);
        // 또는 PWM 정지: pwm_stop(BUZZER_PIN);
    }
}