/**
 * @file hive_pkg.h
 * @brief HIVE package index parser, version comparison and SHA-256 helpers.
 *
 * Repository index records are UTF-8 text with eight pipe-separated fields:
 * name|version|arch|size|sha256|file|depends|description
 */
#ifndef HIVE_PKG_H
#define HIVE_PKG_H

#include <stdint.h>
#include <stddef.h>
#include "libc/string.h"
#include "libc/stdlib.h"

#define HIVE_PKG_NAME_MAX 32
#define HIVE_PKG_VERSION_MAX 24
#define HIVE_PKG_ARCH_MAX 16
#define HIVE_PKG_SHA256_HEX 64
#define HIVE_PKG_FILE_MAX 96
#define HIVE_PKG_DEPENDS_MAX 128
#define HIVE_PKG_DESC_MAX 128
#define HIVE_PKG_LINE_MAX 512

typedef struct {
    char name[HIVE_PKG_NAME_MAX];
    char version[HIVE_PKG_VERSION_MAX];
    char arch[HIVE_PKG_ARCH_MAX];
    unsigned long size;
    char sha256[HIVE_PKG_SHA256_HEX + 1];
    char file[HIVE_PKG_FILE_MAX];
    char depends[HIVE_PKG_DEPENDS_MAX];
    char description[HIVE_PKG_DESC_MAX];
} hive_pkg_record_t;

static inline int hive_pkg_valid_name(const char *name) {
    if (!name || !name[0]) return 0;
    size_t n = 0;
    for (; name[n]; n++) {
        char c = name[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '+' || c == '-' || c == '.' || c == '_'))
            return 0;
        if (n + 1 >= HIVE_PKG_NAME_MAX) return 0;
    }
    return n > 0;
}

static inline int hive_pkg_copy_field(char *dst, size_t cap,
                                      const char *begin, const char *end) {
    size_t n = (size_t)(end - begin);
    if (!dst || cap == 0 || n >= cap) return -1;
    memcpy(dst, begin, n);
    dst[n] = 0;
    return 0;
}

static inline int hive_pkg_parse_line(const char *line,
                                      hive_pkg_record_t *record) {
    if (!line || !record || line[0] == '#' || line[0] == '\n' ||
        line[0] == '\r' || line[0] == 0)
        return 0;
    const char *field[9];
    field[0] = line;
    int count = 1;
    for (const char *p = line; *p && *p != '\n' && *p != '\r'; p++) {
        if (*p == '|') {
            if (count >= 9) return -1;
            field[count++] = p + 1;
        }
    }
    if (count != 8) return -1;
    const char *end[8];
    for (int i = 0; i < 7; i++) end[i] = field[i + 1] - 1;
    end[7] = field[7];
    while (*end[7] && *end[7] != '\n' && *end[7] != '\r') end[7]++;

    memset(record, 0, sizeof(*record));
    if (hive_pkg_copy_field(record->name, sizeof(record->name),
                            field[0], end[0]) < 0 ||
        hive_pkg_copy_field(record->version, sizeof(record->version),
                            field[1], end[1]) < 0 ||
        hive_pkg_copy_field(record->arch, sizeof(record->arch),
                            field[2], end[2]) < 0 ||
        hive_pkg_copy_field(record->sha256, sizeof(record->sha256),
                            field[4], end[4]) < 0 ||
        hive_pkg_copy_field(record->file, sizeof(record->file),
                            field[5], end[5]) < 0 ||
        hive_pkg_copy_field(record->depends, sizeof(record->depends),
                            field[6], end[6]) < 0 ||
        hive_pkg_copy_field(record->description, sizeof(record->description),
                            field[7], end[7]) < 0)
        return -1;

    char number[24];
    if (hive_pkg_copy_field(number, sizeof(number), field[3], end[3]) < 0)
        return -1;
    char *number_end = NULL;
    record->size = strtoul(number, &number_end, 10);
    if (!number[0] || !number_end || *number_end ||
        !hive_pkg_valid_name(record->name) ||
        !record->version[0] || !record->arch[0] ||
        strlen(record->sha256) != HIVE_PKG_SHA256_HEX ||
        !record->file[0])
        return -1;
    for (int i = 0; i < HIVE_PKG_SHA256_HEX; i++) {
        char c = record->sha256[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return -1;
    }
    return 1;
}

static inline int hive_pkg_version_compare(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    while (*a || *b) {
        if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9') {
            while (*a == '0') a++;
            while (*b == '0') b++;
            const char *ae = a, *be = b;
            while (*ae >= '0' && *ae <= '9') ae++;
            while (*be >= '0' && *be <= '9') be++;
            size_t an = (size_t)(ae - a), bn = (size_t)(be - b);
            if (an != bn) return an > bn ? 1 : -1;
            int cmp = strncmp(a, b, an);
            if (cmp) return cmp > 0 ? 1 : -1;
            a = ae; b = be;
            continue;
        }
        char ac = *a, bc = *b;
        if (ac == bc) {
            if (ac) { a++; b++; }
            continue;
        }
        if (!ac) return -1;
        if (!bc) return 1;
        return (unsigned char)ac > (unsigned char)bc ? 1 : -1;
    }
    return 0;
}

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t block_len;
} hive_sha256_t;

