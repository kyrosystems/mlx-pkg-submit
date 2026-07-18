#pragma once

#include <stddef.h>

/* Max field sizes */
#define FIELD_SM   64
#define FIELD_MD   256
#define FIELD_LG   1024
#define FIELD_URL  2048

struct manifest {
    char name[FIELD_SM];
    char version[FIELD_SM];
    int  release;
    char category[FIELD_SM];
    char summary[FIELD_MD];
    char description[FIELD_LG];
    char license[FIELD_SM];
    char url[FIELD_URL];
    char sha256[65];          /* 64 hex + NUL */
    long long size_bytes;
    char arch[FIELD_SM];
    char deps_raw[FIELD_MD];  /* comma-separated, parsed to JSON array */
    char install_type[FIELD_SM];
    char maintainer[FIELD_MD];
    char homepage[FIELD_URL];
    char source_url[FIELD_URL];
    char tags[FIELD_MD];
};

/* sha256.c */
int sha256_file(const char *path, char out_hex[65]);

/* manifest.c */
int manifest_to_json(const struct manifest *m, char *buf, size_t bufsz);
int manifest_validate(const struct manifest *m);

/* http.c */
int http_download(const char *url, const char *dest_path);

/* github.c */
int github_submit(const struct manifest *m, const char *json,
                  const char *token, const char *repo);

/* interactive.c */
int interactive_fill(struct manifest *m, char *local_file, size_t lf_sz);

/* batch.c */
int batch_submit(const char **files, int count,
                 const char *token, const char *repo);

/* init.c */
int init_template(const char *output_path);
