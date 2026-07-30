/* 设置 —— 独立设置前端；当前公开 ABI 可管理本应用外观。 */
#include <hive.h>

HIVE_APP("settings", "HIVE 设置");

enum { ID_TITLE = 1, ID_DARK, ID_SCALE, ID_SAMPLE, ID_STATUS };
static char status_buf[96] = "设置只作用于本应用；系统级设置等待服务 ABI。";

static void layout(hive_ui_t *ui, int width, int height) {
    hive_layout_t l;
    hive_layout_begin(&l, hive_rect(0, 0, width, height), 20, 12);
    hive_ui_widget(ui, ID_TITLE)->rect = hive_layout_row(&l, 26);
    hive_ui_widget(ui, ID_DARK)->rect = hive_layout_row(&l, 30);
    hive_ui_widget(ui, ID_SCALE)->rect = hive_layout_row(&l, 30);
    hive_ui_widget(ui, ID_SAMPLE)->rect = hive_layout_row(&l, 22);
    hive_ui_widget(ui, ID_STATUS)->rect = hive_layout_row(&l, 44);
}

static void apply_theme(hive_ui_t *ui, int dark) {
    ui->theme = hive_dark_theme();
    if (!dark) {
        ui->theme.window_bg = 0xEFF1F3;
        ui->theme.panel_bg = 0xFFFFFF;
        ui->theme.panel_alt = 0xE2E6EA;
        ui->theme.border = 0xAEB6BF;
        ui->theme.text = 0x232629;
        ui->theme.text_dim = 0x626B73;
        ui->theme.accent_text = 0xFFFFFF;
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int width = 430, height = 270;
    if (hive_window_open("设置", width, height) < 0) return 1;
    hive_ui_t ui;
    hive_ui_init(&ui);
    hive_ui_add_label(&ui, ID_TITLE, hive_rect(0, 0, 1, 1),
                      HIVE_VERSION_TAG);
    hive_ui_add_checkbox(&ui, ID_DARK, hive_rect(0, 0, 1, 1),
                         "深色外观", 1);
    hive_ui_add_slider(&ui, ID_SCALE, hive_rect(0, 0, 1, 1),
                       80, 160, 100, 10);
    hive_ui_add_label(&ui, ID_SAMPLE, hive_rect(0, 0, 1, 1),
                      "外观预览 / Appearance preview");
    hive_ui_add_label(&ui, ID_STATUS, hive_rect(0, 0, 1, 1), status_buf);
    layout(&ui, width, height);
    int running = 1, old_w = width, old_h = height;
    while (running && hive_window_active(&width, &height)) {
        if (width != old_w || height != old_h) {
            layout(&ui, width, height); old_w = width; old_h = height;
        }
        hive_event_t event;
        while (hive_ui_poll(&ui, &event) != HIVE_EVENT_NONE) {
            if (event.type == HIVE_EVENT_CLOSE) running = 0;
            else if (event.type == HIVE_EVENT_CHANGE &&
                     event.widget_id == ID_DARK) apply_theme(&ui, event.value);
            else if (event.type == HIVE_EVENT_CHANGE &&
                     event.widget_id == ID_SCALE) {
                snprintf(status_buf, sizeof(status_buf),
                         "预览缩放：%d%%（系统级设置等待服务 ABI）", event.value);
                hive_ui_set_text(&ui, ID_STATUS, status_buf);
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
