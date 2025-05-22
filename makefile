# Makefile for MQTT Subscriber Project

# 컴파일러 설정
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2
LDFLAGS = -lpaho-mqtt3cs -lcjson

# 디렉터리 설정
SRCDIR = src
NETDIR = $(SRCDIR)/network
CTRLDIR = $(SRCDIR)/control
IPCDIR = $(SRCDIR)/IPC
OBJDIR = obj
BINDIR = bin

# 소스 파일들 (network, control 포함)
SOURCES = $(SRCDIR)/main.c \
	$(NETDIR)/topic_manager.c \
	$(NETDIR)/sub_message_handler.c \
	$(NETDIR)/pub_message_handler.c \
	$(wildcard $(CTRLDIR)/*.c) \
	$(IPCDIR)/ipc_handler.c
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
TARGET = $(BINDIR)/mqtt

# 헤더 파일 (경로 반영)
HEADERS = $(SRCDIR)/mqtt.h

# 기본 타겟
all: directories $(TARGET)

# 디렉터리 생성
directories:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)

# 오브젝트 파일 생성 (main.c)
$(OBJDIR)/main.o: $(SRCDIR)/main.c $(HEADERS)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# 오브젝트 파일 생성 (network/*.c)
$(OBJDIR)/network/%.o: $(NETDIR)/%.c $(HEADERS)
	@mkdir -p $(OBJDIR)/network
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# 오브젝트 파일 생성 (control/*.c)
$(OBJDIR)/control/%.o: $(CTRLDIR)/%.c $(HEADERS)
	@mkdir -p $(OBJDIR)/control
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# 오브젝트 파일 생성 (IPC/*.c)
$(OBJDIR)/IPC/%.o: $(IPCDIR)/%.c $(HEADERS)
	@mkdir -p $(OBJDIR)/IPC
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# 실행 파일 생성
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	@$(CC) $^ -o $@ $(LDFLAGS)
	@echo "✓ Build completed successfully!"

# 정리
clean:
	@echo "Cleaning up..."
	@rm -rf $(OBJDIR) $(BINDIR)
	@echo "✓ Clean completed!"

# 재빌드
rebuild: clean all

# 설치 (선택적)
install: $(TARGET)
	@echo "Installing to /usr/local/bin..."
	@sudo cp $(TARGET) /usr/local/bin/
	@sudo chmod +x /usr/local/bin/mqtt_subscriber
	@echo "✓ Installation completed!"

# 제거
uninstall:
	@echo "Removing from /usr/local/bin..."
	@sudo rm -f /usr/local/bin/mqtt_subscriber
	@echo "✓ Uninstallation completed!"

# 실행
run: $(TARGET)
	@echo "Running MQTT Subscriber..."
	@$(TARGET)

# 실행 with custom config
run-config: $(TARGET)
	@echo "Running MQTT Subscriber with custom config..."
	@$(TARGET) $(CONFIG)

# 디버그 실행
debug: $(TARGET)
	@echo "Running with GDB..."
	@gdb $(TARGET)

# 메모리 체크
memcheck: $(TARGET)
	@echo "Running with Valgrind..."
	@valgrind --tool=memcheck --leak-check=full $(TARGET)

# 도움말
help:
	@echo "Available targets:"
	@echo "  all        - Build the project (default)"
	@echo "  clean      - Remove build files"
	@echo "  rebuild    - Clean and build"
	@echo "  install    - Install to /usr/local/bin"
	@echo "  uninstall  - Remove from /usr/local/bin"
	@echo "  run        - Run with default config"
	@echo "  run-config - Run with custom config (usage: make run-config CONFIG=myconfig.conf)"
	@echo "  debug      - Run with GDB debugger"
	@echo "  memcheck   - Run with Valgrind memory checker"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Build requirements:"
	@echo "  - libpaho-mqtt-dev"
	@echo "  - libcjson-dev"
	@echo "  - gcc"
	@echo ""
	@echo "Example usage:"
	@echo "  make                    # Build project"
	@echo "  make run                # Build and run"
	@echo "  make clean rebuild      # Clean rebuild"
	@echo "  make run-config CONFIG=test.conf  # Run with custom config"

# Phony targets
.PHONY: all clean rebuild install uninstall run run-config debug memcheck help directories

# 의존성 검사
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists libpaho-mqtt3c || (echo "❌ libpaho-mqtt3c not found. Install with: sudo apt install libpaho-mqtt-dev" && exit 1)
	@pkg-config --exists libcjson || (echo "❌ libcjson not found. Install with: sudo apt install libcjson-dev" && exit 1)
	@echo "✓ All dependencies found!"

# 라이브러리 정보 출력
info:
	@echo "=== Build Information ==="
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Libraries: $(LDFLAGS)"
	@echo "Sources: $(SOURCES)"
	@echo "Target: $(TARGET)"
	@echo ""
	@echo "=== Library Versions ==="
	@pkg-config --modversion libpaho-mqtt3c 2>/dev/null && echo "Paho MQTT: $$(pkg-config --modversion libpaho-mqtt3c)" || echo "Paho MQTT: Version unknown"
	@pkg-config --modversion libcjson 2>/dev/null && echo "cJSON: $$(pkg-config --modversion libcjson)" || echo "cJSON: Version unknown"