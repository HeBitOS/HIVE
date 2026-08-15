/**
 * @file    hax_tui.h
 * @brief   HAX 终端（TUI）控件 API v1（纯用户态、无新增系统调用）
 *
 * 与 hax_widgets.h 对应，本文件为文本终端应用提供一组轻量控件：分隔线、
 * 进度条、表格、边框盒、菜单、确认、输入行和复选。全部为 static inline，
 * 不使用全局变量或动态内存；输出只依赖 stdout（hax_print / fputs），输入
 * 只依赖 stdin（hax_input / fgets），因此在 HBOS 内核控制台和图形终端中
 * 行为一致。
 *
 * 渲染风格刻意使用纯 ASCII（+ - | = [ ]），保证在 8×16 控制台字体和图形
 * 终端中都不会出现缺字。对齐按“显示格”计算：CJK/全角字符占 2 格，其余
 * 占 1 格，截断不会切断 UTF-8 码点。
 *
 * 输入约定：所有交互控件都是行驱动的（hax_input 读一整行）。空行表示
 * 取默认值；`q`/`Q` 取消（返回 -1）；stdin 读到 EOF 时同样返回 -1，绝不
 * 死循环。菜单支持数字（1..N）或快捷字母（匹配任一选项的首个 ASCII 字符，
 * 不区分大小写）选择。
 *
 * @code
 *   const char *const items[] = {"启动服务", "停止服务", "查看状态"};
 *   HI32 sel = hax_tui_menu("服务管理", items, 3, NULL);
 *   if (sel < 0) return 0;
 *   hax_tui_progress("加载", 60, 0, 100, 30, 1);
 * @endcode
 */
#ifndef HBOS_HAX_TUI_H
#define HBOS_HAX_TUI_H

#include <hax.h>

#define HAX_TUI_ABI_MAJOR 1u
#define HAX_TUI_ABI_MINOR 0u

/** 对齐辅助用的最大列宽（显示格）。 */
#define HAX_TUI_MAX_CELLS 64

/** 表格最大列数（渲染时标题行指针数组的容量）。 */
#define HAX_TUI_MAX_COLS 16

/* ── 显示宽度与 UTF-8 辅助 ───────────────────────────────────────── */

/** 判断码点是否按 2 格显示（CJK 表意文字、全角标点、谚文等）。 */
static inline int hax_tui_cp_wide(HU32 cp) {
    return (cp >= 0x1100 && cp <= 0x115F) ||   /* 谚文 jamo 首音 */
           (cp >= 0x2E80 && cp <= 0x303E) ||   /* CJK 部首/标点 */
           (cp >= 0x3040 && cp <= 0x33FF) ||   /* 假名/注音/全角符号 */
           (cp >= 0x3400 && cp <= 0x4DBF) ||   /* CJK 扩展 A */
           (cp >= 0x4E00 && cp <= 0x9FFF) ||   /* CJK 统一表意文字 */
           (cp >= 0xA000 && cp <= 0xA4CF) ||   /* 彝文音节 */
           (cp >= 0xAC00 && cp <= 0xD7A3) ||   /* 谚文音节 */
           (cp >= 0xF900 && cp <= 0xFAFF) ||   /* CJK 兼容表意文字 */
           (cp >= 0xFE30 && cp <= 0xFE4F) ||   /* CJK 兼容形式 */
           (cp >= 0xFF00 && cp <= 0xFF60) ||   /* 全角 ASCII/标点 */
           (cp >= 0xFFE0 && cp <= 0xFFE6);     /* 全角符号 */
}

/** 从 s 解码下一个码点，返回其 UTF-8 字节数（损坏字节按 1 字节跳过）。 */
static inline HI32 hax_tui_utf8_next_cp(const char *s, HU32 *cp) {
    if (!s || !cp) return 0;
    const HU8 *p = (const HU8 *)s;
    HU32 c = p[0];
    if (c < 0x80) { *cp = c; return 1; }
    HI32 n = 1;
    if ((c & 0xE0) == 0xC0)      { *cp = c & 0x1F; n = 2; }
    else if ((c & 0xF0) == 0xE0) { *cp = c & 0x0F; n = 3; }
    else if ((c & 0xF8) == 0xF0) { *cp = c & 0x07; n = 4; }
    else { *cp = 0xFFFD; return 1; }
    for (HI32 i = 1; i < n; i++) {
        if (!p[i] || ((p[i] & 0xC0u) != 0x80u)) { *cp = 0xFFFD; return i; }
        *cp = (*cp << 6) | (p[i] & 0x3Fu);
    }
    return n;
}