static inline uint32_t hive_sha_rotr(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}

static inline void hive_sha256_transform(hive_sha256_t *ctx,
                                         const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = hive_sha_rotr(w[i-15],7) ^ hive_sha_rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = hive_sha_rotr(w[i-2],17) ^ hive_sha_rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=ctx->state[0], b=ctx->state[1], c=ctx->state[2], d=ctx->state[3];
    uint32_t e=ctx->state[4], f=ctx->state[5], g=ctx->state[6], h=ctx->state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = hive_sha_rotr(e,6) ^ hive_sha_rotr(e,11) ^ hive_sha_rotr(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = hive_sha_rotr(a,2) ^ hive_sha_rotr(a,13) ^ hive_sha_rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static inline void hive_sha256_init(hive_sha256_t *ctx) {
    static const uint32_t initial[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    memcpy(ctx->state, initial, sizeof(initial));
    ctx->bit_count = 0;
    ctx->block_len = 0;
}

static inline void hive_sha256_update(hive_sha256_t *ctx,
                                      const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    ctx->bit_count += (uint64_t)len * 8;
    while (len) {
        size_t take = 64 - ctx->block_len;
        if (take > len) take = len;
        memcpy(ctx->block + ctx->block_len, bytes, take);
        ctx->block_len += take; bytes += take; len -= take;
        if (ctx->block_len == 64) {
            hive_sha256_transform(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
}

static inline void hive_sha256_final(hive_sha256_t *ctx, uint8_t out[32]) {
    ctx->block[ctx->block_len++] = 0x80;
    if (ctx->block_len > 56) {
        while (ctx->block_len < 64) ctx->block[ctx->block_len++] = 0;
        hive_sha256_transform(ctx, ctx->block);
        ctx->block_len = 0;
    }
    while (ctx->block_len < 56) ctx->block[ctx->block_len++] = 0;
    for (int i = 7; i >= 0; i--)
        ctx->block[ctx->block_len++] = (uint8_t)(ctx->bit_count >> (i * 8));
    hive_sha256_transform(ctx, ctx->block);
    for (int i = 0; i < 8; i++) {
        out[i*4]=(uint8_t)(ctx->state[i]>>24);
        out[i*4+1]=(uint8_t)(ctx->state[i]>>16);
        out[i*4+2]=(uint8_t)(ctx->state[i]>>8);
        out[i*4+3]=(uint8_t)ctx->state[i];
    }
}

static inline void hive_sha256_hex(const void *data, size_t len, char out[65]) {
    static const char hex[] = "0123456789abcdef";
    uint8_t digest[32];
    hive_sha256_t ctx;
    hive_sha256_init(&ctx);
    hive_sha256_update(&ctx, data, len);
    hive_sha256_final(&ctx, digest);
    for (int i = 0; i < 32; i++) {
        out[i*2] = hex[digest[i] >> 4];
        out[i*2+1] = hex[digest[i] & 15];
    }
    out[64] = 0;
}

#endif
