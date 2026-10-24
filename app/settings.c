/* 设置 —— 独立设置前端；当前公开 ABI 可管理本应用外观。 */
#include <hive.h>

HIVE_APP("settings", "HIVE 设置");

enum { ID_TITLE = 1, ID_APPEARANCE, ID_DARK, ID_SAMPLE, ID_DISPLAY,
       ID_SCALE, ID_BRIGHT, ID_TASKBAR, ID_SECONDS, ID_STATUS, ID_STATE };
static char status_buf[96] = "系统设置已连接";

static void layout(hive_ui_t *ui, int width, int height) {
    hive_layout_t l;
    hive_layout_begin(&l, hive_rect(0, 0, width, height), 20, 12);
    hive_ui_widget(ui, ID_TITLE)->rect = hive_layout_row(&l, 24);
    hive_ui_widget(ui, ID_APPEARANCE)->rect = hive_layout_row(&l, 20);
    hive_ui_widget(ui, ID_DARK)->rect = hive_layout_row(&l, 30);
    hive_ui_widget(ui, ID_SAMPLE)->rect = hive_layout_row(&l, 22);
    hive_ui_widget(ui, ID_DISPLAY)->rect = hive_layout_row(&l, 20);
    hive_ui_widget(ui, ID_SCALE)->rect = hive_layout_row(&l, 30);
    hive_ui_widget(ui, ID_BRIGHT)->rect = hive_layout_row(&l, 30);
    hive_ui_widget(ui, ID_TASKBAR)->rect = hive_layout_row(&l, 20);
    hive_ui_widget(ui, ID_SECONDS)->rect = hive_layout_row(&l, 30);
    hive_ui_widget(ui, ID_STATUS)->rect = hive_layout_row(&l, 20);
    hive_ui_widget(ui, ID_STATE)->rect = hive_layout_row(&l, 32);
}

static void apply_theme(hive_ui_t *ui, int dark) {
    ui->theme = dark ? hive_dark_theme() : hive_light_theme();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int width = 470, height = 330;
    if (hive_window_open("设置", width, height) < 0) return 1;
    hive_ui_t ui;
    hive_ui_init(&ui);
    hive_ui_add_label(&ui, ID_TITLE, hive_rect(0, 0, 1, 1),
                      HIVE_VERSION_TAG);
    hive_ui_add_label(&ui, ID_APPEARANCE, hive_rect(0, 0, 1, 1), "外观");
    hive_gui_settings_t settings;
    int settings_available = hive_gui_settings_query(&settings) == 0;
    if (!settings_available) {
        memset(&settings, 0, sizeof(settings));
        settings.theme = 1; settings.brightness = 100;
        settings.ui_scale = 100; settings.taskbar_show_seconds = 1;
        snprintf(status_buf, sizeof(status_buf), "系统设置不可用（本地预览）");
    }
    hive_ui_add_checkbox(&ui, ID_DARK, hive_rect(0, 0, 1, 1),
                         "深色主题", settings.theme);
    hive_ui_add_slider(&ui, ID_SCALE, hive_rect(0, 0, 1, 1),
                       70, 220, settings.ui_scale, 10);
    hive_ui_add_label(&ui, ID_DISPLAY, hive_rect(0, 0, 1, 1), "显示");
    hive_ui_add_slider(&ui, ID_BRIGHT, hive_rect(0, 0, 1, 1),
                       20, 100, settings.brightness, 10);
    hive_ui_add_checkbox(&ui, ID_SECONDS, hive_rect(0, 0, 1, 1),
                         "任务栏显示秒数", settings.taskbar_show_seconds);
    hive_ui_add_label(&ui, ID_TASKBAR, hive_rect(0, 0, 1, 1), "任务栏");
    hive_ui_add_label(&ui, ID_SAMPLE, hive_rect(0, 0, 1, 1),
                      "外观预览 / Appearance preview");
    hive_ui_add_label(&ui, ID_STATUS, hive_rect(0, 0, 1, 1), "运行状态");
    hive_ui_add_label(&ui, ID_STATE, hive_rect(0, 0, 1, 1), status_buf);
    apply_theme(&ui, settings.theme);
    layout(&ui, width, height);
    int running = 1, old_w = width, old_h = height;
    while (running && hive_window_active(&width, &height)) {
        if (width != old_w || height != old_h) {
            layout(&ui, width, height); old_w = width; old_h = height;
        }
        hive_event_t event;
        while (hive_ui_poll(&ui, &event) != HIVE_EVENT_NONE) {
            if (event.type == HIVE_EVENT_CLOSE) running = 0;
            else if (event.type == HIVE_EVENT_CHANGE && event.widget_id == ID_DARK) {
                settings.theme = event.value ? 1 : 0;
                apply_theme(&ui, event.value);
                if (settings_available && hive_gui_settings_update(&settings) < 0)
                    snprintf(status_buf, sizeof(status_buf), "主题更新失败");
            } else if (event.type == HIVE_EVENT_CHANGE && event.widget_id == ID_SCALE) {
                settings.ui_scale = event.value;
                if (settings_available && hive_gui_settings_update(&settings) < 0)
                    snprintf(status_buf, sizeof(status_buf), "缩放更新失败");
                else snprintf(status_buf, sizeof(status_buf), "缩放：%d%%", event.value);
                hive_ui_set_text(&ui, ID_STATE, status_buf);
            } else if (event.type == HIVE_EVENT_CHANGE && event.widget_id == ID_BRIGHT) {
                settings.brightness = event.value;
                if (settings_available && hive_gui_settings_update(&settings) < 0)
                    snprintf(status_buf, sizeof(status_buf), "亮度更新失败");
                else snprintf(status_buf, sizeof(status_buf), "亮度：%d%%", event.value);
                hive_ui_set_text(&ui, ID_STATE, status_buf);
            } else if (event.type == HIVE_EVENT_CHANGE && event.widget_id == ID_SECONDS) {
                settings.taskbar_show_seconds = event.value ? 1 : 0;
                if (settings_available && hive_gui_settings_update(&settings) < 0)
                    snprintf(status_buf, sizeof(status_buf), "任务栏设置失败");
                else snprintf(status_buf, sizeof(status_buf), "任务栏秒数：%s", event.value ? "开" : "关");
                hive_ui_set_text(&ui, ID_STATE, status_buf);
            }
        }
        hive_window_clear(ui.theme.window_bg);
        hive_ui_draw(&ui);
        hive_window_present();
        hive_yield();
    }
    hive_window_close();
    return 0;
}
