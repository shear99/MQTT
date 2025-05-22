#include "../mqtt.h"

// LED 제어를 위한 함수
void handle_led(const char *command) {
    // 4번째 필드(명령) 출력
    printf("LED Control Command: %s\n", command);
}

// 기존 코드 유지...