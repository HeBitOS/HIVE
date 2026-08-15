#include <hive.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_cells_and_fit(void) {
    /* "A中"：A=1 格，中=2 格 */
    CHECK(hax_tui_utf8_cells("A中") == 3);
    CHECK(hax_tui_utf8_cells("") == 0);
    CHECK(hax_tui_utf8_cells(NULL) == 0);
    /* 全角标点按 2 格 */
    CHECK(hax_tui_utf8_cells("：") == 2);

    char out[64];
    /* 截断到 4 格 = 两个汉字 */
    CHECK(hax_tui_fit("中文测试", 4, out, (HI32)sizeof(out)) == 6);
    CHECK(strcmp(out, "中文") == 0);
    /* 截断到 2 格 = 一个汉字，不切断码点 */
    CHECK(hax_tui_fit("中文", 2, out, (HI32)sizeof(out)) == 3);
    CHECK(strcmp(out, "中") == 0);
    /* 超过宽度时不截断 ASCII */
    CHECK(hax_tui_fit("abcd", 8, out, (HI32)sizeof(out)) == 4);
    CHECK(strcmp(out, "abcd") == 0);
    /* 窄容量不越界 */
    CHECK(hax_tui_fit("abcd", 8, out, 3) == 2);
    CHECK(out[0] == 'a' && out[1] == 'b' && out[2] == 0);
    return 0;
}

static int test_pad_cells(void) {
    char out[96];
    /* 左对齐补空格：2 格文本 + 4 空格 = 6 格 */
    CHECK(hax_tui_pad_cells("ab", 6, 0, out, (HI32)sizeof(out)) == 6);
    CHECK(strcmp(out, "ab    ") == 0);
    /* 右对齐：4 空格 + 2 格文本 */
    CHECK(hax_tui_pad_cells("ab", 6, 1, out, (HI32)sizeof(out)) == 6);
    CHECK(strcmp(out, "    ab") == 0);
    /* 汉字按 2 格计：中+ab = 4 格 + 2 空格 = 6 格，7 字节 */
    CHECK(hax_tui_pad_cells("中ab", 6, 0, out, (HI32)sizeof(out)) == 7);
    CHECK(strcmp(out, "中ab  ") == 0);
    /* 超宽截断：6 格放不下 "abcdefgh"，截为 6 字符 */
    CHECK(hax_tui_pad_cells("abcdefgh", 6, 0, out, (HI32)sizeof(out)) == 6);
    CHECK(strcmp(out, "abcdef") == 0);
    return 0;
}

static int test_progress(void) {
    char out[128];
    /* 最小值：全 '-'，0%（宽度 12 → 10 个填充字符） */
    CHECK(hax_tui_progress_to("加载", 0, 0, 100, 12, 0,
                              out, (HI32)sizeof(out)) > 0);
    CHECK(strstr(out, "加载") != NULL);
    CHECK(strstr(out, "[----------]") != NULL);
    CHECK(strstr(out, " 0%") != NULL);
    /* 最大值：全 '='，100% */
    CHECK(hax_tui_progress_to("加载", 100, 0, 100, 12, 0,
                              out, (HI32)sizeof(out)) > 0);
    CHECK(strstr(out, "[==========]") != NULL);
    CHECK(strstr(out, " 100%") != NULL);
    /* 50%：5 个 '=' + 5 个 '-' */
    CHECK(hax_tui_progress_to(NULL, 50, 0, 100, 12, 0,
                              out, (HI32)sizeof(out)) > 0);
    CHECK(strstr(out, "[=====-----]") != NULL);
    CHECK(strstr(out, " 50%") != NULL);
    /* 越界钳制 */
    CHECK(hax_tui_progress_to(NULL, -5, 0, 100, 12, 0,
                              out, (HI32)sizeof(out)) > 0);
    CHECK(strstr(out, " 0%") != NULL);
    CHECK(hax_tui_progress_to(NULL, 999, 0, 100, 12, 0,
                              out, (HI32)sizeof(out)) > 0);
    CHECK(strstr(out, " 100%") != NULL);
    /* in_place 模式以 '\r' 开头 */
    CHECK(hax_tui_progress_to(NULL, 50, 0, 100, 12, 1,
                              out, (HI32)sizeof(out)) > 0);
    CHECK(out[0] == '\r');
    return 0;
}

static int test_table(void) {
    const hax_tui_column_t cols[] = {
        {"名称", 6, 0},
        {"大小", 8, 1},
        {"说明", 12, 0},
    };
    char out[256];
    /* 分隔线 */
    CHECK(hax_tui_table_sep_to(cols, 3, out, (HI32)sizeof(out)) > 0);
    CHECK(strcmp(out, "+--------+----------+--------------+") == 0);

    /* 数据行：名称左对齐、大小右对齐、说明左对齐（每格两侧各有 1 空格） */
    const char *row[] = {"bin", "12345", "系统目录"};
    CHECK(hax_tui_table_row_to(cols, 3, row, out, (HI32)sizeof(out)) > 0);
    CHECK(strcmp(out, "| bin    |    12345 | 系统目录     |") == 0);

    /* 标题行走同一渲染路径 */
    const char *hdr[] = {"名称", "大小", "说明"};
    CHECK(hax_tui_table_row_to(cols, 3, hdr, out, (HI32)sizeof(out)) > 0);
    CHECK(strcmp(out, "| 名称   |     大小 | 说明         |") == 0);
    return 0;
}

