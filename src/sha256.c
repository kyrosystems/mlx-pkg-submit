#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

#include "mlx_submit.h"

int sha256_file(const char *path, char out_hex[65]) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror(path); return -1; }

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        SHA256_Update(&ctx, buf, n);
    }
    fclose(fp);

    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &ctx);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(out_hex + i * 2, 3, "%02x", digest[i]);
    }
    out_hex[64] = '\0';
    return 0;
}
