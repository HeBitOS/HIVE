# hivepkg 软件包管理器

`hivepkg` 是 HIVE 的用户态软件包管理器，定位类似 Debian/Ubuntu 的
`apt`。首版管理单文件 HAX/ELF 软件包，提供索引缓存、搜索、依赖安装、
SHA-256 完整性检查、本地安装数据库、删除、升级和执行。

## 快速开始

在开发主机生成仓库并通过 HTTP 提供：

```sh
make repo
python3 -m http.server 8080 --directory build/repo
```

QEMU 用户网络中，主机地址通常为 `10.0.2.2`。在 HBOS Shell 中执行：

```text
run hivepkg init
run hivepkg source http 10.0.2.2 8080 /
run hivepkg update
run hivepkg search HIVE
run hivepkg install calc
run hivepkg list --installed
run hivepkg run calc
```

也可以使用系统内的本地目录仓库：

```text
run hivepkg source file /packages
run hivepkg update
```

## 命令

| 命令 | 作用 |
|---|---|
| `init` | 创建 `/var/lib/hivepkg`、`/etc/hivepkg` 和 `/bin` |
| `source` | 查看或设置一个本地/IPv4 HTTP 软件源 |
| `update` | 获取并验证 `Packages` 索引 |
| `list` | 列出缓存的软件包 |
| `list --installed` | 列出已安装软件包 |
| `search <文字>` | 搜索包名和描述 |
| `show <包名>` | 显示版本、架构、大小、依赖与校验值 |
| `install <包名>` | 递归安装依赖并写入 `/bin/<包名>` |
| `remove <包名>` | 删除载荷和安装数据库记录 |
| `upgrade [包名]` | 升级指定包或全部已安装包 |
| `run <包名> [参数]` | 通过 `execve` 执行磁盘中的软件包 |

## 仓库格式

仓库由 `Packages` 和 `pool/` 组成。索引每行八个 `|` 分隔字段：

```text
name|version|arch|size|sha256|file|depends|description
```

`packages/manifest.json` 是 HIVE 官方包元数据源。`make repo` 调用
`tools/hivepkg_repo.py`，从 `build/hbos/app/*.hax` 复制载荷、计算 SHA-256
并生成确定性排序的索引。

## 当前边界

- HTTP 源目前只接受 IPv4 地址，不做 DNS 和 HTTPS。
- SHA-256 能检测传输损坏，但索引尚未进行签名；不要添加不可信软件源。
- HBOS 的桌面 HAX manifest 仍在构建期生成，因此磁盘安装的软件不会自动出现
  在桌面启动器。当前通过 `run hivepkg run <包名>` 执行。后续需要增加
  “磁盘应用枚举 + 异步文件执行”运行时 ABI。
- 当前包格式是一包一个 HAX/ELF。多文件包、安装脚本、冲突关系和仓库签名属于
  后续格式版本。
- 依赖字段当前是逗号分隔的精确包名，不支持版本范围。删除操作会拒绝移除仍被
  其他已安装包依赖的软件包。
- 单包上限由文件系统决定：传统 HBFS 当前为 64 KiB，FAT32/ramfs 可以容纳
  更大的包；`hivepkg` 自身的下载缓冲上限为 2 MiB。
