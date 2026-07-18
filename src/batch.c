#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mlx_submit.h"

/*
 * Read entire file into a heap-allocated buffer.
 * Caller must free() the returned pointer.
 * Returns NULL on error.
 */
static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror(path); return NULL; }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);

    if (sz <= 0 || sz > (1 << 20)) { /* max 1 MiB per manifest */
        fprintf(stderr, "[batch] File too large or empty: %s\n", path);
        fclose(fp);
        return NULL;
    }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return NULL; }

    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        fprintf(stderr, "[batch] Read error: %s\n", path);
        free(buf); fclose(fp);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(fp);
    return buf;
}

/*
 * Naive JSON string extractor — looks for "key": "value" and copies value.
 * Returns 0 on success, -1 if key not found or not a string.
 */
static int json_get_str(const char *json, const char *key,
                        char *out, size_t outsz) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *p = strstr(json, needle);
    if (!p) return -1;
    p += strlen(needle);
    while (*p == ' ') p++;
    if (*p != '"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < outsz) out[i++] = *p++;
    out[i] = '\0';
    return 0;
}

/*
 * Load a pre-rendered manifest JSON file into a struct manifest.
 * Only fills the fields needed by github_submit (name, version, category,
 * license, maintainer, sha256, install_type, arch); the full JSON blob is
 * passed separately as the file content itself.
 */
static int load_manifest_from_json(const char *json, struct manifest *m) {
    memset(m, 0, sizeof(*m));
    m->release = 1;
    strncpy(m->arch, "x86_64", sizeof(m->arch) - 1);
    strncpy(m->install_type, "rpm", sizeof(m->install_type) - 1);

    if (json_get_str(json, "name",         m->name,         sizeof(m->name))         != 0) return -1;
    if (json_get_str(json, "version",      m->version,      sizeof(m->version))      != 0) return -1;
    if (json_get_str(json, "category",     m->category,     sizeof(m->category))     != 0) return -1;
    /* optional but nice to have */
    json_get_str(json, "license",      m->license,      sizeof(m->license));
    json_get_str(json, "maintainer",   m->maintainer,   sizeof(m->maintainer));
    json_get_str(json, "sha256",       m->sha256,       sizeof(m->sha256));
    json_get_str(json, "install_type", m->install_type, sizeof(m->install_type));
    json_get_str(json, "architecture", m->arch,         sizeof(m->arch));

    return 0;
}

/*
 * Submit multiple pre-rendered manifest JSON files in sequence.
 * Prints a summary at the end.
 * Returns number of failures (0 = all OK).
 */
int batch_submit(const char **files, int count,
                 const char *token, const char *repo) {
    int ok = 0, fail = 0;

    for (int i = 0; i < count; i++) {
        const char *path = files[i];
        printf("\n[batch] (%d/%d) Processing: %s\n", i + 1, count, path);

        char *json = read_file(path);
        if (!json) {
            fprintf(stderr, "[batch] Skipping %s (read error)\n", path);
            fail++;
            continue;
        }

        struct manifest m;
        if (load_manifest_from_json(json, &m) != 0) {
            fprintf(stderr, "[batch] Skipping %s (JSON parse error — missing name/version/category)\n",
                    path);
            free(json);
            fail++;
            continue;
        }

        printf("[batch] Submitting: %s %s (%s)\n",
               m.name, m.version, m.category);

        int rc = github_submit(&m, json, token, repo);
        if (rc == 0) {
            printf("[batch] OK: %s %s\n", m.name, m.version);
            ok++;
        } else {
            fprintf(stderr, "[batch] FAIL: %s %s (code %d)\n",
                    m.name, m.version, rc);
            fail++;
        }

        free(json);
    }

    printf("\n[batch] Done. %d submitted OK, %d failed.\n", ok, fail);
    return fail;
}
