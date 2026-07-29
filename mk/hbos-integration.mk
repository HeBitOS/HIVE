# Build overlay loaded before the HBOS Makefile.
#
# This keeps the transitional integration logic in HIVE: HBOS supplies the
# kernel, drivers, boot files and resource blobs, while GUI/runtime C sources
# and HAX applications come directly from this checkout.

SRC_DIR ?= src

ifndef HIVE_SRC_DIR
$(error HIVE_SRC_DIR must point to the HIVE src directory)
endif

override GUI_C_SRCS = \
	$(HIVE_SRC_DIR)/gpu.c \
	$(HIVE_SRC_DIR)/graphics/gui_font.c \
	$(HIVE_SRC_DIR)/graphics/gui_wall.c \
	$(HIVE_SRC_DIR)/graphics/gui_icons.c \
	$(HIVE_SRC_DIR)/tools/gui.c \
	$(HIVE_SRC_DIR)/gui/service.c \
	$(HIVE_SRC_DIR)/gui/wm.c \
	$(HIVE_SRC_DIR)/gui/winsrv.c \
	$(HIVE_SRC_DIR)/gui/compositor.c \
	$(HIVE_SRC_DIR)/gui/effects.c \
	$(HIVE_SRC_DIR)/gui/gui_dirty.c \
	$(HIVE_SRC_DIR)/gui/gui_apps.c \
	$(HIVE_SRC_DIR)/gui/apps/app_calc.c \
	$(HIVE_SRC_DIR)/gui/apps/app_clock.c \
	$(HIVE_SRC_DIR)/gui/apps/app_settings.c \
	$(HIVE_SRC_DIR)/gui/apps/app_files.c \
	$(HIVE_SRC_DIR)/gui/apps/app_taskmgr.c \
	$(HIVE_SRC_DIR)/gui/apps/app_shortcuts.c \
	$(HIVE_SRC_DIR)/gui/apps/app_imgview.c \
	$(HIVE_SRC_DIR)/gui/apps/app_hexview.c

override HBOS_C_SRCS = $(filter-out $(HIVE_SRC_DIR)/%,$(C_SRCS))
override HIVE_C_SRCS = $(filter $(HIVE_SRC_DIR)/%,$(C_SRCS))
override C_OBJS = \
	$(HBOS_C_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
	$(HIVE_C_SRCS:$(HIVE_SRC_DIR)/%.c=$(BUILD_DIR)/hive/%.o)

$(BUILD_DIR)/hive/tools/gui.o: $(SRC_DIR)/version.h
$(BUILD_DIR)/hive/gui/apps/app_settings.o: $(SRC_DIR)/version.h

$(BUILD_DIR)/hive/%.o: $(HIVE_SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(@D)
	$(CC) -c \
		-I$(HIVE_SRC_DIR) -I$(HIVE_SRC_DIR)/graphics \
		-I$(HIVE_SRC_DIR)/gui -I$(HIVE_SRC_DIR)/tools \
		-I$(SRC_DIR)/gui/apps $(CFLAGS) $< -o $@
