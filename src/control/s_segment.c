#include "../mqtt.h"

// 7-segment 디스플레이 패턴 (공통 캐소드 기준)
// 각 비트는 a, b, c, d, e, f, g, dp 세그먼트를 나타냄
static const unsigned char segment_patterns[10] = {
    0x3F, // 0: abcdef
    0x06, // 1: bc
    0x5B, // 2: abdeg
    0x4F, // 3: abcdg
    0x66, // 4: bcfg
    0x6D, // 5: acdfg
    0x7D, // 6: acdefg
    0x07, // 7: abc
    0x7F, // 8: abcdefg
    0x6F  // 9: abcdfg
};

// s_segment 제어 함수
void handle_s_segment(const char *command) {
    printf("[S_SEGMENT] Command received: %s\n", command);
    
    char topic[MAX_TOPIC_LEN];
    char result[MAX_STRING_LEN];
    int display_value = -1;
    
    // 명령어 파싱
    if (strcmp(command, "clear") == 0 || strcmp(command, "off") == 0) {
        // 디스플레이 끄기
        seven_segment_display(-1);
        printf("[S_SEGMENT] Display cleared\n");
        
        snprintf(topic, sizeof(topic), "status/raspberry_001/s_segment/return");
        snprintf(result, sizeof(result), 
                 "{\"device\":\"7segment\",\"command\":\"%s\",\"status\":\"success\",\"timestamp\":%ld}", 
                 command, time(NULL));
    }
    else if (strcmp(command, "test") == 0) {
        // 테스트 패턴 (0-9 순차 표시)
        printf("[S_SEGMENT] Running test pattern\n");
        for (int i = 0; i <= 9; i++) {
            seven_segment_display(i);
            usleep(500000); // 0.5초 대기
        }
        seven_segment_display(-1); // 끄기
        
        snprintf(topic, sizeof(topic), "status/raspberry_001/s_segment/return");
        snprintf(result, sizeof(result), 
                 "{\"device\":\"7segment\",\"command\":\"%s\",\"status\":\"success\",\"message\":\"test pattern completed\",\"timestamp\":%ld}", 
                 command, time(NULL));
    }
    else {
        // 숫자 값으로 파싱 시도
        display_value = atoi(command);
        if (display_value >= 0 && display_value <= 9) {
            seven_segment_display(display_value);
            printf("[S_SEGMENT] Displaying: %d\n", display_value);
            
            snprintf(topic, sizeof(topic), "status/raspberry_001/s_segment/return");
            snprintf(result, sizeof(result), 
                     "{\"device\":\"7segment\",\"command\":\"%s\",\"value\":%d,\"status\":\"success\",\"timestamp\":%ld}", 
                     command, display_value, time(NULL));
        } else {
            printf("[S_SEGMENT] Invalid value: %s (must be 0-9, 'clear', 'off', or 'test')\n", command);
            
            snprintf(topic, sizeof(topic), "status/raspberry_001/s_segment/return");
            snprintf(result, sizeof(result), 
                     "{\"device\":\"7segment\",\"command\":\"%s\",\"status\":\"error\",\"message\":\"invalid value (0-9, clear, off, test)\",\"timestamp\":%ld}", 
                     command, time(NULL));
        }
    }
    
    send_result_to_topic(topic, result);
}

// 실제 7-세그먼트 디스플레이 제어 함수 (하드웨어 인터페이스)
void seven_segment_display(int value) {
    // TODO: 실제 GPIO 제어 코드 구현
    
    if (value == -1) {
        // 디스플레이 끄기
        printf("[HW] 7-Segment display: OFF (all segments)\n");
        // 모든 GPIO 핀을 LOW로 설정
        // for (int i = 0; i < 8; i++) gpio_write(segment_pins[i], GPIO_LOW);
        return;
    }
    
    if (value < 0 || value > 9) {
        printf("[HW] 7-Segment display: Invalid value %d\n", value);
        return;
    }
    
    unsigned char pattern = segment_patterns[value];
    printf("[HW] 7-Segment display: %d (pattern: 0x%02X)\n", value, pattern);
    
    // 실제 구현 시 추가될 코드:
    // - GPIO 핀 설정 (예: BCM 2, 3, 4, 17, 27, 22, 10, 9번 핀)
    // - 각 세그먼트(a~g, dp)에 해당하는 GPIO 핀 제어
    // - 공통 캐소드/애노드에 따른 로직 레벨 조정
    
    // 임시로 세그먼트별 상태 출력
    printf("[HW] Segments: ");
    char segments[] = "abcdefgp";
    for (int i = 0; i < 8; i++) {
        if (pattern & (1 << i)) {
            printf("%c", segments[i]);
            // gpio_write(segment_pins[i], GPIO_HIGH);
        } else {
            printf("-");
            // gpio_write(segment_pins[i], GPIO_LOW);
        }
    }
    printf("\n");
    
    // 실제 GPIO 제어 예시:
    /*
    static int segment_pins[8] = {2, 3, 4, 17, 27, 22, 10, 9}; // a, b, c, d, e, f, g, dp
    for (int i = 0; i < 8; i++) {
        if (pattern & (1 << i)) {
            gpio_write(segment_pins[i], GPIO_HIGH);
        } else {
            gpio_write(segment_pins[i], GPIO_LOW);
        }
    }
    */
}