/* 图片查看器 —— 独立用户态 BMP 查看器（未压缩 24/32 位）。 */
#include <hive.h>

HIVE_APP("imgview", "图片查看器");

#define IMG_MAX_W 512
#define IMG_MAX_H 384
#define IMG_RAW_CAP (IMG_MAX_W * IMG_MAX_H * 4 + 4096)
enum { ID_PATH = 1, ID_LOAD, ID_IMAGE, ID_STATUS };
static char path_buf[256];
static unsigned char raw[IMG_RAW_CAP];
static HU32 pixels[IMG_MAX_W * IMG_MAX_H];
static char status_buf[120] = "支持未压缩 24/32 位 BMP";
static int image_w, image_h;

static HU16 le16(const unsigned char *p) {
    return (HU16)(p[0] | ((HU16)p[1] << 8));
}
static HU32 le32(const unsigned char *p) {
    return (HU32)p[0] | ((HU32)p[1] << 8) |
           ((HU32)p[2] << 16) | ((HU32)p[3] << 24);
}

static int decode_bmp(long n) {
    if (n < 54 || raw[0] != 'B' || raw[1] != 'M') return -1;
    HU32 offset = le32(raw + 10);
    int w = (int)le32(raw + 18);
    int signed_h = (int)le32(raw + 22);
    int h = signed_h < 0 ? -signed_h : signed_h;
    int bpp = le16(raw + 28);
    HU32 compression = le32(raw + 30);
    if (w <= 0 || h <= 0 || w > IMG_MAX_W || h > IMG_MAX_H ||
        (bpp != 24 && bpp != 32) || compression != 0) return -1;
    HU32 stride = (HU32)(((HU32)w * (HU32)(bpp / 8) + 3) & ~3u);
    if ((HU64)offset + (HU64)stride * (HU64)h > (HU64)n) return -1;
    for (int y = 0; y < h; y++) {
        int source_y = signed_h > 0 ? h - 1 - y : y;
        const unsigned char *row = raw + offset + (HU32)source_y * stride;
        for (int x = 0; x < w; x++) {
            const unsigned char *p = row + x * (bpp / 8);
            pixels[y * w + x] = 0xFF000000u |
                ((HU32)p[2] << 16) | ((HU32)p[1] << 8) | p[0];
        }
    }
    image_w = w; image_h = h;
    return 0;
}

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
    hive_ui_widget(ui, ID_IMAGE)->rect = hive_layout_row(&l, height - 112);
    hive_ui_widget(ui, ID_STATUS)->rect = hive_layout_row(&l, 24);
}

static void load_image(hive_ui_t *ui) {
    long n = hax_read_file(path_buf, raw, sizeof(raw));
    if (n < 0) {
        snprintf(status_buf, sizeof(status_buf), "无法读取：%s", path_buf);
    } else if (decode_bmp(n) < 0) {
        image_w = image_h = 0;
        snprintf(status_buf, sizeof(status_buf),
                 "格式不支持：需要 <=512x384 的未压缩 24/32 位 BMP");
    } else {
        snprintf(status_buf, sizeof(status_buf), "%s — %d x %d",
                 path_buf, image_w, image_h);
    }
    hax_widget_t *image = hive_ui_widget(ui, ID_IMAGE);
    image->image_width = image_w;
    image->image_height = image_h;
    image->image_stride = image_w;
    hive_ui_set_text(ui, ID_STATUS, status_buf);
}

int main(int argc, char **argv) {
    if (argc > 1 && argv[1]) {
        strncpy(path_buf, argv[1], sizeof(path_buf) - 1);
        path_buf[sizeof(path_buf) - 1] = 0;
    }
    int width = 600, height = 500;
    if (hive_window_open("图片查看器", width, height) < 0) return 1;
    hive_ui_t ui;
    hive_ui_init(&ui);
    hive_ui_add_textbox(&ui, ID_PATH, hive_rect(0, 0, 1, 1),
                        path_buf, sizeof(path_buf));
    hive_ui_add_button(&ui, ID_LOAD, hive_rect(0, 0, 1, 1), "打开");
    hive_ui_add_image(&ui, ID_IMAGE, hive_rect(0, 0, 1, 1),
                      pixels, 1, 1, 1);
    hive_ui_add_label(&ui, ID_STATUS, hive_rect(0, 0, 1, 1), status_buf);
    layout(&ui, width, height);
    if (path_buf[0]) load_image(&ui);
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
                      event.widget_id == ID_PATH)) load_image(&ui);
        }
        hive_window_clear(ui.theme.window_bg);
        hive_ui_draw(&ui);
        hive_window_present();
        hive_yield();
    }
    hive_window_close();
    return 0;
}
