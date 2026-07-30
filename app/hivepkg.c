/* hivepkg —— HIVE 的 apt 风格软件包管理器。 */
#include <hax.h>
#include <hive_pkg.h>
#include "libc/unistd.h"
#include "libc/sys/stat.h"
#include "libc/socket.h"

HAX_APP("hivepkg", "HIVE 软件包管理器", HAX_KIND_TUI);

#define PKG_ROOT "/var/lib/hivepkg"
#define PKG_CACHE PKG_ROOT "/Packages"
#define PKG_STATUS PKG_ROOT "/status"
#define PKG_SOURCES "/etc/hivepkg/sources.list"
#define PKG_BIN "/bin"
#define PKG_BUFFER_CAP (2 * 1024 * 1024)
#define INDEX_CAP (64 * 1024)
#define STATUS_CAP (32 * 1024)

typedef enum { SOURCE_FILE, SOURCE_HTTP } source_kind_t;
typedef struct {
    source_kind_t kind;
    char root[192];
    char host[48];
    int port;
} pkg_source_t;

static unsigned char package_buf[PKG_BUFFER_CAP];
static char index_buf[INDEX_CAP];
static char status_buf[STATUS_CAP];
static char rewrite_buf[STATUS_CAP];
static char upgrade_buf[STATUS_CAP];

static void usage(void) {
    hax_println("hivepkg — HIVE 软件包管理器");
    hax_println("用法: run hivepkg <命令> [参数]");
    hax_println("  init                 初始化目录和默认源");
    hax_println("  source               显示当前软件源");
    hax_println("  source file <目录>   设置本地软件源");
    hax_println("  source http <IP> <端口> <路径>  设置 HTTP 源");
    hax_println("  update               更新软件包索引");
    hax_println("  list [--installed]    列出可用或已安装软件包");
    hax_println("  search <文字>         搜索软件包");
    hax_println("  show <包名>           显示软件包信息");
    hax_println("  install <包名>        安装或升级软件包");
    hax_println("  remove <包名>         删除软件包");
    hax_println("  upgrade [包名]        升级一个或全部软件包");
    hax_println("  run <包名> [参数...]  执行已安装软件包");
    hax_println("源格式: file|/packages");
    hax_println("        http|10.0.2.2|8080|/hive");
}

static int ensure_dirs(void) {
    (void)mkdir("/var", 0755);
    (void)mkdir("/var/lib", 0755);
    (void)mkdir(PKG_ROOT, 0755);
    (void)mkdir("/etc", 0755);
    (void)mkdir("/etc/hivepkg", 0755);
    (void)mkdir(PKG_BIN, 0755);
    return 0;
}

static int init_layout(void) {
    ensure_dirs();
    if (access(PKG_SOURCES, F_OK) != 0) {
        static const char defaults[] =
            "# HIVE package sources\n"
            "file|/packages\n";
        if (hax_write_file(PKG_SOURCES, defaults,
                           (long)strlen(defaults)) < 0) {
            hax_println("hivepkg: 无法写入 sources.list");
            return -1;
        }
    }
    if (access(PKG_STATUS, F_OK) != 0)
        (void)hax_write_file(PKG_STATUS, "", 0);
    hax_println("hivepkg: 初始化完成");
    return 0;
}

static int configure_source(int argc, char **argv) {
    ensure_dirs();
    if (argc == 2) {
        char config[2048];
        long n = hax_read_file(PKG_SOURCES, config, sizeof(config) - 1);
        if (n < 0) {
            hax_println("hivepkg: 尚未配置软件源");
            return -1;
        }
        config[n] = 0;
        hax_print(config);
        return 0;
    }
    char config[320];
    int n = -1;
    if (argc == 4 && !strcmp(argv[2], "file")) {
        n = snprintf(config, sizeof(config), "file|%s\n", argv[3]);
    } else if (argc == 6 && !strcmp(argv[2], "http")) {
        int port = atoi(argv[4]);
        if (port <= 0 || port >= 65536) {
            hax_println("hivepkg: 无效端口");
            return -1;
        }
        n = snprintf(config, sizeof(config), "http|%s|%d|%s\n",
                     argv[3], port, argv[5]);
    } else {
        hax_println("hivepkg: source 参数错误");
        return -1;
    }
    if (n <= 0 || (size_t)n >= sizeof(config) ||
        hax_write_file(PKG_SOURCES, config, n) != n) {
        hax_println("hivepkg: 无法保存软件源");
        return -1;
    }
    hax_println("hivepkg: 软件源已更新");
    return 0;
}

