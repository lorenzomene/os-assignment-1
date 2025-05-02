SRC_DIR  := src
BUILD_DIR:= build
BIN      := iot_analyzer

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -pthread
SRCS    := $(wildcard $(SRC_DIR)/*.c)
OBJS    := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

.PHONY: clean run
run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN) output/results.csv
