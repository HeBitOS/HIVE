/**
 * @file tui.c
 * @brief HIVE TUI 控件展示 —— HAX_KIND_TUI 文本终端应用
 *
 * 演示 hax_tui.h 提供的全部控件：标题分隔线、表格、边框盒、进度条、
 * 菜单、确认、输入行与复选。所有交互都是行驱动的，stdin 读到 EOF
 * （例如被无输入管道启动）时自动退出，不会死循环。
 */
#include <hive.h>

HAX_APP("tui", "HIVE TUI 控件展示", HAX_KIND_TUI);

static const char *const services[] = {
    "启动服务",
    "停止服务",
    "查看状态"
};

static const char *const proc_rows[] = {
    "hive",   "4",  "桌面环境",
    "shell",  "2",  "命令行外壳",
    "hpt",    "1",  "软件包管理器",
    "calc",   "1",  "计算器"
};

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* ── 标题分隔线 ── */
    hax_tui_rule("HIVE TUI Kit 1.0 控件展示", 56);
    hax_println("");

    /* ── 表格 ── */
    const hax_tui_column_t cols[] = {
        {"进程", 8, 0},
        {"计数", 6, 1},
        {"说明", 16, 0},
    };
    hax_println("表格（hax_tui_table）：");
    hax_tui_table(cols, 3, proc_rows, 4);
    hax_println("");

    /* ── 边框盒 ── */
    const char *const box_lines[] = {
        "运行环境: HBOS / HIVE 0.1-beta5-gui.4",
        "语言:     C (HAX SDK)",
        "模式:     文本终端 (TUI)",
    };
    hax_println("边框盒（hax_tui_box）：");
    hax_tui_box("系统信息", box_lines, 3);
    hax_println("");

    /* ── 进度条（in_place 依赖终端支持 \r，图形终端退化为逐行） ── */
    hax_println("进度条（hax_tui_progress）：");
    for (HI32 i = 0; i <= 100; i += 10)
        hax_tui_progress("渲染", i, 0, 100, 30, 1);
    hax_println("");
    hax_println("");

    /* ── 菜单 ── */
    HI32 sel = hax_tui_menu("服务管理", services, 3, NULL);
    if (sel < 0) {
        hax_println("已取消。");
        return 0;
    }
    hax_printf("你选择了：%s\n\n", services[sel]);

    /* ── 确认 ── */
    if (sel == 0) {
        int yes = hax_tui_confirm("确认启动服务？", 1);
        if (yes < 0) { hax_println("已取消。"); return 0; }
        hax_println(yes ? "好的，服务已启动。" : "好的，已跳过启动。");
    }

    /* ── 输入行 ── */
    char name[48] = "hive";
    HI32 n = hax_tui_input("输入进程名", name, name, (HI32)sizeof(name));
    if (n < 0) { hax_println("输入被中断。"); return 0; }
    hax_printf("进程名: %s\n\n", name);

    /* ── 复选 ── */
    int auto_start = hax_tui_checkbox("开机自动启动", 1, NULL);
    if (auto_start < 0) { hax_println("已取消。"); return 0; }
    hax_printf("自动启动: %s\n", auto_start ? "开" : "关");

    hax_tui_hline('=', 56);
    return 0;
}