static char *next_line(char **cursor) {
    if (!cursor || !*cursor || !**cursor) return NULL;
    char *line = *cursor;
    char *p = line;
    while (*p && *p != '\n' && *p != '\r') p++;
    if (*p) {
        *p++ = 0;
        while (*p == '\n' || *p == '\r') p++;
    }
    *cursor = p;
    return line;
}

static int load_source(pkg_source_t *source) {
    char config[2048];
    long n = hax_read_file(PKG_SOURCES, config, sizeof(config) - 1);
    if (n < 0) {
        hax_println("hivepkg: 未找到软件源；先运行 init");
        return -1;
    }
    config[n] = 0;
    char *cursor = config;
    char *line;
    while ((line = next_line(&cursor)) != NULL) {
        if (!line[0] || line[0] == '#') continue;
        memset(source, 0, sizeof(*source));
        source->port = 80;
        if (!strncmp(line, "file|", 5)) {
            source->kind = SOURCE_FILE;
            strncpy(source->root, line + 5, sizeof(source->root) - 1);
            return source->root[0] ? 0 : -1;
        }
        if (!strncmp(line, "http|", 5)) {
            char *host = line + 5;
            char *port = strchr(host, '|');
            if (!port) continue;
            *port++ = 0;
            char *root = strchr(port, '|');
            if (!root) continue;
            *root++ = 0;
            source->kind = SOURCE_HTTP;
            strncpy(source->host, host, sizeof(source->host) - 1);
            strncpy(source->root, root, sizeof(source->root) - 1);
            source->port = atoi(port);
            if (source->host[0] && source->root[0] &&
                source->port > 0 && source->port < 65536)
                return 0;
        }
    }
    hax_println("hivepkg: sources.list 中没有有效软件源");
    return -1;
}

static int join_path(char *out, size_t cap, const char *root,
                     const char *relative) {
    size_t rn = strlen(root);
    int slash = rn > 0 && root[rn - 1] != '/' &&
                relative && relative[0] != '/';
    int n = snprintf(out, cap, "%s%s%s", root, slash ? "/" : "",
                     relative ? relative : "");
    return n > 0 && (size_t)n < cap ? 0 : -1;
}

static long fetch_file(const pkg_source_t *source, const char *relative,
                       unsigned char *out, long cap) {
    char path[320];
    if (join_path(path, sizeof(path), source->root, relative) < 0) return -1;
    return hax_read_file(path, out, cap);
}

static long fetch_http(const pkg_source_t *source, const char *relative,
                       unsigned char *out, long cap) {
    char path[320], request[512];
    if (join_path(path, sizeof(path), source->root, relative) < 0) return -1;
    uint32_t address = inet_addr(source->host);
    if (!address) {
        hax_println("hivepkg: HTTP 源当前需要 IPv4 地址，尚不支持 DNS");
        return -1;
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_in peer;
    memset(&peer, 0, sizeof(peer));
    peer.sin_family = AF_INET;
    peer.sin_port = htons((uint16_t)source->port);
    peer.sin_addr = address;
    if (connect(fd, &peer, sizeof(peer)) < 0) {
        close(fd);
        return -1;
    }
    int request_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n"
        "User-Agent: hivepkg/0.1\r\n\r\n", path, source->host);
    if (request_len <= 0) {
        close(fd);
        return -1;
    }
    int sent = 0;
    while (sent < request_len) {
        int n = send(fd, request + sent, (size_t)(request_len - sent), 0);
        if (n <= 0) {
            close(fd);
            return -1;
        }
        sent += n;
    }
    long total = 0;
    for (;;) {
        int got = recv(fd, out + total, (size_t)(cap - total));
        if (got <= 0) break;
        total += got;
        if (total == cap) {
            close(fd);
            return -1;
        }
    }
    close(fd);
    if (total < 12 || memcmp(out, "HTTP/1.", 7) ||
        !(out[9] == '2' && out[10] == '0' && out[11] == '0'))
        return -1;
    long body = -1;
    for (long i = 0; i + 3 < total; i++) {
        if (out[i] == '\r' && out[i+1] == '\n' &&
            out[i+2] == '\r' && out[i+3] == '\n') {
            body = i + 4;
            break;
        }
    }
    if (body < 0) return -1;
    long body_len = total - body;
    memmove(out, out + body, (size_t)body_len);
    return body_len;
}

