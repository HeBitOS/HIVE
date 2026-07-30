#include <hive_pkg.h>
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void) {
    hive_pkg_record_t record;
    const char *line =
        "widgets|1.2.10|x86_64|51752|"
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855|"
        "pool/widgets.hax|hive-runtime|Widget showcase\n";
    CHECK(hive_pkg_parse_line(line, &record) == 1);
    CHECK(!strcmp(record.name, "widgets"));
    CHECK(!strcmp(record.version, "1.2.10"));
    CHECK(record.size == 51752);
    CHECK(!strcmp(record.depends, "hive-runtime"));
    CHECK(hive_pkg_parse_line("# comment", &record) == 0);
    CHECK(hive_pkg_parse_line("broken|record", &record) == -1);
    CHECK(hive_pkg_valid_name("hive-tools+dev"));
    CHECK(!hive_pkg_valid_name("../escape"));
    CHECK(hive_pkg_version_compare("1.10.0", "1.9.9") > 0);
    CHECK(hive_pkg_version_compare("2.0", "2.0") == 0);
    CHECK(hive_pkg_version_compare("1.0", "1.0.1") < 0);

    char digest[65];
    hive_sha256_hex("", 0, digest);
    CHECK(!strcmp(digest,
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855"));
    hive_sha256_hex("abc", 3, digest);
    CHECK(!strcmp(digest,
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad"));
    puts("HIVE package format tests passed");
    return 0;
}
