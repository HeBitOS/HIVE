CC ?= cc
BUILD_DIR ?= build
CFLAGS ?= -O2 -Wall -Wextra

.PHONY: all test clean

all: test

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_hive_ui: tests/test_hive_ui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iapp/include -Isrc/user $< -o $@

test: $(BUILD_DIR)/test_hive_ui
	$(BUILD_DIR)/test_hive_ui
	@echo "HIVE UI 1.1 tests passed"

clean:
	rm -f $(BUILD_DIR)/test_hive_ui