static long fetch(const pkg_source_t *source, const char *relative,
                  unsigned char *out, long cap) {
    return source->kind == SOURCE_FILE ?
        fetch_file(source, relative, out, cap) :
        fetch_http(source, relative, out, cap);
}

static int validate_index(char *data) {
    char *cursor = data;
    char *line;
    int count = 0;
    hive_pkg_record_t record;
    while ((line = next_line(&cursor)) != NULL) {
        int parsed = hive_pkg_parse_line(line, &record);
        if (parsed < 0) return -1;
        if (parsed > 0) count++;
    }
    return count;
}

static int update_index(void) {
    ensure_dirs();
    pkg_source_t source;
    if (load_source(&source) < 0) return -1;
    long n = fetch(&source, "Packages", (unsigned char *)index_buf,
                   sizeof(index_buf) - 1);
    if (n < 0) {
        hax_println("hivepkg: 下载 Packages 失败");
        return -1;
    }
    index_buf[n] = 0;
    memcpy(package_buf, index_buf, (size_t)n + 1);
    int count = validate_index((char *)package_buf);
    if (count < 0) {
        hax_println("hivepkg: 软件源索引格式错误");
        return -1;
    }
    if (hax_write_file(PKG_CACHE, index_buf, n) != n) {
        hax_println("hivepkg: 无法保存索引");
        return -1;
    }
    hax_printf("已获取 %d 个软件包，%ld 字节。\n", count, n);
    return 0;
}

static long load_text(const char *path, char *buf, long cap) {
    long n = hax_read_file(path, buf, cap - 1);
    if (n < 0) return -1;
    buf[n] = 0;
    return n;
}

static int find_record_in(char *data, const char *name,
                          hive_pkg_record_t *out) {
    char *cursor = data;
    char *line;
    hive_pkg_record_t record;
    while ((line = next_line(&cursor)) != NULL) {
        if (hive_pkg_parse_line(line, &record) > 0 &&
            !strcmp(record.name, name)) {
            if (out) *out = record;
            return 1;
        }
    }
    return 0;
}

static int cached_record(const char *name, hive_pkg_record_t *out) {
    if (load_text(PKG_CACHE, index_buf, sizeof(index_buf)) < 0) {
        hax_println("hivepkg: 没有缓存索引；先运行 update");
        return -1;
    }
    return find_record_in(index_buf, name, out);
}

static int installed_record(const char *name, hive_pkg_record_t *out) {
    if (load_text(PKG_STATUS, status_buf, sizeof(status_buf)) < 0) return 0;
    return find_record_in(status_buf, name, out);
}

static int append_record(char *out, size_t cap, size_t *used,
                         const hive_pkg_record_t *r) {
    int n = snprintf(out + *used, cap - *used,
        "%s|%s|%s|%lu|%s|%s|%s|%s\n",
        r->name, r->version, r->arch, r->size, r->sha256,
        r->file, r->depends, r->description);
    if (n <= 0 || (size_t)n >= cap - *used) return -1;
    *used += (size_t)n;
    return 0;
}

static int rewrite_status(const char *drop,
                          const hive_pkg_record_t *append) {
    long n = load_text(PKG_STATUS, status_buf, sizeof(status_buf));
    if (n < 0) status_buf[0] = 0;
    size_t used = 0;
    char *cursor = status_buf;
    char *line;
    hive_pkg_record_t record;
    while ((line = next_line(&cursor)) != NULL) {
        if (hive_pkg_parse_line(line, &record) <= 0) continue;
        if (drop && !strcmp(record.name, drop)) continue;
        if (append_record(rewrite_buf, sizeof(rewrite_buf), &used, &record) < 0)
            return -1;
    }
    if (append && append_record(rewrite_buf, sizeof(rewrite_buf),
                                &used, append) < 0)
        return -1;
    return hax_write_file(PKG_STATUS, rewrite_buf, (long)used) == (long)used ?
        0 : -1;
}

