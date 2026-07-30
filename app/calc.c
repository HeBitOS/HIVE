/* 计算器 —— 独立 HIVE 应用，不依赖桌面 gui_state。 */
#include <hive.h>

HIVE_APP("calc", "计算器");

enum {
    ID_TITLE = 1, ID_LEFT, ID_OP, ID_RIGHT, ID_EQUALS, ID_RESULT, ID_CLEAR
};

static char left_buf[32] = "0";
static char right_buf[32] = "0";
static char result_buf[96] = "结果：0";
static const char *const operators[] = { "+", "-", "*", "/", "%" };

static void layout(hive_ui_t *ui, int width, int height) {
    hive_layout_t l;
    hive_layout_begin(&l, hive_rect(0, 0, width, height), 20, 10);
    hive_ui_widget(ui, ID_TITLE)->rect = hive_layout_row(&l, 26);
    hive_rect_t expression = hive_layout_row(&l, 72);
    hive_ui_widget(ui, ID_LEFT)->rect = hive_grid_cell(expression, 3, 10, 0);
    hive_ui_widget(ui, ID_OP)->rect = hive_grid_cell(expression, 3, 10, 1);
    hive_ui_widget(ui, ID_RIGHT)->rect = hive_grid_cell(expression, 3, 10, 2);
    hive_ui_widget(ui, ID_RESULT)->rect = hive_layout_row(&l, 30);
    hive_rect_t actions = hive_layout_row(&l, 38);
    hive_ui_widget(ui, ID_EQUALS)->rect = hive_grid_cell(actions, 2, 10, 0);
    hive_ui_widget(ui, ID_CLEAR)->rect = hive_grid_cell(actions, 2, 10, 1);
}

static void calculate(hive_ui_t *ui) {
    HI32 selected = 0;
    long a = strtol(left_buf, NULL, 10);
    long b = strtol(right_buf, NULL, 10);
    long value = 0;
    hive_ui_get_value(ui, ID_OP, &selected);
    if ((selected == 3 || selected == 4) && b == 0) {
        snprintf(result_buf, sizeof(result_buf), "错误：不能除以零");
    } else {
        if (selected == 0) value = a + b;
        else if (selected == 1) value = a - b;
        else if (selected == 2) value = a * b;
        else if (selected == 3) value = a / b;
        else value = a % b;
        snprintf(result_buf, sizeof(result_buf), "结果：%ld", value);
    }
    hive_ui_set_text(ui, ID_RESULT, result_buf);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int width = 430, height = 270;
    if (hive_window_open("计算器", width, height) < 0) return 1;

    hive_ui_t ui;
    hive_ui_init(&ui);
    hive_ui_add_label(&ui, ID_TITLE, hive_rect(0, 0, 1, 1),
                      "整数计算器（Tab 切换焦点）");
    hive_ui_add_textbox(&ui, ID_LEFT, hive_rect(0, 0, 1, 1),
                        left_buf, sizeof(left_buf));
    hive_ui_add_list(&ui, ID_OP, hive_rect(0, 0, 1, 1),
                     operators, 5, 0);
    hive_ui_add_textbox(&ui, ID_RIGHT, hive_rect(0, 0, 1, 1),
                        right_buf, sizeof(right_buf));
    hive_ui_add_label(&ui, ID_RESULT, hive_rect(0, 0, 1, 1), result_buf);
    hive_ui_add_button(&ui, ID_EQUALS, hive_rect(0, 0, 1, 1), "计算");
    hive_ui_add_button(&ui, ID_CLEAR, hive_rect(0, 0, 1, 1), "清空");
    layout(&ui, width, height);

    int running = 1, old_w = width, old_h = height;
    while (running && hive_window_active(&width, &height)) {
        if (width != old_w || height != old_h) {
            layout(&ui, width, height); old_w = width; old_h = height;
        }
        hive_event_t event;
        while (hive_ui_poll(&ui, &event) != HIVE_EVENT_NONE) {
            if (event.type == HIVE_EVENT_CLOSE) running = 0;
            else if ((event.type == HIVE_EVENT_CLICK &&
                      event.widget_id == ID_EQUALS) ||
                     event.type == HIVE_EVENT_SUBMIT) calculate(&ui);
            else if (event.type == HIVE_EVENT_CLICK &&
                     event.widget_id == ID_CLEAR) {
                strcpy(left_buf, "0"); strcpy(right_buf, "0");
                strcpy(result_buf, "结果：0");
                hive_ui_set_text(&ui, ID_RESULT, result_buf);
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