/** 返回字符串占用的显示格数（CJK/全角按 2 格）。 */
static inline HI32 hax_tui_utf8_cells(const char *s) {
    HI32 cells = 0;
    if (!s) return 0;
    while (*s) {
        HU32 cp;
        HI32 n = hax_tui_utf8_next_cp(s, &cp);
        cells += hax_tui_cp_wide(cp) ? 2 : 1;
        s += n;
    }
    return cells;
}

/** 把 s 截断到最多 cells 个显示格，写入 out（NUL 结尾，不切断码点）。
 * 返回写入的字节数。out 至少需要 cells*4+1 字节。 */
static inline HI32 hax_tui_fit(const char *s, HI32 cells, char *out, HI32 cap) {
    if (!s) s = "";
    if (!out || cap < 1) return 0;
    HI32 used = 0, remaining = cells;
    while (*s && remaining > 0 && used < cap - 1) {
        HU32 cp;
        HI32 n = hax_tui_utf8_next_cp(s, &cp);
        HI32 w = hax_tui_cp_wide(cp) ? 2 : 1;
        if (w > remaining) break;
        for (HI32 i = 0; i < n && used < cap - 1; i++) out[used++] = s[i];
        s += n;
        remaining -= w;
    }
    out[used] = 0;
    return used;
}

/** 把 s 按 cells 格左对齐或右对齐写入 out（超宽截断），返回写入字节数。
 * 调用方需保证 cells <= HAX_TUI_MAX_CELLS 且 out 至少有 cells*5+1 字节。 */
static inline HI32 hax_tui_pad_cells(const char *s, HI32 cells, int right_align,
                                     char *out, HI32 cap) {
    if (!out || cap < 1) return 0;
    if (cells < 1) { out[0] = 0; return 0; }
    if (cells > HAX_TUI_MAX_CELLS) cells = HAX_TUI_MAX_CELLS;
    char tmp[HAX_TUI_MAX_CELLS * 4 + 1];
    hax_tui_fit(s, cells, tmp, (HI32)sizeof(tmp));
    HI32 text_cells = hax_tui_utf8_cells(tmp);
    HI32 pad = cells - text_cells;
    if (pad < 0) pad = 0;
    HI32 pos = 0;
    if (right_align) {
        for (HI32 i = 0; i < pad && pos < cap - 1; i++) out[pos++] = ' ';
    }
    for (HI32 i = 0; tmp[i] && pos < cap - 1; i++) out[pos++] = tmp[i];
    if (!right_align) {
        for (HI32 i = 0; i < pad && pos < cap - 1; i++) out[pos++] = ' ';
    }
    out[pos] = 0;
    return pos;
}

/* ── 输出控件：分隔线 / 进度条 / 边框盒 / 表格 ────────────────────── */

/** 渲染纯横线（默认字符 '-'），返回字节数。 */
static inline HI32 hax_tui_hline_to(char ch, HI32 width, char *out, HI32 cap) {
    if (!out || cap < 1) return 0;
    if (width < 0) width = 0;
    if (!ch) ch = '-';
    HI32 pos = 0;
    for (HI32 i = 0; i < width && pos < cap - 1; i++) out[pos++] = ch;
    out[pos] = 0;
    return pos;
}

static inline void hax_tui_hline(char ch, HI32 width) {
    char buf[HAX_TUI_MAX_CELLS * 2 + 2];
    hax_tui_hline_to(ch, width, buf, (HI32)sizeof(buf));
    fputs(buf, stdout);
    fputc('\n', stdout);
}

