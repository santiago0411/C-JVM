CC = gcc
CFLAGS_COMMON = -Wall -Wextra -std=c11
SRC_DIR = src
BUILD_DIR = bin
BIN_INT_DIR = bin-int
TARGET = jvm.exe

DEBUG_CFLAGS = -g
RELEASE_CFLAGS = -O3 -s

SRCS = $(wildcard $(SRC_DIR)/*.c)
HEADERS = $(wildcard $(SRC_DIR)/*.h)
DEBUG_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BIN_INT_DIR)/debug/%.o, $(SRCS))
RELEASE_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BIN_INT_DIR)/release/%.o, $(SRCS))

.PHONY: all clean debug release

all: debug release

debug: CFLAGS = $(CFLAGS_COMMON) $(DEBUG_CFLAGS)
debug: $(BUILD_DIR)/debug/$(TARGET)

release: CFLAGS = $(CFLAGS_COMMON) $(RELEASE_CFLAGS)
release: $(BUILD_DIR)/release/$(TARGET)

$(BUILD_DIR)/debug/$(TARGET): $(DEBUG_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/release/$(TARGET): $(RELEASE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_INT_DIR)/debug/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c -DAPP_DEBUG -o $@ $<

$(BIN_INT_DIR)/release/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c -DAPP_RELEASE -o $@ $<

clean:
	rm -rf $(BUILD_DIR) $(BIN_INT_DIR)

$(shell mkdir -p $(BUILD_DIR)/debug)
$(shell mkdir -p $(BUILD_DIR)/release)

$(shell mkdir -p $(BIN_INT_DIR)/debug)
$(shell mkdir -p $(BIN_INT_DIR)/release)
