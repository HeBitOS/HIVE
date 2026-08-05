# HIVE Chromium 后端

HIVE 保留轻量静态浏览后端，并以固定版本的 Chromium/Blink/V8 建立完整 Web
后端。上游的大型源码树不提交进 HIVE；`VERSION` 记录精确 Linux Stable
revision，`args.host.gn` 先验证引擎，`args.hbos.gn` 用于最终 HBOS 平台端口。

五个验收门如下：

- G0：Lite 后端能力诊断与离线回归；
- G1：动态 ELF、线程/futex、IPC、IPv6/X.509、窗口和音频平台基础；
- G2：Blink/V8 通过 HTML、CSS、ES modules、fetch 和 Vue 3 hydration；
- G3：Bilibili 首页完成 hydration、图片、导航与交互；
- G4：视频页完成 MSE、解码、音频输出和 720p 连续播放。

每一项必须留下自动验收或运行日志；仅下载源码或编译成功不能算网页兼容。
HBOS 集成树中的 `scripts/chromium_source.sh` 负责外置工作树同步和宿主生成。

构建完成后，`make modern-web-test HIVE_CONTENT_SHELL=/path/content_shell`
运行离线 Vue 门；`make bilibili-home-smoke` 和 `make bilibili-video-smoke`
通过 CDP 记录现网页面能力。后两者只是快速探针，不能代替 G3 的完整交互验收
或 G4 的 30 分钟音画同步验收。