/** 渲染带标题分隔线：`===== 标题 =====`，返回字节数。 */
static inline HI32 hax_tui_rule_to(const char *title, HI32 width,
                                   char *out, HI32 cap) {
    if (!out || cap < 1) return 0;
    if (width < 4) width = 4;
    if (!title) title = "";
    HI32 title_cells = hax_tui_utf8_cells(title);
    HI32 side = width - title_cells - 2;
    if (side < 0) side = 0;
    HI32 left = side / 2;
    HI32 right = side - left;
    HI32 pos = 0;
    for (HI32 i = 0; i < left && pos < cap - 1; i++) out[pos++] = '=';
    if (*title && pos < cap - 1) out[pos++] = ' ';
    for (HI32 i = 0; title[i] && pos < cap - 1; i++) out[pos++] = title[i];
    if (*title && pos < cap - 1) out[pos++] = ' ';
    for (HI32 i = 0; i < right && pos < cap - 1; i++) out[pos++] = '=';
    out[pos] = 0;
    return pos;
}

static inline void hax_tui_rule(const char *title, HI32 width) {
    char buf[HAX_TUI_MAX_CELLS * 4 + 4];
    hax_tui_rule_to(title, width, buf, (HI32)sizeof(buf));
    fputs(buf, stdout);
    fputc('\n', stdout);
}

/** 渲染进度条到 out：`label [====----] 45%`。
 * in_place 为 1 时以 '\r' 开头，便于原地刷新（依赖终端支持 \r；
 * HBOS 图形终端会丢弃 \r，退化为逐行输出）。返回字节数。 */
static inline HI32 hax_tui_progress_to(const char *label, HI32 value, HI32 minv,
                                       HI32 maxv, HI32 width, int in_place,
                                       char *out, HI32 cap) {
    if (!out || cap < 1) return 0;
    if (maxv <= minv) maxv = minv + 1;
    if (value < minv) value = minv;
    if (value > maxv) value = maxv;
    if (width < 4) width = 4;
    HI32 range = maxv - minv;
    HI32 filled = (HI32)(((HI64)(value - minv) * (HI64)(width - 2)) / range);
    if (filled < 0) filled = 0;
    if (filled > width - 2) filled = width - 2;
    HI32 pct = (HI32)(((HI64)(value - minv) * 100) / range);
    HI32 pos = 0;
    if (in_place && pos < cap - 1) out[pos++] = '\r';
    if (label) {
        for (HI32 i = 0; label[i] && pos < cap - 1; i++) out[pos++] = label[i];
        if (*label && pos < cap - 1) out[pos++] = ' ';
    }
    if (pos < cap - 1) out[pos++] = '[';
    for (HI32 i = 0; i < width - 2 && pos < cap - 1; i++)
        out[pos++] = (i < filled) ? '=' : '-';
    if (pos < cap - 1) out[pos++] = ']';
    char num[16];
    int n = snprintf(num, sizeof(num), " %d%%", (int)pct);
    if (n < 0) n = 0;
    for (HI32 i = 0; i < n && pos < cap - 1; i++) out[pos++] = num[i];
    out[pos] = 0;
    return pos;
}

/** 打印进度条；in_place 时不换行（完成时调用方自行补一个换行）。 */
static inline void hax_tui_progress(const char *label, HI32 value, HI32 minv,
                                    HI32 maxv, HI32 width, int in_place) {
    char buf[HAX_TUI_MAX_CELLS * 4 + 32];
    hax_tui_progress_to(label, value, minv, maxv, width, in_place,
                        buf, (HI32)sizeof(buf));
    fputs(buf, stdout);
    if (!in_place) fputc('\n', stdout);
}

/** 渲染一行边框盒内容：`| text |`（按 max_cells 补齐内宽）。返回字节数。 */
static inline HI32 hax_tui_box_line_to(const char *text, HI32 max_cells,
                                       char *out, HI32 cap) {
    if (!out || cap < 1) return 0;
    if (max_cells < 1) max_cells = 1;
    char body[HAX_TUI_MAX_CELLS * 4 + 1];
    hax_tui_pad_cells(text, max_cells, 0, body, (HI32)sizeof(body));
    HI32 pos = 0;
    if (pos < cap - 1) out[pos++] = '|';
    if (pos < cap - 1) out[pos++] = ' ';
    for (HI32 i = 0; body[i] && pos < cap - 1; i++) out[pos++] = body[i];
    if (pos < cap - 1) out[pos++] = ' ';
    if (pos < cap - 1) out[pos++] = '|';
    out[pos] = 0;
    return pos;
}