static int install_package(const char *name, int depth) {
    if (depth > 8 || !hive_pkg_valid_name(name)) {
        hax_println("hivepkg: 无效包名或依赖循环");
        return -1;
    }
    hive_pkg_record_t package, installed;
    int found = cached_record(name, &package);
    if (found <= 0) {
        hax_printf("hivepkg: 找不到软件包 %s\n", name);
        return -1;
    }
    if (strcmp(package.arch, "x86_64") && strcmp(package.arch, "all")) {
        hax_printf("hivepkg: 不支持的软件包架构 %s\n", package.arch);
        return -1;
    }
    if (installed_record(name, &installed) > 0 &&
        hive_pkg_version_compare(installed.version, package.version) >= 0) {
        hax_printf("%s 已是最新版本 %s。\n", name, installed.version);
        return 0;
    }

    char dependencies[HIVE_PKG_DEPENDS_MAX];
    strncpy(dependencies, package.depends, sizeof(dependencies) - 1);
    dependencies[sizeof(dependencies) - 1] = 0;
    char *dep = dependencies;
    while (*dep) {
        while (*dep == ' ' || *dep == ',') dep++;
        if (!*dep) break;
        char *end = dep;
        while (*end && *end != ',' && *end != ' ') end++;
        char saved = *end; *end = 0;
        if (install_package(dep, depth + 1) < 0) return -1;
        *end = saved; dep = end;
    }

    pkg_source_t source;
    if (load_source(&source) < 0) return -1;
    hax_printf("获取 %s %s...\n", package.name, package.version);
    long bytes = fetch(&source, package.file, package_buf, sizeof(package_buf));
    if (bytes < 0 || (unsigned long)bytes != package.size) {
        hax_println("hivepkg: 下载失败或文件大小不匹配");
        return -1;
    }
    char digest[65];
    hive_sha256_hex(package_buf, (size_t)bytes, digest);
    if (strcmp(digest, package.sha256)) {
        hax_println("hivepkg: SHA-256 校验失败，拒绝安装");
        return -1;
    }
    char target[96];
    snprintf(target, sizeof(target), PKG_BIN "/%s", package.name);
    if (hax_write_file(target, package_buf, bytes) != bytes) {
        hax_println("hivepkg: 无法写入 /bin");
        return -1;
    }
    if (rewrite_status(package.name, &package) < 0) {
        (void)unlink(target);
        hax_println("hivepkg: 无法更新安装数据库，已回滚");
        return -1;
    }
    hax_printf("已安装 %s %s。\n", package.name, package.version);
    return 0;
}

static int dependency_list_has(const char *dependencies, const char *name) {
    const char *p = dependencies;
    size_t name_len = strlen(name);
    while (p && *p) {
        while (*p == ' ' || *p == ',') p++;
        const char *end = p;
        while (*end && *end != ',' && *end != ' ') end++;
        if ((size_t)(end - p) == name_len && !strncmp(p, name, name_len))
            return 1;
        p = end;
    }
    return 0;
}

static int remove_package(const char *name) {
    hive_pkg_record_t installed;
    if (installed_record(name, &installed) <= 0) {
        hax_printf("%s 未安装。\n", name);
        return -1;
    }
    if (load_text(PKG_STATUS, status_buf, sizeof(status_buf)) >= 0) {
        char *cursor = status_buf, *line;
        hive_pkg_record_t other;
        while ((line = next_line(&cursor)) != NULL) {
            if (hive_pkg_parse_line(line, &other) > 0 &&
                strcmp(other.name, name) &&
                dependency_list_has(other.depends, name)) {
                hax_printf("hivepkg: %s 仍依赖 %s，拒绝删除\n",
                           other.name, name);
                return -1;
            }
        }
    }
    char target[96];
    snprintf(target, sizeof(target), PKG_BIN "/%s", name);
    if (unlink(target) < 0) {
        hax_printf("hivepkg: 无法删除 %s\n", target);
        return -1;
    }
    if (rewrite_status(name, NULL) < 0) {
        hax_println("hivepkg: 警告：安装数据库更新失败");
        return -1;
    }
    hax_printf("已删除 %s。\n", name);
    return 0;
}

