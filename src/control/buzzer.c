#include "../mqtt.h"

// 버저 제어 함수
void handle_buzzer(const char *command) {
    // 4번째 필드(명령) 출력
    printf("Buzzer Control Command: %s\n", command);
}