/** 打印 ASCII 边框盒；title 显示在顶边（如 `+-- 标题 --+`）。 */
static inline void hax_tui_box(const char *title, const char *const *lines,
                               HI32 line_count) {
    if (!title) title = "";
    if (!lines || line_count < 0) line_count = 0;
    HI32 max_cells = hax_tui_utf8_cells(title);
    for (HI32 i = 0; i < line_count; i++)
        if (lines[i] && hax_tui_utf8_cells(lines[i]) > max_cells)
            max_cells = hax_tui_utf8_cells(lines[i]);
    if (max_cells < 1) max_cells = 1;
    if (max_cells > HAX_TUI_MAX_CELLS) max_cells = HAX_TUI_MAX_CELLS;

    char line[HAX_TUI_MAX_CELLS * 5 + 8];
    HI32 pos = 0;
    line[pos++] = '+';
    line[pos++] = '-';
    if (*title) {
        for (HI32 i = 0; title[i] && pos < (HI32)sizeof(line) - 1; i++)
            line[pos++] = title[i];
        line[pos++] = '-';
    }
    HI32 top = pos + max_cells + 1;            /* 右半边的 '-' 数 */
    if (top < 3) top = 3;
    for (HI32 i = pos; i < top && pos < (HI32)sizeof(line) - 1; i++)
        line[pos++] = '-';
    line[pos++] = '+';
    line[pos] = 0;
    fputs(line, stdout);
    fputc('\n', stdout);

    for (HI32 i = 0; i < line_count; i++) {
        hax_tui_box_line_to(lines[i] ? lines[i] : "", max_cells,
                            line, (HI32)sizeof(line));
        fputs(line, stdout);
        fputc('\n', stdout);
    }

    pos = 0;
    line[pos++] = '+';
    for (HI32 i = 0; i < max_cells + 2 && pos < (HI32)sizeof(line) - 1; i++)
        line[pos++] = '-';
    line[pos++] = '+';
    line[pos] = 0;
    fputs(line, stdout);
    fputc('\n', stdout);
}

/** 表格列描述。width 为列宽（显示格），right_align 用于数值列。 */
typedef struct {
    const char *title;    /**< 列标题 */
    HI32 width;           /**< 列宽（显示格，1..HAX_TUI_MAX_CELLS） */
    int right_align;      /**< 1 = 右对齐 */
} hax_tui_column_t;

/** 渲染表格分隔线 `+----+----+` 到 out，返回字节数。 */
static inline HI32 hax_tui_table_sep_to(const hax_tui_column_t *cols,
                                        HI32 ncols, char *out, HI32 cap) {
    if (!out || cap < 1) return 0;
    if (!cols || ncols < 1) { out[0] = 0; return 0; }
    if (ncols > HAX_TUI_MAX_COLS) ncols = HAX_TUI_MAX_COLS;
    HI32 pos = 0;
    for (HI32 c = 0; c < ncols && pos < cap - 1; c++) {
        out[pos++] = '+';
        HI32 w = cols[c].width;
        if (w < 1) w = 1;
        if (w > HAX_TUI_MAX_CELLS) w = HAX_TUI_MAX_CELLS;
        for (HI32 i = 0; i < w + 2 && pos < cap - 1; i++) out[pos++] = '-';
    }
    if (pos < cap - 1) out[pos++] = '+';
    out[pos] = 0;
    return pos;
}

/** 渲染一行表格数据 `| a | b |` 到 out；cells 长度至少 ncols。返回字节数。 */
static inline HI32 hax_tui_table_row_to(const hax_tui_column_t *cols,
                                        HI32 ncols,
                                        const char *const *cells,
                                        char *out, HI32 cap) {
    if (!out || cap < 1) return 0;
    if (!cols || ncols < 1) { out[0] = 0; return 0; }
    if (ncols > HAX_TUI_MAX_COLS) ncols = HAX_TUI_MAX_COLS;
    HI32 pos = 0;
    for (HI32 c = 0; c < ncols && pos < cap - 1; c++) {
        out[pos++] = '|';
        out[pos++] = ' ';
        char body[HAX_TUI_MAX_CELLS * 4 + 1];
        hax_tui_pad_cells(cells ? cells[c] : "", cols[c].width,
                          cols[c].right_align != 0, body, (HI32)sizeof(body));
        for (HI32 i = 0; body[i] && pos < cap - 1; i++) out[pos++] = body[i];
        out[pos++] = ' ';
    }
    if (pos < cap - 1) out[pos++] = '|';
    out[pos] = 0;
    return pos;
}

