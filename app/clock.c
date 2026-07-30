/* 时钟 —— 用户态只能取得开机秒数，因此明确显示系统运行时间。 */
#include <hive.h>
#include "libc/time.h"

HIVE_APP("clock", "时钟与系统运行时间");

enum { ID_TITLE = 1, ID_TIME, ID_DAYS, ID_HINT };
static char time_buf[48];
static char days_buf[48];

static void layout(hive_ui_t *ui, int width, int height) {
    hive_layout_t l;
    hive_layout_begin(&l, hive_rect(0, 0, width, height), 24, 14);
    hive_ui_widget(ui, ID_TITLE)->rect = hive_layout_row(&l, 26);
    hive_ui_widget(ui, ID_TIME)->rect = hive_layout_row(&l, 34);
    hive_ui_widget(ui, ID_DAYS)->rect = hive_layout_row(&l, 26);
    hive_ui_widget(ui, ID_HINT)->rect = hive_layout_row(&l, 42);
}

static void update_time(hive_ui_t *ui) {
    long total = (long)time(NULL);
    long days = total / 86400;
    long rest = total % 86400;
    long hours = rest / 3600;
    long minutes = (rest % 3600) / 60;
    long seconds = rest % 60;
    snprintf(time_buf, sizeof(time_buf), "%02ld:%02ld:%02ld",
             hours, minutes, seconds);
    snprintf(days_buf, sizeof(days_buf), "已运行 %ld 天", days);
    hive_ui_set_text(ui, ID_TIME, time_buf);
    hive_ui_set_text(ui, ID_DAYS, days_buf);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int width = 360, height = 220;
    if (hive_window_open("时钟", width, height) < 0) return 1;
    hive_ui_t ui;
    hive_ui_init(&ui);
    hive_ui_add_label(&ui, ID_TITLE, hive_rect(0, 0, 1, 1), "系统运行时间");
    hive_ui_add_label(&ui, ID_TIME, hive_rect(0, 0, 1, 1), time_buf);
    hive_ui_add_label(&ui, ID_DAYS, hive_rect(0, 0, 1, 1), days_buf);
    hive_ui_add_label(&ui, ID_HINT, hive_rect(0, 0, 1, 1),
                      "真实日期/时区需要未来的系统时间服务。");
    layout(&ui, width, height);
    int running = 1, old_w = width, old_h = height;
    while (running && hive_window_active(&width, &height)) {
        if (width != old_w || height != old_h) {
            layout(&ui, width, height); old_w = width; old_h = height;
        }
        hive_event_t event;
        while (hive_ui_poll(&ui, &event) != HIVE_EVENT_NONE)
            if (event.type == HIVE_EVENT_CLOSE) running = 0;
        update_time(&ui);
        hive_window_clear(ui.theme.window_bg);
        hive_ui_draw(&ui);
        hive_window_present();
        hive_yield();
    }
    hive_window_close();
    return 0;
}
