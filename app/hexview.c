/* 十六进制查看器 —— 独立 HAX 文件查看器。 */
#include <hive.h>

HIVE_APP("hexview", "十六进制查看器");

#define HEX_CAP 4096
#define HEX_ROWS 16
enum { ID_PATH = 1, ID_LOAD, ID_LINES, ID_PAGE, ID_STATUS };
static char path_buf[256];
static unsigned char data[HEX_CAP];
static char line_buf[HEX_ROWS][96];
static const char *lines[HEX_ROWS];
static char status_buf[96] = "输入文件路径";
static long data_len;
static int page;

static void layout(hive_ui_t *ui, int width, int height) {
    hive_layout_t l;
    hive_layout_begin(&l, hive_rect(0, 0, width, height), 12, 8);
    hive_rect_t top = hive_layout_row(&l, 30);
    hive_ui_widget(ui, ID_PATH)->rect = hive_grid_cell(top, 4, 8, 0);
    hive_ui_widget(ui, ID_PATH)->rect.w =
        hive_grid_cell(top, 4, 8, 2).x +
        hive_grid_cell(top, 4, 8, 2).w -
        hive_ui_widget(ui, ID_PATH)->rect.x;
    hive_ui_widget(ui, ID_LOAD)->rect = hive_grid_cell(top, 4, 8, 3);
    hive_ui_widget(ui, ID_LINES)->rect = hive_layout_row(&l, height - 132);
    hive_ui_widget(ui, ID_PAGE)->rect = hive_layout_row(&l, 26);
    hive_ui_widget(ui, ID_STATUS)->rect = hive_layout_row(&l, 24);
}

static void format_page(hive_ui_t *ui) {
    static const char hex[] = "0123456789ABCDEF";
    long base = (long)page * HEX_ROWS * 16;
    int count = 0;
    for (int r = 0; r < HEX_ROWS && base + r * 16 < data_len; r++) {
        long off = base + r * 16;
        char *out = line_buf[r];
        int p = snprintf(out, sizeof(line_buf[r]), "%08lX  ", off);
        for (int c = 0; c < 16; c++) {
            long i = off + c;
            if (i < data_len) {
                unsigned v = data[i];
                out[p++] = hex[v >> 4]; out[p++] = hex[v & 15]; out[p++] = ' ';
            } else {
                out[p++] = ' '; out[p++] = ' '; out[p++] = ' ';
            }
        }
        out[p++] = ' ';
        for (int c = 0; c < 16 && off + c < data_len; c++) {
            unsigned v = data[off + c];
            out[p++] = (v >= 32 && v < 127) ? (char)v : '.';
        }
        out[p] = 0;
        lines[count++] = out;
    }
    hax_widget_t *list = hive_ui_widget(ui, ID_LINES);
    list->items = lines; list->item_count = count;
    list->selected = count ? 0 : -1;
    int pages = data_len > 0 ? (int)((data_len + HEX_ROWS * 16 - 1) /
                                     (HEX_ROWS * 16)) : 1;
    hax_widget_t *slider = hive_ui_widget(ui, ID_PAGE);
    slider->max_value = pages - 1;
    hive_ui_set_value(ui, ID_PAGE, page);
}

static void load_file(hive_ui_t *ui) {
    data_len = hax_read_file(path_buf, data, sizeof(data));
    page = 0;
    if (data_len < 0) {
        data_len = 0;
        snprintf(status_buf, sizeof(status_buf), "无法读取：%s", path_buf);
    } else {
        snprintf(status_buf, sizeof(status_buf), "已读取 %ld 字节%s",
                 data_len, data_len == HEX_CAP ? "（已截断）" : "");
    }
    hive_ui_set_text(ui, ID_STATUS, status_buf);
    format_page(ui);
}

int main(int argc, char **argv) {
    if (argc > 1 && argv[1]) {
        strncpy(path_buf, argv[1], sizeof(path_buf) - 1);
        path_buf[sizeof(path_buf) - 1] = 0;
    }
    int width = 720, height = 500;
    if (hive_window_open("十六进制查看器", width, height) < 0) return 1;
    hive_ui_t ui;
    hive_ui_init(&ui);
    hive_ui_add_textbox(&ui, ID_PATH, hive_rect(0, 0, 1, 1),
                        path_buf, sizeof(path_buf));
    hive_ui_add_button(&ui, ID_LOAD, hive_rect(0, 0, 1, 1), "读取");
    hive_ui_add_list(&ui, ID_LINES, hive_rect(0, 0, 1, 1), lines, 0, -1);
    hive_ui_add_slider(&ui, ID_PAGE, hive_rect(0, 0, 1, 1), 0, 0, 0, 1);
    hive_ui_add_label(&ui, ID_STATUS, hive_rect(0, 0, 1, 1), status_buf);
    layout(&ui, width, height);
    if (path_buf[0]) load_file(&ui);
    int running = 1, old_w = width, old_h = height;
    while (running && hive_window_active(&width, &height)) {
        if (width != old_w || height != old_h) {
            layout(&ui, width, height); old_w = width; old_h = height;
        }
        hive_event_t event;
        while (hive_ui_poll(&ui, &event) != HIVE_EVENT_NONE) {
            if (event.type == HIVE_EVENT_CLOSE) running = 0;
            else if ((event.type == HIVE_EVENT_CLICK &&
                      event.widget_id == ID_LOAD) ||
                     (event.type == HIVE_EVENT_SUBMIT &&
                      event.widget_id == ID_PATH)) load_file(&ui);
            else if (event.type == HIVE_EVENT_CHANGE &&
                     event.widget_id == ID_PAGE) {
                page = event.value; format_page(&ui);
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
