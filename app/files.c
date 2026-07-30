/* 文件管理器 —— 仅使用 POSIX dirent/stat 公开接口。 */
#include <hive.h>
#include "libc/dirent.h"
#include "libc/sys/stat.h"

HIVE_APP("files", "文件管理器");

#define FILE_ROWS 16
enum { ID_PATH = 1, ID_LIST, ID_OPEN, ID_UP, ID_REFRESH, ID_STATUS };

static char path_buf[256] = "/";
static char row_buf[FILE_ROWS][280];
static char name_buf[FILE_ROWS][256];
static unsigned char type_buf[FILE_ROWS];
static const char *rows[FILE_ROWS];
static char status_buf[96] = "选择目录后按“打开”";
static int row_count;

static void layout(hive_ui_t *ui, int width, int height) {
    hive_layout_t l;
    hive_layout_begin(&l, hive_rect(0, 0, width, height), 14, 8);
    hive_ui_widget(ui, ID_PATH)->rect = hive_layout_row(&l, 30);
    int list_h = height - 132;
    if (list_h < 80) list_h = 80;
    hive_ui_widget(ui, ID_LIST)->rect = hive_layout_row(&l, list_h);
    hive_rect_t actions = hive_layout_row(&l, 34);
    hive_ui_widget(ui, ID_OPEN)->rect = hive_grid_cell(actions, 3, 8, 0);
    hive_ui_widget(ui, ID_UP)->rect = hive_grid_cell(actions, 3, 8, 1);
    hive_ui_widget(ui, ID_REFRESH)->rect = hive_grid_cell(actions, 3, 8, 2);
    hive_ui_widget(ui, ID_STATUS)->rect = hive_layout_row(&l, 24);
}

static void normalize_path(void) {
    size_t n = strlen(path_buf);
    if (n == 0) strcpy(path_buf, "/");
    while (n > 1 && path_buf[n - 1] == '/') path_buf[--n] = 0;
}

static void refresh(hive_ui_t *ui) {
    normalize_path();
    row_count = 0;
    DIR *dir = opendir(path_buf);
    if (!dir) {
        snprintf(status_buf, sizeof(status_buf), "无法打开：%s", path_buf);
    } else {
        struct dirent *entry;
        while (row_count < FILE_ROWS && (entry = readdir(dir)) != NULL) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                continue;
            strncpy(name_buf[row_count], entry->d_name,
                    sizeof(name_buf[row_count]) - 1);
            name_buf[row_count][sizeof(name_buf[row_count]) - 1] = 0;
            type_buf[row_count] = entry->d_type;
            snprintf(row_buf[row_count], sizeof(row_buf[row_count]), "%s %s",
                     entry->d_type == DT_DIR ? "[目录]" : "[文件]",
                     entry->d_name);
            rows[row_count] = row_buf[row_count];
            row_count++;
        }
        closedir(dir);
        snprintf(status_buf, sizeof(status_buf), "显示前 %d 项", row_count);
    }
    hax_widget_t *list = hive_ui_widget(ui, ID_LIST);
    list->items = rows; list->item_count = row_count;
    list->selected = row_count ? 0 : -1;
    hive_ui_set_text(ui, ID_STATUS, status_buf);
}

static void enter_selected(hive_ui_t *ui) {
    HI32 selected = -1;
    hive_ui_get_value(ui, ID_LIST, &selected);
    if (selected < 0 || selected >= row_count) return;
    if (type_buf[selected] != DT_DIR) {
        char full[320];
        snprintf(full, sizeof(full), "%s%s%s", path_buf,
                 !strcmp(path_buf, "/") ? "" : "/", name_buf[selected]);
        struct stat info;
        if (stat(full, &info) == 0)
            snprintf(status_buf, sizeof(status_buf), "%s：%ld 字节",
                     name_buf[selected], (long)info.st_size);
        else snprintf(status_buf, sizeof(status_buf), "文件：%s", name_buf[selected]);
        hive_ui_set_text(ui, ID_STATUS, status_buf);
        return;
    }
    char next[sizeof(path_buf)];
    snprintf(next, sizeof(next), "%s%s%s", path_buf,
             !strcmp(path_buf, "/") ? "" : "/", name_buf[selected]);
    strncpy(path_buf, next, sizeof(path_buf) - 1);
    path_buf[sizeof(path_buf) - 1] = 0;
    refresh(ui);
}

static void go_up(hive_ui_t *ui) {
    normalize_path();
    if (!strcmp(path_buf, "/")) return;
    char *slash = strrchr(path_buf, '/');
    if (!slash || slash == path_buf) strcpy(path_buf, "/");
    else *slash = 0;
    refresh(ui);
}

int main(int argc, char **argv) {
    if (argc > 1 && argv[1] && argv[1][0]) {
        strncpy(path_buf, argv[1], sizeof(path_buf) - 1);
        path_buf[sizeof(path_buf) - 1] = 0;
    }
    int width = 560, height = 480;
    if (hive_window_open("文件管理器", width, height) < 0) return 1;
    hive_ui_t ui;
    hive_ui_init(&ui);
    hive_ui_add_textbox(&ui, ID_PATH, hive_rect(0, 0, 1, 1),
                        path_buf, sizeof(path_buf));
    hive_ui_add_list(&ui, ID_LIST, hive_rect(0, 0, 1, 1), rows, 0, -1);
    hive_ui_add_button(&ui, ID_OPEN, hive_rect(0, 0, 1, 1), "打开");
    hive_ui_add_button(&ui, ID_UP, hive_rect(0, 0, 1, 1), "上级");
    hive_ui_add_button(&ui, ID_REFRESH, hive_rect(0, 0, 1, 1), "刷新");
    hive_ui_add_label(&ui, ID_STATUS, hive_rect(0, 0, 1, 1), status_buf);
    layout(&ui, width, height);
    refresh(&ui);
    int running = 1, old_w = width, old_h = height;
    while (running && hive_window_active(&width, &height)) {
        if (width != old_w || height != old_h) {
            layout(&ui, width, height); old_w = width; old_h = height;
        }
        hive_event_t event;
        while (hive_ui_poll(&ui, &event) != HIVE_EVENT_NONE) {
            if (event.type == HIVE_EVENT_CLOSE) running = 0;
            else if (event.type == HIVE_EVENT_CLICK && event.widget_id == ID_OPEN)
                enter_selected(&ui);
            else if (event.type == HIVE_EVENT_CLICK && event.widget_id == ID_UP)
                go_up(&ui);
            else if ((event.type == HIVE_EVENT_CLICK &&
                      event.widget_id == ID_REFRESH) ||
                     (event.type == HIVE_EVENT_SUBMIT &&
                      event.widget_id == ID_PATH)) refresh(&ui);
        }
        hive_window_clear(ui.theme.window_bg);
        hive_ui_draw(&ui);
        hive_window_present();
        hive_yield();
    }
    hive_window_close();
    return 0;
}
