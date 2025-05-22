#include "../mqtt.h"

// s_segment 제어 함수
void handle_s_segment(const char *command) {
    // 4번째 필드(명령) 출력
    printf("S-Segment Control Command: %s\n", command);
}

// ...기존 코드 계속...