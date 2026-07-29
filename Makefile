CC ?= cc
BUILD_DIR ?= build
CFLAGS ?= -O2 -Wall -Wextra
HBOS_DIR ?= $(if $(wildcard ../HBOS/Makefile),../HBOS,$(if $(wildcard ../hbosv2/Makefile),../hbosv2,../HBOS))
HBOS_BUILD_DIR ?= $(CURDIR)/$(BUILD_DIR)/hbos
HBOS_RUN_TARGET ?= run

.PHONY: all help test integration-test image run start run-uefi sync clean

all: test

help:
	@echo "HIVE targets:"
	@echo "  make test              Run standalone Toolkit API tests"
	@echo "  make integration-test  Test the runtime against HBOS kernel contracts"
	@echo "  make image             Build HBOS images with this HIVE source tree"
	@echo "  make run / make start  Build and start HIVE in QEMU (BIOS)"
	@echo "  make run-uefi          Build and start HIVE in QEMU (UEFI)"
	@echo "  make sync              Refresh the transitional mirror from HBOS"
	@echo "  Override HBOS_DIR=/path/to/HBOS when it is not a sibling checkout"

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_hive_ui: tests/test_hive_ui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iapp/include -Isrc/user $< -o $@

test: $(BUILD_DIR)/test_hive_ui
	$(BUILD_DIR)/test_hive_ui
	@echo "HIVE Toolkit API 1.3 tests passed"

integration-test: test
	@test -f "$(HBOS_DIR)/Makefile" || \
		{ echo "HBOS checkout not found at $(HBOS_DIR); set HBOS_DIR=/path/to/HBOS" >&2; exit 2; }
	$(CC) $(CFLAGS) -I"$(HBOS_DIR)/scripts" -I"$(HBOS_DIR)/src/gui" \
		tests/test_hive_winsrv.c src/gui/winsrv.c \
		-o $(BUILD_DIR)/test_hive_winsrv
	$(BUILD_DIR)/test_hive_winsrv
	@echo "HIVE runtime integration tests passed"

image: integration-test
	$(MAKE) -C "$(HBOS_DIR)" -f "$(CURDIR)/mk/hbos-integration.mk" \
		-f Makefile \
		BUILD_DIR="$(HBOS_BUILD_DIR)" \
		HIVE_SRC_DIR="$(CURDIR)/src" APP_DIR="$(CURDIR)/app" iso

run: integration-test
	$(MAKE) -C "$(HBOS_DIR)" -f "$(CURDIR)/mk/hbos-integration.mk" \
		-f Makefile \
		BUILD_DIR="$(HBOS_BUILD_DIR)" \
		HIVE_SRC_DIR="$(CURDIR)/src" APP_DIR="$(CURDIR)/app" \
		"$(HBOS_RUN_TARGET)"

start: run

run-uefi: integration-test
	$(MAKE) -C "$(HBOS_DIR)" -f "$(CURDIR)/mk/hbos-integration.mk" \
		-f Makefile \
		BUILD_DIR="$(HBOS_BUILD_DIR)" \
		HIVE_SRC_DIR="$(CURDIR)/src" APP_DIR="$(CURDIR)/app" run-uefi

sync:
	@test -f "$(HBOS_DIR)/Makefile" || \
		{ echo "HBOS checkout not found at $(HBOS_DIR); set HBOS_DIR=/path/to/HBOS" >&2; exit 2; }
	bash tools/sync-from-hbos.sh "$(HBOS_DIR)"

clean:
	rm -f $(BUILD_DIR)/test_hive_ui $(BUILD_DIR)/test_hive_winsrv
