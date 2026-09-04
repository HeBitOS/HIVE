# HIVE 浏览器与 Bilibili 兼容升级方案

> 审计日期：2026-08-04  
> 目标：在保留轻量、快速静态阅读能力的同时，最终运行不经修改的现代
> HTML/CSS/JavaScript/Vue 页面，并以 Bilibili 首页和视频页作为高阶验收样本。

## 结论

当前 HIVE 内置浏览器不是完整 Web 引擎。它已经能通过 HTTP/HTTPS 获取页面，
跟随重定向，提取标题和 Open Graph 数据，显示文本、链接、表格及少量图片，
并解析有限 CSS；但它没有 DOM、JavaScript 引擎、标准盒模型、Web API、媒体
管线或浏览器安全模型。因此当前版本只能标记为“轻量静态浏览”，不能标记为
“完整支持 HTML/CSS/JS/Vue”。

Vue 不是需要单独识别的一种文件格式。Vue 生产包能否运行，取决于浏览器是否
正确实现 ECMAScript、DOM、事件、CSSOM、Fetch、History、URL、定时器、Promise、
模块加载以及页面布局。只加入一个 JavaScript 解释器并不能实现 Vue 兼容。

推荐采用双后端：继续保留现有轻量后端处理文档、错误页和低内存场景；完整网页
交给可替换的标准引擎后端。完整后端优先复用 HPT/Linux ABI 路线所需的线程、
内存、动态链接、IPC、字体和图形能力，再移植 Chromium content shell/Blink/V8。
这样与“原版 KDE/Qt 最终可运行”的目标共用基础设施，不在内核里重复实现一套
不可维护的 Web 平台。

## 当前代码能力审计

| 能力 | 当前实现 | 结论 |
|---|---|---|
| 网络 | DNS、HTTP、HTTPS、重定向、后台抓取 | 基础可用；仍需 HTTP/2、压缩、缓存、Cookie、CORS/CSP 和完整证书策略 |
| HTML | 流式扫描标签，生成文本块；标题、段落、列表、链接、表格、OG 元数据 | 不是 WHATWG HTML parser，没有 DOM 和错误恢复模型 |
| CSS | 最多 96 条简单规则；颜色、粗体、斜体、下划线、简化字号/背景/对齐/边框/间距 | 没有级联、继承、盒模型、Flex、Grid、定位、变量、媒体查询和动画 |
| 图片 | 每页登记 8 个槽，最多抓取 2 张，PNG/JPEG/BMP 软件解码 | 适合静态摘要，不满足响应式图片和现代网页资源模型 |
| JavaScript | `<script>` 整段跳过，`javascript:` 链接拒绝 | 不支持 |
| DOM/Web API | 无 | 不支持 Vue、React 等运行时 |
| 媒体 | `<video>`、`<audio>`、`<canvas>`、`<iframe>`、`<svg>` 整段跳过 | 不支持在线视频播放 |

现有实现的优点是依赖少、内存上限清楚、启动快。它应长期作为 `lite` 后端存在，
而不是在同一个流式文本渲染器中不断堆叠标准特例。

## Bilibili 现网抽样