static int test_box_line(void) {
    char out[96];
    CHECK(hax_tui_box_line_to("服务", 8, out, (HI32)sizeof(out)) > 0);
    CHECK(strcmp(out, "| 服务     |") == 0);
    CHECK(hax_tui_box_line_to("abc", 8, out, (HI32)sizeof(out)) > 0);
    CHECK(strcmp(out, "| abc      |") == 0);
    return 0;
}

static int test_menu_parse(void) {
    static const char *const items[] = {
        "启动服务",   /* 快捷字母 启（非 ASCII，不参与匹配）*/
        "Stop",       /* 快捷字母 S/s */
        "状态",
    };
    /* 数字选择 */
    CHECK(hax_tui_menu_parse("1", items, 3) == 0);
    CHECK(hax_tui_menu_parse("3", items, 3) == 2);
    CHECK(hax_tui_menu_parse(" 2 ", items, 3) == 1);
    /* 快捷字母（不区分大小写，ASCII 首字符） */
    CHECK(hax_tui_menu_parse("s", items, 3) == 1);
    CHECK(hax_tui_menu_parse("S", items, 3) == 1);
    /* 取消 */
    CHECK(hax_tui_menu_parse("q", items, 3) == -1);
    CHECK(hax_tui_menu_parse("Q", items, 3) == -1);
    /* 无效输入 */
    CHECK(hax_tui_menu_parse("", items, 3) == -2);
    CHECK(hax_tui_menu_parse("9", items, 3) == -2);
    CHECK(hax_tui_menu_parse("0", items, 3) == -2);
    CHECK(hax_tui_menu_parse("abc", items, 3) == -2);
    CHECK(hax_tui_menu_parse("1x", items, 3) == -2);
    CHECK(hax_tui_menu_parse(NULL, items, 3) == -2);
    CHECK(hax_tui_menu_parse("1", NULL, 3) == -2);
    CHECK(hax_tui_menu_parse("1", items, 0) == -2);
    return 0;
}

static int test_confirm_parse(void) {
    CHECK(hax_tui_confirm_parse("y", 0) == 1);
    CHECK(hax_tui_confirm_parse("Y", 0) == 1);
    CHECK(hax_tui_confirm_parse("n", 1) == 0);
    CHECK(hax_tui_confirm_parse("N", 1) == 0);
    CHECK(hax_tui_confirm_parse("", 1) == 1);      /* 空行取默认 */
    CHECK(hax_tui_confirm_parse("", 0) == 0);
    CHECK(hax_tui_confirm_parse("  ", 1) == 1);
    CHECK(hax_tui_confirm_parse("q", 1) == -1);    /* 取消 */
    CHECK(hax_tui_confirm_parse("x", 1) == -2);    /* 无效 */
    CHECK(hax_tui_confirm_parse(NULL, 1) == 1);
    return 0;
}

static int test_input_utf8_clamp(void) {
    char buf[8];
    /* 超宽默认值按 UTF-8 边界截断：cap 8 → 最多 7 字节，
       7 字节会切断"测"，回退到 6 字节 = "中文" */
    CHECK(hax_tui_clamp_copy("中文测试", 12, buf, 8) == 6);
    CHECK(strcmp(buf, "中文") == 0);
    /* 正好放得下 */
    CHECK(hax_tui_clamp_copy("中文", 6, buf, 8) == 6);
    CHECK(strcmp(buf, "中文") == 0);
    /* 短输入原样复制 */
    CHECK(hax_tui_clamp_copy("ab", 2, buf, 8) == 2);
    CHECK(strcmp(buf, "ab") == 0);
    return 0;
}

static int test_hline_rule(void) {
    char out[64];
    CHECK(hax_tui_hline_to('-', 10, out, (HI32)sizeof(out)) == 10);
    CHECK(strcmp(out, "----------") == 0);
    CHECK(hax_tui_hline_to(0, 3, out, (HI32)sizeof(out)) == 3);
    CHECK(strcmp(out, "---") == 0);
    /* 标题分隔线：两边对称补 '=' */
    CHECK(hax_tui_rule_to("TUI", 15, out, (HI32)sizeof(out)) > 0);
    CHECK(strcmp(out, "===== TUI =====") == 0);
    return 0;
}

#define RUN(name, fn) do { \
        int r = (fn); \
        if (r) { hax_printf("%s failed at line %d\n", (name), r); return r; } \
    } while (0)

int main(void) {
    CHECK(HAX_TUI_ABI_MAJOR == 1 && HAX_TUI_ABI_MINOR == 0);
    CHECK(HIVE_TUI_API_MAJOR == 1 && HIVE_TUI_API_MINOR == 0);

    RUN("cells_and_fit", test_cells_and_fit());
    RUN("pad_cells", test_pad_cells());
    RUN("progress", test_progress());
    RUN("table", test_table());
    RUN("box_line", test_box_line());
    RUN("menu_parse", test_menu_parse());
    RUN("confirm_parse", test_confirm_parse());
    RUN("input_utf8_clamp", test_input_utf8_clamp());
    RUN("hline_rule", test_hline_rule());

    hax_printf("HIVE TUI kit tests passed\n");
    return 0;
}