static int list_records(int installed_only, const char *query) {
    char *buffer = installed_only ? status_buf : index_buf;
    size_t cap = installed_only ? sizeof(status_buf) : sizeof(index_buf);
    const char *path = installed_only ? PKG_STATUS : PKG_CACHE;
    if (load_text(path, buffer, (long)cap) < 0) {
        hax_printf("hivepkg: 无法读取 %s\n", path);
        return -1;
    }
    char *cursor = buffer, *line;
    hive_pkg_record_t record;
    int count = 0;
    while ((line = next_line(&cursor)) != NULL) {
        if (hive_pkg_parse_line(line, &record) <= 0) continue;
        if (query && !strstr(record.name, query) &&
            !strstr(record.description, query))
            continue;
        hax_printf("%-20s %-12s %s\n",
                   record.name, record.version, record.description);
        count++;
    }
    hax_printf("共 %d 个软件包。\n", count);
    return 0;
}

static int show_package(const char *name) {
    hive_pkg_record_t record;
    if (cached_record(name, &record) <= 0) {
        hax_printf("hivepkg: 找不到软件包 %s\n", name);
        return -1;
    }
    hax_printf("Package: %s\nVersion: %s\nArchitecture: %s\n",
               record.name, record.version, record.arch);
    hax_printf("Installed-Size: %lu\nSHA256: %s\nFilename: %s\n",
               record.size, record.sha256, record.file);
    hax_printf("Depends: %s\nDescription: %s\n",
               record.depends[0] ? record.depends : "(none)",
               record.description);
    return 0;
}

static int upgrade_packages(const char *only) {
    if (only) return install_package(only, 0);
    if (load_text(PKG_STATUS, status_buf, sizeof(status_buf)) < 0) {
        hax_println("hivepkg: 没有已安装的软件包");
        return -1;
    }
    strcpy(upgrade_buf, status_buf);
    char *cursor = upgrade_buf, *line;
    hive_pkg_record_t installed, available;
    int upgraded = 0;
    while ((line = next_line(&cursor)) != NULL) {
        if (hive_pkg_parse_line(line, &installed) <= 0) continue;
        if (cached_record(installed.name, &available) > 0 &&
            hive_pkg_version_compare(available.version,
                                     installed.version) > 0) {
            if (install_package(installed.name, 0) < 0) return -1;
            upgraded++;
        }
    }
    hax_printf("升级完成：%d 个软件包。\n", upgraded);
    return 0;
}

static int run_package(int argc, char **argv) {
    const char *name = argv[2];
    if (!hive_pkg_valid_name(name)) return -1;
    hive_pkg_record_t installed;
    if (installed_record(name, &installed) <= 0) {
        hax_printf("hivepkg: %s 未安装\n", name);
        return -1;
    }
    char path[96];
    snprintf(path, sizeof(path), PKG_BIN "/%s", name);
    char *child_argv[32];
    int child_argc = 0;
    child_argv[child_argc++] = (char *)name;
    for (int i = 3; i < argc && child_argc < 31; i++)
        child_argv[child_argc++] = argv[i];
    child_argv[child_argc] = NULL;
    execve(path, child_argv, NULL);
    hax_printf("hivepkg: 无法执行 %s\n", path);
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 0;
    }
    if (!strcmp(argv[1], "init")) return init_layout() < 0;
    if (!strcmp(argv[1], "source")) return configure_source(argc, argv) < 0;
    if (!strcmp(argv[1], "update")) return update_index() < 0;
    if (!strcmp(argv[1], "list"))
        return list_records(argc > 2 && !strcmp(argv[2], "--installed"),
                            NULL) < 0;
    if (!strcmp(argv[1], "search") && argc > 2)
        return list_records(0, argv[2]) < 0;
    if (!strcmp(argv[1], "show") && argc > 2)
        return show_package(argv[2]) < 0;
    if (!strcmp(argv[1], "install") && argc > 2)
        return install_package(argv[2], 0) < 0;
    if (!strcmp(argv[1], "remove") && argc > 2)
        return remove_package(argv[2]) < 0;
    if (!strcmp(argv[1], "upgrade"))
        return upgrade_packages(argc > 2 ? argv[2] : NULL) < 0;
    if (!strcmp(argv[1], "run") && argc > 2)
        return run_package(argc, argv) < 0;
    usage();
    return 1;
}
