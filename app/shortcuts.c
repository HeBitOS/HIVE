/* 快捷键 —— 独立静态帮助应用。 */
#include <hive.h>

HIVE_APP("shortcuts", "HIVE 快捷键");

enum { ID_TITLE = 1, ID_LIST, ID_HINT };
static const char *const shortcuts[] = {
    "Tab / Shift+Tab    切换控件焦点",
    "方向键             列表、滑杆与滚动条",
    "Enter / Space      激活当前控件",
    "Home / End         跳到文本或列表边界",
    "PageUp / PageDown  快速滚动",
    "鼠标拖动标题栏     移动窗口",
    "窗口右上角按钮     最小化、最大化、关闭"
};

static void layout(hive_ui_t *ui, int width, int height) {
    hive_layout_t l;
    hive_layout_begin(&l, hive_rect(0, 0, width, height), 18, 10);
    hive_ui_widget(ui, ID_TITLE)->rect = hive_layout_row(&l, 26);
    hive_ui_widget(ui, ID_LIST)->rect = hive_layout_row(&l, height - 104);
    hive_ui_widget(ui, ID_HINT)->rect = hive_layout_row(&l, 24);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int width = 540, height = 350;
    if (hive_window_open("快捷键", width, height) < 0) return 1;
    hive_ui_t ui;
    hive_ui_init(&ui);
    hive_ui_add_label(&ui, ID_TITLE, hive_rect(0, 0, 1, 1),
                      "HIVE 桌面与应用通用快捷键");
    hive_ui_add_list(&ui, ID_LIST, hive_rect(0, 0, 1, 1),
                     shortcuts, sizeof(shortcuts) / sizeof(shortcuts[0]), 0);
    hive_ui_add_label(&ui, ID_HINT, hive_rect(0, 0, 1, 1),
                      "独立应用可自行扩展快捷键。");
    layout(&ui, width, height);
    int running = 1, old_w = width, old_h = height;
    while (running && hive_window_active(&width, &height)) {
        if (width != old_w || height != old_h) {
            layout(&ui, width, height); old_w = width; old_h = height;
        }
        hive_event_t event;
        while (hive_ui_poll(&ui, &event) != HIVE_EVENT_NONE)
            if (event.type == HIVE_EVENT_CLOSE) running = 0;
        hive_window_clear(ui.theme.window_bg);
        hive_ui_draw(&ui);
        hive_window_present();
        hive_yield();
    }
    hive_window_close();
    return 0;
}