本次使用桌面 Chrome User-Agent 请求 [Bilibili 首页](https://www.bilibili.com/)
以及从首页实时推荐中抽取的一个视频页。Bilibili 会进行 A/B 发布，资源哈希和
局部框架可能随时变化，以下是该日期的可复现实测，不把单次样本外推成整个站点
永远不变的架构。

### 首页

- HTTP/2 返回服务端渲染 HTML，响应包含 `server-timing: render`。
- 页面注入 `window.__pinia` 状态，主入口使用 `type="module"`，并同时提供
  Vite legacy polyfill/entry；这表明当前样本采用 Vue + Pinia + Vite，且有
  SSR/客户端 hydration 路径。
- 主 JavaScript 约 1.13 MB，样式约 363 KB。代码使用 Fetch/XHR、
  `IntersectionObserver`、`ResizeObserver`、`MutationObserver`、WebAssembly
  和 WebGL 等能力。
- CSS 样本包含大量 CSS 自定义属性、媒体查询、Flex、Grid、变换、动画和
  `object-fit`。现有 HIVE CSS 映射无法还原其布局。

Vue 官方文档说明 SPA 会由 Vue 控制页面状态和导航，而 SSR 页面随后需要在客户
端 hydration 才能获得完整交互：[Vue 的使用方式](https://vuejs.org/guide/extras/ways-of-using-vue.html)。

### 视频页与播放器

- 视频页含 Vue SSR 标记、`window.__INITIAL_STATE__`，并按运行时状态插入独立
  webpack 页面包；播放器是另一份独立核心脚本，而不是普通 `<video src>` 即可
  替代的静态页面。
- 本次页面应用包约 3.58 MB，播放器核心约 2.02 MB。静态扫描可见 MSE
  `MediaSource`/`SourceBuffer`、Worker、Fetch/XHR、WebAudio、WebGL、观察器和
  `requestAnimationFrame` 等调用。
- Bilibili 公开的 [flv.js](https://github.com/bilibili/flv.js) 也说明其网页播放
  路径需要 JavaScript 将 FLV 转封装为分段 MP4，再送入 Media Source Extensions。
  这意味着仅解析 HTML、下载 MP4 或加入 Vue 语法识别都不足以播放 Bilibili。

因此，Bilibili 完整兼容至少需要：标准 DOM/CSS/JS、ES module、SSR hydration、
Fetch/XHR、URL/History、Cookie/Storage、Observer API、Canvas/SVG、MSE、Worker、
音频输出、H.264/AAC 解码、Range/CORS，以及可靠的 TLS 证书链和 IPv4/IPv6 网络。

## 目标架构

```text
HIVE 浏览器外壳
├── 地址栏、历史、下载、权限与错误 UI
├── LiteBackend（现有静态阅读器）
└── WebBackend（独立用户态内容进程）
    ├── Blink + V8 + 软件光栅起步
    ├── HBOS/HPT POSIX-Linux 兼容层
    ├── 共享内存 ARGB surface + 脏矩形提交
    ├── 网络、证书、IPv6、Cookie/CORS/CSP
    └── 媒体进程：MSE + demux + H.264/AAC + AC97/后续音频后端
```

浏览器 UI、网页内容和媒体解码必须有独立资源生命周期。首版可采用受限单内容
进程和 CPU 软件光栅，但接口从一开始保留多进程与 GPU surface 的可能；页面
崩溃不能拖垮桌面。完整后端不可回退到明文 HTTP，证书或 TLS 失败必须显示错误。

## 分阶段升级与验收

### G0：修稳现有 LiteBackend

- 将 URL/网络、HTML、CSS、布局、图片和绘制从 `src/tools/gui.c` 拆出。
- 为本地 HTML、CSS、重定向、HTTPS 失败、链接、滚动和图片建立离线回归页。
- 保留明确能力标识：`JavaScript: unavailable`，不把静态 SSR 摘要称为完整页面。

验收：低内存环境下静态文档可稳定加载；关闭或加载失败后无悬挂任务和窗口。

### G1：补齐完整引擎所需的系统底座

- 动态 ELF/真实 musl、线程/TLS、futex、epoll、共享内存、文件映射、大地址空间、
  时钟/随机数、进程退出回收和资源限制。
- 字体发现、FreeType/Harfbuzz 路径、剪贴板、IME、窗口 surface、缩放和输入事件。
- 网络补齐 IPv6、非阻塞 socket、DNS A/AAAA、TLS 证书链、SNI/ALPN、HTTP/2、
  gzip/Brotli、代理和可靠取消。

验收：平台测试不能用空实现返回成功；长时间并发、映射、网络和窗口压力测试通过。

### G2：接入标准 WebBackend，验证 Vue

- 固定一个 Chromium 上游基线及补丁集，建立 Clang/GN/Ninja/sysroot 构建。
- 先运行 content shell：软件光栅、共享内存提交、键鼠/滚轮、窗口缩放。
- 离线运行 Vue 3 production bundle，覆盖响应式更新、组件、事件、表单、
  Vue Router、Pinia、ES module、Fetch 和 hydration。
- 逐步接入 Web Platform Tests；不以“首页出现文字”替代标准行为测试。

验收：未修改的 Vue 测试应用全部交互通过，刷新/前进/后退状态正确，脚本异常只
终止内容页。

### G3：Bilibili 首页

- 支持其模块/legacy 加载、SSR hydration、CSS Flex/Grid/变量/媒体查询、图片
  懒加载、Cookie、CORS/CSP 和观察器 API。
- 记录首次内容时间、滚动帧率、峰值内存、网络请求数和页面崩溃恢复。

验收：首页布局和滚动可用，导航/搜索/动态卡片可交互；无视频能力时播放器区域
显示明确的“媒体后端未启用”，其余页面不得卡死。

### G4：Bilibili 视频播放

- 接入 MSE/SourceBuffer、Worker、Range 请求、demux、H.264/AAC 软件解码与音频。
- 首先验证 720p、普通 AVC/AAC、非 DRM、单视频；再做清晰度切换、全屏、弹幕、
  字幕和硬件解码。

验收：连续播放 30 分钟音画同步，无持续增长的内存/句柄；暂停、拖动、切换窗口
和网络中断恢复可用。登录、付费/DRM、直播和 4K 是后续独立门槛。

## 轻量与性能约束

- LiteBackend 永远可单独构建；完整引擎不得链接进内核或 HIVE 桌面进程。
- HTML/CSS/JS 解析、图片和媒体解码在用户态内容进程执行，并设置内存/CPU 限额。
- 页面绘制使用共享内存 tile 和脏矩形，禁止每帧复制整张屏幕。
- 网络缓存、字体缓存和解码缓冲均使用有上限的 LRU；页面关闭后可观测地回收。
- 先以软件光栅保证正确性；性能数据证明瓶颈后再增加 GPU 合成或硬解，不让 GPU
  驱动成为 Vue/普通网页兼容的前置条件。
- CI 同时运行 LiteBackend 回归、Vue 离线套件和完整引擎能力检查，防止“为了
  Bilibili 写站点特判”。

## 当前状态声明

截至 2026-08-04 的可验证进展如下：

- 已修复窗口打开后桌面图标无法点击，以及必须点标题栏才能切换窗口的问题；
  内容区点击现在也会聚焦并置顶窗口。
- G0 已加入版本化后端契约、能力位图和 host 离线回归，Lite 后端会明确报告
  JavaScript/Vue/MSE 不可用；遗留解析/排版代码的物理拆分仍继续进行。
- G1 已加入 futex wait/wake、bitset、绝对超时、robust-list `OWNER_DIED`、
  64 任务/128 FD 上限及 GNU hash 动态符号查找。加固后的线程用例已在 ring3
  QEMU 中得到 `LINUX_THREAD: PASS`。IPv6 和 X.509 验证路径已有实现，但
  `PT_INTERP` 最小解释器交接以及真实 musl 1.2.5 动态 hello 已在 HBOS QEMU
  通过；普通 ring3 signal handler、`SA_RESTORER` 与 `rt_sigreturn` 也已由
  QEMU 用例验证；628 KiB 共享对象的 VFS 流式 `dlopen/dlsym/dlclose` 亦已
  通过，句柄与符号解析已按地址空间隔离。共享库 General Dynamic TLS
  已支持 `PT_TLS`、`DTPMOD64/DTPOFF64`、`__tls_get_addr` 和按线程惰性实例，
  真实 `__thread` 的 `.tdata/.tbss`、对齐与父子线程写隔离已由 QEMU 验证。
  GNU 符号版本也已支持 `VERSYM/VERDEF/VERNEED`、默认版本和 `dlvsym`，并由
  真实 version-script 共享库验证函数及 TLS 版本绑定。GNU IFUNC/IRELATIVE
  resolver 也已改为 ring3 延迟执行并通过 CPL 检查回归。loader 已加入按 TID
  递归事务锁、线程局部 `dlerror`、构造函数重入，以及四线程各 64 轮
  `dlopen/dlsym/dlclose` 压力回归；内核跨地址空间对象链表也已保护。原生
  `clone`/`clone3` 的 pthread 形态也已恢复子返回上下文、新栈、FS/TLS 与
  TID 生命周期；未修改 musl 1.2.5 的四线程 create/join、mutex、condvar、
  rwlock、once、realtime 超时、FPU 环境继承和静态 TLS 隔离已通过。
  复杂 musl/glibc 依赖、完整 TLS descriptor/destructor、线程取消
  时的锁恢复、`clone3` 的进程/pidfd/cgroup 形态、完整
  `siginfo/ucontext`、altstack 与嵌套信号仍是硬阻塞项。
- NX/EFER 位定义和 CPUID 门控已经修正，`mmap/mprotect` 会修改真实页表权限；
  主 ELF、解释器和共享对象在装载/重定位后按 `PT_LOAD` 应用 W^X。RW→RX
  JIT 形态用例已通过。部分 `munmap` VMA 拆分、匿名页/`brk` 回收、任务退出
  地址空间销毁与 Linux `sysinfo` 已完成；连续启动/退出 12 次内存无增长，
  `prlimit64/getrusage/prctl/tgkill/madvise/clock_nanosleep` 与 RTC+PIT 时钟
  基线也已加入；普通 VFS 文件的私有快照及只读共享 `mmap` 已通过 ring3
  QEMU；`dlopen` 已具备递归 `DT_NEEDED`、路径去重、强符号失败和引用级联
  卸载；`DT_INIT/FINI` 与数组由用户 libc 在 ring3 按依赖顺序执行；按线程
  General Dynamic TLS 也已纳入同一回归。未修改 glibc 2.43 的动态
  loader/libc、符号版本、malloc 和基础 pthread/静态 TLS 也已通过。完整十八项
  musl/glibc/Linux 门禁通过；同一 glibc 探针还覆盖轻量 procfs 的
  self/exe、status、maps、fd、meminfo 与 cpuinfo，并枚举 CPU/网络只读
  sysfs、读取实际 E1000 MAC 和链路状态；真实目录 fd 相对的
  `openat/newfstatat/readlinkat/symlinkat/renameat*`、`AT_EMPTY_PATH`/nofollow、
  ramfs/HBFS 最终及中间路径符号链接、普通文件三种 rename 入口、覆盖/
  NOREPLACE 语义、ramfs/HBFS 目录整树迁移、cwd/打开目录 fd/后代 watch 跟随，
  以及固定容量 inotify 的原生 syscall、同 cookie 移动事件、文件 watch 改名
  跟随与 poll/epoll 就绪也已通过。AF_UNIX 的原生 syscall 51/52 与 libc
  `getsockname/getpeername` 已覆盖 pathname/abstract 地址、短缓冲区完整长度、
  未连接错误和 accept 真实对端地址，为后续 D-Bus 服务发现补齐 socket ABI。
  用户 page fault 也已可进入 ring3 `SIGSEGV` handler；回归会 `mprotect` 修页、
  经 `rt_sigreturn` 恢复完整通用寄存器并重试故障写入。原生 `fork` 已从 syscall
  现场恢复子进程，私有 owned 页以引用计数实现 4 KiB COW；八轮 QEMU 压力覆盖
  写 fault、`mprotect`、`MADV_DONTNEED`、父子隔离与回收。可写共享文件页缓存、
  跨核 TLB shootdown、空页表即时裁剪、`SIGBUS` 与完整 siginfo/ucontext 仍是
  Chromium 前置缺口。
- Chromium Linux Stable 151.0.7922.71 已按精确 revision 外置同步；116 项
  hooks 全部通过，Headless Linux 参考配置已由 GN 成功生成。离线 Vue 3.5.40
  验收页已加入 `tests/browser/modern-web/`，等待 `content_shell` 构建完成后执行。
- G2 只有出现 `HIVE_MODERN_WEB: PASS` 才算通过；G4 目前仍未通过，不能
  对外声明“Bilibili 视频兼容”。

> 2026-08-25 进展：
> - **G2 已通过**：`tests/browser/modern-web/run_content_shell.sh` 输出
>   `HIVE_MODERN_WEB: PASS`（DOM/CSS Grid 与自定义属性/Vue3 hydration 响应式/
>   fetch/WebAssembly/MediaSource/Worker 七项全过），日志：
>   `tests/browser/modern-web/g2-modern-web.log`。
> - **G3 引擎级首页验收已通过（host 参考 content_shell）**：
>   `HIVE_BILIBILI_HOME_SMOKE: PASS`（真实 Bilibili 首页加载、SSR hydration
>   `__pinia` 就绪、66 图、84 处 Flex/Grid、3 个 ES module），以及新增的
>   `HIVE_BILIBILI_HOME_INTERACTION: PASS`（滚动 43fps、导航点击进直播页、
>   搜索框真实输入+Enter、动态卡片点击新标签打开视频页、全程无崩溃），日志：
>   `tests/browser/g3-home-interaction.log`。视频页播放器容器（bpx-player）
>   已在导航落点确认存在。
> - “媒体后端未启用”占位、完整交互与性能验收（FCP/峰值内存/滚动帧率门禁）
>   仍需在 HBOS 平台端口（args.hbos.gn）内落地后复测；G3/G4 的对外声明
>   以平台端口验收为准。
> - **#1 平台端口推进（args.hbos.gn）**：已按“hbos 视作桌面 Linux 等价”的
>   最小骨架在 pinned Chromium 上打通 `target_os="hbos"` 的 GN 生成
>   （`gn gen out/hbos` 完成为 31212 targets / 4833 files），ffmpeg
>   `os_config` 映射到 `linux`，V8 `V8_HAVE_TARGET_OS`/`V8_TARGET_OS_LINUX`
>   映射到 `linux`（否则 `std-object-sizes.h` 的 `#if V8_TARGET_OS_LINUX`
>   因空宏编译失败），并注册了占位 ozone 后端
>   `ui/ozone/platform/hbos`（委托 headless，先保证能链接运行）。
>   `out/hbos` 用树内 `third_party/ninja/ninja` 编译 `content_shell`。
>
> **⚠ 构建铁律（2026-08-25 OOM 事故）**：本机只有 15GiB 内存 + 16 核，
> ninja **默认并行 = 全核 × clang 大 TU(2–4GB) = 必爆内存**。
> 23:17 内核 OOM killer 杀掉了 msedge（pid 233436，还连带 dsh web 重启、
> ninja 在 [1928/23975] 被 SIGKILL、移动硬盘掉盘）。
> **此后 Chromium 构建必须 `ninja -j2 -C out/hbos <target>`**，
> 想提速需先关掉 QQ/Steam 等大户再考虑 -j4。
> 日志：`/tmp/ninjahbos9.log`。
