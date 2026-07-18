#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mlx_submit.h"

/* Escape a string for JSON (handles quotes and backslashes) */
static void json_escape(const char *src, char *dst, size_t dstsz) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < dstsz; i++) {
        if (src[i] == '"' || src[i] == '\\') {
            if (j + 3 >= dstsz) break;
            dst[j++] = '\\';
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

/* Build JSON array string from comma-separated deps_raw */
static void deps_to_json_array(const char *deps_raw, char *out, size_t outsz) {
    if (!deps_raw || !deps_raw[0]) {
        snprintf(out, outsz, "[]");
        return;
    }

    char tmp[FIELD_MD];
    strncpy(tmp, deps_raw, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    out[0] = '[';
    size_t pos = 1;
    char *tok = strtok(tmp, ",");
    int first = 1;

    while (tok && pos + 64 < outsz) {
        /* trim leading spaces */
        while (*tok == ' ') tok++;
        if (!first) { out[pos++] = ','; out[pos++] = ' '; }
        pos += snprintf(out + pos, outsz - pos, "\"%s\"", tok);
        first = 0;
        tok = strtok(NULL, ",");
    }

    if (pos + 2 < outsz) { out[pos++] = ']'; out[pos] = '\0'; }
}

int manifest_to_json(const struct manifest *m, char *buf, size_t bufsz) {
    char esc_summary[512], esc_desc[2048];
    char deps_json[512];

    json_escape(m->summary,     esc_summary, sizeof(esc_summary));
    json_escape(m->description, esc_desc,    sizeof(esc_desc));
    deps_to_json_array(m->deps_raw, deps_json, sizeof(deps_json));

    int n = snprintf(buf, bufsz,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"name\": \"%s\",\n"
        "  \"version\": \"%s\",\n"
        "  \"release\": %d,\n"
        "  \"category\": \"%s\",\n"
        "  \"summary\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"license\": \"%s\",\n"
        "  \"url\": \"%s\",\n"
        "  \"sha256\": \"%s\",\n"
        "  \"size_bytes\": %lld,\n"
        "  \"architecture\": \"%s\",\n"
        "  \"dependencies\": %s,\n"
        "  \"install_type\": \"%s\",\n"
        "  \"maintainer\": \"%s\",\n"
        "  \"homepage\": \"%s\",\n"
        "  \"source_url\": \"%s\"\n"
        "}\n",
        m->name, m->version, m->release,
        m->category,
        esc_summary, esc_desc,
        m->license,
        m->url, m->sha256, m->size_bytes,
        m->arch, deps_json, m->install_type,
        m->maintainer, m->homepage, m->source_url
    );

    return (n > 0 && (size_t)n < bufsz) ? 0 : -1;
}

int manifest_validate(const struct manifest *m) {
    int ok = 1;
    if (!m->name[0])         { fprintf(stderr, "[validate] missing: name\n");        ok = 0; }
    if (!m->version[0])      { fprintf(stderr, "[validate] missing: version\n");     ok = 0; }
    if (!m->category[0])     { fprintf(stderr, "[validate] missing: category\n");    ok = 0; }
    if (!m->url[0])          { fprintf(stderr, "[validate] missing: url\n");         ok = 0; }
    if (!m->license[0])      { fprintf(stderr, "[validate] missing: license\n");     ok = 0; }
    if (!m->maintainer[0])   { fprintf(stderr, "[validate] missing: maintainer\n");  ok = 0; }
    if (strlen(m->sha256) != 64) {
        fprintf(stderr, "[validate] sha256 must be 64 hex chars\n"); ok = 0;
    }
    return ok ? 0 : -1;
}