/** 打印整张表格（标题行 + 分隔线 + 数据行 + 底线）。rows 为
 * rows[r*ncols + c] 的行优先数据。 */
static inline void hax_tui_table(const hax_tui_column_t *cols, HI32 ncols,
                                 const char *const *rows, HI32 nrows) {
    char line[HAX_TUI_MAX_COLS * (HAX_TUI_MAX_CELLS * 4 + 4) + 16];
    if (!cols || ncols < 1) return;
    if (nrows < 0) nrows = 0;
    if (nrows > 0 && !rows) nrows = 0;
    if (ncols > HAX_TUI_MAX_COLS) ncols = HAX_TUI_MAX_COLS;

    const char *header_cells[HAX_TUI_MAX_COLS];
    for (HI32 c = 0; c < ncols; c++)
        header_cells[c] = cols[c].title ? cols[c].title : "";
    hax_tui_table_row_to(cols, ncols, header_cells, line, (HI32)sizeof(line));
    fputs(line, stdout);
    fputc('\n', stdout);
    hax_tui_table_sep_to(cols, ncols, line, (HI32)sizeof(line));
    fputs(line, stdout);
    fputc('\n', stdout);
    for (HI32 r = 0; r < nrows; r++) {
        hax_tui_table_row_to(cols, ncols, rows + (HI64)r * ncols,
                             line, (HI32)sizeof(line));
        fputs(line, stdout);
        fputc('\n', stdout);
    }
    hax_tui_table_sep_to(cols, ncols, line, (HI32)sizeof(line));
    fputs(line, stdout);
    fputc('\n', stdout);
}

/* ── 交互控件：菜单 / 确认 / 输入 / 复选 ──────────────────────────── */

/** 解析一行菜单输入。
 *  数字 1..count      → 返回选中下标 0..count-1
 *  'q'/'Q'            → 返回 -1（取消）
 *  快捷字母           → 返回首个 ASCII 字符匹配的选项下标（不区分大小写）
 *  其他（含越界）      → 返回 -2（无效，调用方应重新提示）
 */
static inline HI32 hax_tui_menu_parse(const char *line,
                                      const char *const *items, HI32 count) {
    if (!line || !items || count <= 0) return -2;
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 0) return -2;
    if (*p == 'q' || *p == 'Q') return -1;
    if (*p >= '0' && *p <= '9') {
        HI32 n = 0;
        while (*p >= '0' && *p <= '9') {
            n = n * 10 + (*p - '0');
            if (n > 1000000) return -2;
            p++;
        }
        while (*p == ' ' || *p == '\t') p++;
        if (*p != 0) return -2;
        if (n >= 1 && n <= count) return n - 1;
        return -2;
    }
    char c = *p;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (HI32 i = 0; i < count; i++) {
        const char *it = items[i];
        if (!it || !*it) continue;
        char ic = *it;
        if (ic >= 'a' && ic <= 'z') ic = (char)(ic - 'a' + 'A');
        if (ic == c) return i;
    }
    return -2;
}

/** 数字菜单：打印标题与编号选项，循环读入直到选中/取消/EOF。
 *  prompt 为 printf 格式串，可含一个 %d（选项总数）；传 NULL 用默认。
 *  返回选中下标 0..count-1；取消或 EOF 返回 -1。 */
static inline HI32 hax_tui_menu(const char *title, const char *const *items,
                                HI32 count, const char *prompt) {
    if (!items || count <= 0) return -1;
    if (!title) title = "";
    if (!prompt) prompt = "请选择 (1-%d, q=退出): ";
    hax_println(title);
    for (HI32 i = 0; i < count; i++)
        hax_printf("  %d. %s\n", (int)(i + 1), items[i] ? items[i] : "");
    for (;;) {
        hax_printf(prompt, (int)count);
        char line[64];
        if (hax_input(line, (int)sizeof(line)) < 0) return -1;  /* EOF */
        HI32 sel = hax_tui_menu_parse(line, items, count);
        if (sel >= 0) return sel;
        if (sel == -1) return -1;
        hax_printf("无效输入，请输入 1-%d 或 q。\n", (int)count);
    }
}

