/* 任务管理器 —— 独立用户态进程控制前端。 */
#include <hive.h>

HIVE_APP("taskmgr", "任务管理器");

enum { ID_TITLE = 1, ID_SELF, ID_PID, ID_SIGNAL, ID_SEND, ID_STATUS };
static char self_buf[64];
static char pid_buf[24];
static char status_buf[96] = "输入 PID 后发送信号";
static const char *const signals[] = { "TERM (15)", "KILL (9)", "INT (2)" };

static void layout(hive_ui_t *ui, int width, int height) {
    hive_layout_t l;
    hive_layout_begin(&l, hive_rect(0, 0, width, height), 20, 10);
    hive_ui_widget(ui, ID_TITLE)->rect = hive_layout_row(&l, 24);
    hive_ui_widget(ui, ID_SELF)->rect = hive_layout_row(&l, 24);
    hive_rect_t row = hive_layout_row(&l, 72);
    hive_ui_widget(ui, ID_PID)->rect = hive_grid_cell(row, 2, 10, 0);
    hive_ui_widget(ui, ID_SIGNAL)->rect = hive_grid_cell(row, 2, 10, 1);
    hive_ui_widget(ui, ID_SEND)->rect = hive_layout_row(&l, 36);
    hive_ui_widget(ui, ID_STATUS)->rect = hive_layout_row(&l, 28);
}

static void send_signal(hive_ui_t *ui) {
    HI32 selected = 0;
    int pid = atoi(pid_buf);
    int sigs[] = { 15, 9, 2 };
    hive_ui_get_value(ui, ID_SIGNAL, &selected);
    if (pid <= 0 || pid == hax_pid()) {
        strcpy(status_buf, "拒绝：请输入其他有效 PID");
    } else {
        int rc = hive_process_signal(pid, sigs[selected]);
        snprintf(status_buf, sizeof(status_buf),
                 rc == 0 ? "已向 PID %d 发送信号 %d" : "发送失败：PID %d",
                 pid, sigs[selected]);
    }
    hive_ui_set_text(ui, ID_STATUS, status_buf);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int width = 430, height = 300;
    if (hive_window_open("任务管理器", width, height) < 0) return 1;
    hive_ui_t ui;
    hive_ui_init(&ui);
    snprintf(self_buf, sizeof(self_buf), "当前应用 PID：%d", hax_pid());
    hive_ui_add_label(&ui, ID_TITLE, hive_rect(0, 0, 1, 1),
                      "进程信号控制");
    hive_ui_add_label(&ui, ID_SELF, hive_rect(0, 0, 1, 1), self_buf);
    hive_ui_add_textbox(&ui, ID_PID, hive_rect(0, 0, 1, 1),
                        pid_buf, sizeof(pid_buf));
    hive_ui_add_list(&ui, ID_SIGNAL, hive_rect(0, 0, 1, 1),
                     signals, 3, 0);
    hive_ui_add_button(&ui, ID_SEND, hive_rect(0, 0, 1, 1), "发送信号");
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
            else if ((event.type == HIVE_EVENT_CLICK &&
                      event.widget_id == ID_SEND) ||
                     event.type == HIVE_EVENT_SUBMIT) send_signal(&ui);
        }
        hive_window_clear(ui.theme.window_bg);
        hive_ui_draw(&ui);
        hive_window_present();
        hive_yield();
    }
    hive_window_close();
    return 0;
}
