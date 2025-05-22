#include "../mqtt.h"

// 포토레지스터 제어 함수
void handle_photoresistor(const char *command) {
    printf("[PHOTORESISTOR] Command received: %s\n", command);
    
    char topic[MAX_TOPIC_LEN];
    char result[MAX_STRING_LEN];
    int sensor_value = 0;
    
    // 실제 제어 로직
    if (strcmp(command, "read") == 0 || strcmp(command, "value") == 0) {
        sensor_value = photoresistor_read();
        printf("[PHOTORESISTOR] Read value: %d\n", sensor_value);
        
        snprintf(topic, sizeof(topic), "status/raspberry_001/photoresistor/return");
        snprintf(result, sizeof(result), 
                 "{\"device\":\"photoresistor\",\"command\":\"%s\",\"value\":%d,\"status\":\"success\",\"timestamp\":%ld}", 
                 command, sensor_value, time(NULL));
    } 
    else if (strcmp(command, "calibrate") == 0) {
        // 센서 캘리브레이션 (여러 번 읽어서 평균값 계산)
        int sum = 0;
        int samples = 10;
        for (int i = 0; i < samples; i++) {
            sum += photoresistor_read();
            usleep(100000); // 100ms 간격
        }
        sensor_value = sum / samples;
        printf("[PHOTORESISTOR] Calibrated average value: %d\n", sensor_value);
        
        snprintf(topic, sizeof(topic), "status/raspberry_001/photoresistor/return");
        snprintf(result, sizeof(result), 
                 "{\"device\":\"photoresistor\",\"command\":\"%s\",\"calibrated_value\":%d,\"samples\":%d,\"status\":\"success\",\"timestamp\":%ld}", 
                 command, sensor_value, samples, time(NULL));
    }
    else {
        printf("[PHOTORESISTOR] Invalid command: %s\n", command);
        
        snprintf(topic, sizeof(topic), "status/raspberry_001/photoresistor/return");
        snprintf(result, sizeof(result), 
                 "{\"device\":\"photoresistor\",\"command\":\"%s\",\"status\":\"error\",\"message\":\"invalid command\",\"timestamp\":%ld}", 
                 command, time(NULL));
    }
    
    send_result_to_topic(topic, result);
}

// 실제 포토레지스터 읽기 함수 (하드웨어 인터페이스)
int photoresistor_read(void) {
    // TODO: 실제 ADC 읽기 코드 구현
    // 포토레지스터는 아날로그 센서이므로 ADC(Analog-to-Digital Converter) 필요
    
    // 실제 구현 시 추가될 코드:
    // - SPI 인터페이스 초기화 (MCP3008 등의 ADC 칩 사용)
    // - ADC 채널 선택
    // - ADC 값 읽기 (0-1023 범위)
    // - 조도 값으로 변환
    
    // 임시로 시뮬레이션 (랜덤 값 + 시간 기반 변화)
    static int base_value = 512;
    static time_t last_time = 0;
    
    time_t current_time = time(NULL);
    if (current_time != last_time) {
        // 시간이 바뀔 때마다 약간의 변화 추가 (조도 변화 시뮬레이션)
        base_value += (rand() % 100 - 50); // -50 ~ +49 범위의 변화
        if (base_value < 0) base_value = 0;
        if (base_value > 1023) base_value = 1023;
        last_time = current_time;
    }
    
    // 약간의 노이즈 추가
    int noise = rand() % 20 - 10; // -10 ~ +9 범위의 노이즈
    int final_value = base_value + noise;
    
    if (final_value < 0) final_value = 0;
    if (final_value > 1023) final_value = 1023;
    
    printf("[HW] Photoresistor ADC value: %d (simulated)\n", final_value);
    
    return final_value;
}