/** 解析一行确认输入。空行 → 取默认；y/Y → 1；n/N → 0；q/Q → -1；其余 -2。 */
static inline int hax_tui_confirm_parse(const char *line, int default_yes) {
    if (!line) return default_yes ? 1 : 0;
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 0) return default_yes ? 1 : 0;
    char c = *p;
    if (c == 'y' || c == 'Y') return 1;
    if (c == 'n' || c == 'N') return 0;
    if (c == 'q' || c == 'Q') return -1;
    return -2;
}

/** 确认提示 `prompt [Y/n]: `；回车取默认。返回 1/0，取消或 EOF 返回 -1。 */
static inline int hax_tui_confirm(const char *prompt, int default_yes) {
    if (!prompt) prompt = "确认";
    for (;;) {
        hax_printf("%s [%c/%c]: ", prompt,
                   default_yes ? 'Y' : 'y', default_yes ? 'n' : 'N');
        char line[32];
        if (hax_input(line, (int)sizeof(line)) < 0) return -1;
        int r = hax_tui_confirm_parse(line, default_yes);
        if (r >= 0) return r;
        if (r == -1) return -1;
        hax_println("请输入 y 或 n。");
    }
}

/** 把 src 的 src_len 字节复制到 buf（上限 cap-1，按 UTF-8 边界截断，
 *  不切断码点）。返回写入长度（不含 NUL）。 */
static inline HI32 hax_tui_clamp_copy(const char *src, HI32 src_len,
                                      char *buf, HI32 cap) {
    if (!buf || cap < 1) return -1;
    if (!src) src = "";
    if (src_len < 0) src_len = 0;
    HI32 out_len = src_len;
    if (out_len > cap - 1) out_len = cap - 1;
    /* 别在 UTF-8 序列中间截断。 */
    while (out_len > 0 && (((HU8)src[out_len] & 0xC0u) == 0x80u)) out_len--;
    if (out_len > 0) memcpy(buf, src, (size_t)out_len);
    buf[out_len] = 0;
    return out_len;
}

/** 带提示与默认值的输入行。空行把 dflt 写入 buf（按 UTF-8 边界截断到
 *  cap-1）。返回写入长度（不含 NUL），EOF 返回 -1 且 buf[0]=0。 */
static inline HI32 hax_tui_input(const char *prompt, const char *dflt,
                                 char *buf, HI32 cap) {
    if (!buf || cap < 1) return -1;
    if (!prompt) prompt = "";
    hax_print(prompt);
    if (dflt && *dflt) hax_printf(" [%s]", dflt);
    hax_print(": ");
    char line[256];
    HI32 n = (HI32)hax_input(line, (HI32)sizeof(line));
    if (n < 0) { buf[0] = 0; return -1; }
    const char *src = line;
    HI32 src_len = n;
    if (n == 0 && dflt) { src = dflt; src_len = (HI32)strlen(dflt); }
    return hax_tui_clamp_copy(src, src_len, buf, cap);
}

/** 复选行：先打印当前状态 `[x] label`，再循环询问切换。
 *  返回切换后的状态（0/1），EOF 返回 -1。 */
static inline int hax_tui_checkbox(const char *label, int checked,
                                   const char *prompt) {
    if (!label) label = "";
    if (!prompt) prompt = "切换 (y/n, 回车保持): ";
    for (;;) {
        hax_printf("[%c] %s\n", checked ? 'x' : ' ', label);
        hax_print(prompt);
        char line[32];
        if (hax_input(line, (int)sizeof(line)) < 0) return -1;
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) return checked;
        if (*p == 'y' || *p == 'Y') return 1;
        if (*p == 'n' || *p == 'N') return 0;
        if (*p == 'q' || *p == 'Q') return -1;
        hax_println("请输入 y / n。");
    }
}

#endif /* HBOS_HAX_TUI_H */
