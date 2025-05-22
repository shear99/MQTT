#include "../mqtt.h"

// 포토레지스터 제어 함수
void handle_photoresistor(const char *command) {
    // 4번째 필드(명령) 출력
    printf("Photoresistor Control Command: %s\n", command);
}