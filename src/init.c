#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

#include "mlx_submit.h"

/*
 * Recursively create directories (like mkdir -p).
 * Modifies path in-place temporarily.
 */
static int mkdirp(const char *path) {
    char tmp[4096];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                perror(tmp);
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        perror(tmp);
        return -1;
    }
    return 0;
}

static const char TEMPLATE[] =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"name\": \"\",\n"
    "  \"version\": \"\",\n"
    "  \"release\": 1,\n"
    "  \"category\": \"\",\n"
    "  \"summary\": \"\",\n"
    "  \"description\": \"\",\n"
    "  \"license\": \"\",\n"
    "  \"url\": \"\",\n"
    "  \"sha256\": \"\",\n"
    "  \"size_bytes\": 0,\n"
    "  \"architecture\": \"x86_64\",\n"
    "  \"dependencies\": [],\n"
    "  \"install_type\": \"rpm\",\n"
    "  \"maintainer\": \"\",\n"
    "  \"homepage\": \"\",\n"
    "  \"source_url\": \"\"\n"
    "}\n";

int init_template(const char *output_path) {
    /* Create parent directories if needed */
    char dir_buf[4096];
    strncpy(dir_buf, output_path, sizeof(dir_buf) - 1);
    dir_buf[sizeof(dir_buf) - 1] = '\0';

    char *dir = dirname(dir_buf);
    if (dir && dir[0] && strcmp(dir, ".") != 0) {
        if (mkdirp(dir) != 0) {
            fprintf(stderr, "[init] Failed to create directory: %s\n", dir);
            return -1;
        }
    }

    FILE *fp = fopen(output_path, "w");
    if (!fp) {
        perror(output_path);
        return -1;
    }

    fputs(TEMPLATE, fp);
    fclose(fp);

    printf("[init] Template written to: %s\n", output_path);
    return 0;
}
