CC ?= cc
BUILD_DIR ?= build
CFLAGS ?= -O2 -Wall -Wextra
HBOS_DIR ?= $(if $(wildcard ../HBOS/Makefile),../HBOS,$(if $(wildcard ../hbosv2/Makefile),../hbosv2,../HBOS))
HBOS_BUILD_DIR ?= $(CURDIR)/$(BUILD_DIR)/hbos
HBOS_RUN_TARGET ?= run
PKG_REPO_DIR ?= $(CURDIR)/$(BUILD_DIR)/repo
HPT_DIR ?= $(CURDIR)/HPT
APP_STAGE_DIR ?= $(CURDIR)/$(BUILD_DIR)/app-src

.PHONY: all help check-hpt test browser-test modern-web-test bilibili-home-smoke bilibili-video-smoke test-app-boundaries integration-test stage-apps apps repo image run start run-uefi sync clean

all: test

help:
	@echo "HIVE targets:"
	@echo "  make test              Run standalone Toolkit API tests"
	@echo "  make browser-test      Check browser capability gates"
	@echo "  make modern-web-test   Run Blink/V8/Vue acceptance (set HIVE_CONTENT_SHELL)"
	@echo "  make bilibili-home-smoke  Probe Bilibili home through CDP"
	@echo "  make bilibili-video-smoke Probe a video URL (set HIVE_BILIBILI_VIDEO_URL)"
	@echo "  make integration-test  Test the runtime against HBOS kernel contracts"
	@echo "  make apps              Build all HIVE apps, including mandatory HPT"
	@echo "  make repo              Build a local HPT repository"
	@echo "  make image             Build HBOS images with this HIVE source tree"
	@echo "  make run / make start  Build and start HIVE in QEMU (BIOS)"
	@echo "  make run-uefi          Build and start HIVE in QEMU (UEFI)"
	@echo "  make sync              Refresh the transitional mirror from HBOS"
	@echo "  Override HBOS_DIR=/path/to/HBOS when it is not a sibling checkout"

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_hive_ui: tests/test_hive_ui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iapp/include -Isrc/user $< -o $@

$(BUILD_DIR)/test_browser_backend: tests/test_browser_backend.c src/gui/browser_backend.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc/gui $^ -o $@

browser-test: $(BUILD_DIR)/test_browser_backend
	$(BUILD_DIR)/test_browser_backend

modern-web-test:
	tests/browser/modern-web/run_content_shell.sh

bilibili-home-smoke:
	node tests/browser/bilibili_probe.mjs

bilibili-video-smoke:
	node tests/browser/bilibili_probe.mjs --video "$(HIVE_BILIBILI_VIDEO_URL)"

test-app-boundaries:
	bash tests/test_app_boundaries.sh

check-hpt:
	@test -f "$(HPT_DIR)/app/hpt.c" && \
		test -f "$(HPT_DIR)/app/include/hpt.h" && \
		test -f "$(HPT_DIR)/tools/hpt_repo.py" || \
		{ echo "HPT submodule is missing; run: git submodule update --init --recursive" >&2; exit 2; }

test: check-hpt $(BUILD_DIR)/test_hive_ui browser-test test-app-boundaries
	$(BUILD_DIR)/test_hive_ui
	$(MAKE) -C "$(HPT_DIR)" test
	python3 "$(HPT_DIR)/tools/hpt_repo.py" check packages/manifest.json
	@echo "HIVE host tests passed"

integration-test: test
	@test -f "$(HBOS_DIR)/Makefile" || \
		{ echo "HBOS checkout not found at $(HBOS_DIR); set HBOS_DIR=/path/to/HBOS" >&2; exit 2; }
	$(CC) $(CFLAGS) -I"$(HBOS_DIR)/scripts" -I"$(HBOS_DIR)/src/gui" \
		tests/test_hive_winsrv.c src/gui/winsrv.c \
		-o $(BUILD_DIR)/test_hive_winsrv
	$(BUILD_DIR)/test_hive_winsrv
	@echo "HIVE runtime integration tests passed"

stage-apps: check-hpt | $(BUILD_DIR)
	rm -rf "$(APP_STAGE_DIR)"
	mkdir -p "$(APP_STAGE_DIR)/include"
	cp app/*.c "$(APP_STAGE_DIR)/"
	cp app/include/*.h "$(APP_STAGE_DIR)/include/"
	cp "$(HPT_DIR)/app/hpt.c" "$(APP_STAGE_DIR)/hpt.c"
	cp "$(HPT_DIR)/app/include/hpt.h" "$(APP_STAGE_DIR)/include/hpt.h"

apps: integration-test stage-apps
	$(MAKE) -C "$(HBOS_DIR)" -f "$(CURDIR)/mk/hbos-integration.mk" \
		-f Makefile \
		BUILD_DIR="$(HBOS_BUILD_DIR)" \
		HIVE_SRC_DIR="$(CURDIR)/src" APP_DIR="$(APP_STAGE_DIR)" hax-apps

repo: apps
	python3 "$(HPT_DIR)/tools/hpt_repo.py" build \
		--manifest packages/manifest.json \
		--apps-dir "$(HBOS_BUILD_DIR)/app" \
		--output "$(PKG_REPO_DIR)"

image: integration-test stage-apps
	$(MAKE) -C "$(HBOS_DIR)" -f "$(CURDIR)/mk/hbos-integration.mk" \
		-f Makefile \
		BUILD_DIR="$(HBOS_BUILD_DIR)" \
		HIVE_SRC_DIR="$(CURDIR)/src" APP_DIR="$(APP_STAGE_DIR)" iso

run: integration-test stage-apps
	$(MAKE) -C "$(HBOS_DIR)" -f "$(CURDIR)/mk/hbos-integration.mk" \
		-f Makefile \
		BUILD_DIR="$(HBOS_BUILD_DIR)" \
		HIVE_SRC_DIR="$(CURDIR)/src" APP_DIR="$(APP_STAGE_DIR)" \
		"$(HBOS_RUN_TARGET)"

start: run

run-uefi: integration-test stage-apps
	$(MAKE) -C "$(HBOS_DIR)" -f "$(CURDIR)/mk/hbos-integration.mk" \
		-f Makefile \
		BUILD_DIR="$(HBOS_BUILD_DIR)" \
		HIVE_SRC_DIR="$(CURDIR)/src" APP_DIR="$(APP_STAGE_DIR)" run-uefi

sync:
	@test -f "$(HBOS_DIR)/Makefile" || \
		{ echo "HBOS checkout not found at $(HBOS_DIR); set HBOS_DIR=/path/to/HBOS" >&2; exit 2; }
	bash tools/sync-from-hbos.sh "$(HBOS_DIR)"

clean:
	rm -rf "$(BUILD_DIR)"
