CC ?= cc
BUILD_DIR ?= build
CFLAGS ?= -O2 -Wall -Wextra

.PHONY: all test integration-test sync clean

all: test

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_hive_ui: tests/test_hive_ui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iapp/include -Isrc/user $< -o $@

test: $(BUILD_DIR)/test_hive_ui
	$(BUILD_DIR)/test_hive_ui
	@echo "HIVE Toolkit API 1.3 tests passed"

integration-test: test
	@test -n "$(HBOS_DIR)" || \
		{ echo "usage: make integration-test HBOS_DIR=/path/to/HBOS" >&2; exit 2; }
	$(CC) $(CFLAGS) -I"$(HBOS_DIR)/scripts" -I"$(HBOS_DIR)/src/gui" \
		tests/test_hive_winsrv.c src/gui/winsrv.c \
		-o $(BUILD_DIR)/test_hive_winsrv
	$(BUILD_DIR)/test_hive_winsrv
	@echo "HIVE runtime integration tests passed"

sync:
	@test -n "$(HBOS_DIR)" || \
		{ echo "usage: make sync HBOS_DIR=/path/to/HBOS" >&2; exit 2; }
	bash tools/sync-from-hbos.sh "$(HBOS_DIR)"

clean:
	rm -f $(BUILD_DIR)/test_hive_ui $(BUILD_DIR)/test_hive_winsrv
