#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mlx_submit.h"

/* Declared in http.c */
extern int http_github_api(const char *method, const char *endpoint,
                           const char *token, const char *body,
                           char **out_buf, long *out_http_code);

/* Naive base64 encoder for small payloads (GitHub API needs base64 content) */
static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char *in, size_t inlen,
                          char *out, size_t outsz) {
    size_t i = 0, j = 0;
    unsigned char buf[3];
    unsigned char tmp[4];

    while (inlen >= 3 && j + 5 < outsz) {
        buf[0] = in[i++]; buf[1] = in[i++]; buf[2] = in[i++];
        inlen -= 3;
        tmp[0] = (buf[0] >> 2) & 0x3F;
        tmp[1] = ((buf[0] << 4) | (buf[1] >> 4)) & 0x3F;
        tmp[2] = ((buf[1] << 2) | (buf[2] >> 6)) & 0x3F;
        tmp[3] = buf[2] & 0x3F;
        for (int k = 0; k < 4 && j + 1 < outsz; k++) out[j++] = b64chars[tmp[k]];
    }
    if (inlen > 0 && j + 5 < outsz) {
        buf[0] = in[i]; buf[1] = (inlen > 1) ? in[i+1] : 0; buf[2] = 0;
        tmp[0] = (buf[0] >> 2) & 0x3F;
        tmp[1] = ((buf[0] << 4) | (buf[1] >> 4)) & 0x3F;
        tmp[2] = ((buf[1] << 2)) & 0x3F;
        out[j++] = b64chars[tmp[0]];
        out[j++] = b64chars[tmp[1]];
        out[j++] = (inlen > 1) ? b64chars[tmp[2]] : '=';
        out[j++] = '=';
    }
    out[j] = '\0';
}

/* Naive JSON string extractor — finds first value of "key":"value" */
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

int github_submit(const struct manifest *m, const char *json_content,
                  const char *token, const char *repo) {
    char endpoint[512];
    char *resp = NULL;
    long http_code = 0;
    char req_body[16384];
    char b64_content[16384];

    /* --- Step 1: Get default branch SHA --- */
    snprintf(endpoint, sizeof(endpoint), "/repos/%s/git/ref/heads/main", repo);
    if (http_github_api("GET", endpoint, token, NULL, &resp, &http_code) != 0
        || http_code != 200) {
        fprintf(stderr, "[github] Failed to get main branch ref (HTTP %ld)\n", http_code);
        if (resp) { fprintf(stderr, "%s\n", resp); free(resp); }
        return -1;
    }
    char base_sha[64] = {0};
    /* Find sha in: {"object":{"sha":"..."}} */
    const char *sha_p = strstr(resp, "\"sha\":");
    if (sha_p) {
        sha_p += 7; /* skip "sha": " */
        if (*sha_p == '"') sha_p++;
        size_t k = 0;
        while (sha_p[k] && sha_p[k] != '"' && k < 63) { base_sha[k] = sha_p[k]; k++; }
        base_sha[k] = '\0';
    }
    free(resp); resp = NULL;
    if (!base_sha[0]) { fprintf(stderr, "[github] Could not parse main SHA\n"); return -1; }
    printf("[github] main SHA: %.12s...\n", base_sha);

    /* --- Step 2: Create branch pkg/<name>-<version> --- */
    char branch[128];
    snprintf(branch, sizeof(branch), "pkg/%s-%s", m->name, m->version);

    snprintf(req_body, sizeof(req_body),
        "{\"ref\":\"refs/heads/%s\",\"sha\":\"%s\"}",
        branch, base_sha);
    snprintf(endpoint, sizeof(endpoint), "/repos/%s/git/refs", repo);
    if (http_github_api("POST", endpoint, token, req_body, &resp, &http_code) != 0
        || (http_code != 201 && http_code != 422)) {
        fprintf(stderr, "[github] Failed to create branch (HTTP %ld)\n", http_code);
        if (resp) { fprintf(stderr, "%s\n", resp); free(resp); }
        return -1;
    }
    free(resp); resp = NULL;
    printf("[github] Branch created: %s\n", branch);

    /* --- Step 3: Create file via Contents API --- */
    base64_encode((const unsigned char *)json_content, strlen(json_content),
                  b64_content, sizeof(b64_content));

    char file_path[512];
    snprintf(file_path, sizeof(file_path),
             "manifests/%s/%s/%s.json",
             m->category, m->name, m->version);

    char commit_msg[256];
    snprintf(commit_msg, sizeof(commit_msg),
             "feat: add %s %s", m->name, m->version);

    snprintf(req_body, sizeof(req_body),
        "{\"message\":\"%s\","
        "\"content\":\"%s\","
        "\"branch\":\"%s\"}",
        commit_msg, b64_content, branch);

    snprintf(endpoint, sizeof(endpoint),
             "/repos/%s/contents/%s", repo, file_path);

    if (http_github_api("PUT", endpoint, token, req_body, &resp, &http_code) != 0
        || (http_code != 201 && http_code != 200)) {
        fprintf(stderr, "[github] Failed to create file (HTTP %ld)\n", http_code);
        if (resp) { fprintf(stderr, "%s\n", resp); free(resp); }
        return -1;
    }
    free(resp); resp = NULL;
    printf("[github] Manifest committed: %s\n", file_path);

    /* --- Step 4: Open Pull Request --- */
    char pr_title[256], pr_body[1024];
    snprintf(pr_title, sizeof(pr_title), "[pkg] Add %s %s", m->name, m->version);
    snprintf(pr_body, sizeof(pr_body),
        "## New Package: %s %s\\n\\n"
        "- **Category:** %s\\n"
        "- **License:** %s\\n"
        "- **Maintainer:** %s\\n"
        "- **SHA-256:** `%s`\\n\\n"
        "Submitted via `mlx-pkg-submit`.",
        m->name, m->version, m->category,
        m->license, m->maintainer, m->sha256);

    snprintf(req_body, sizeof(req_body),
        "{\"title\":\"%s\","
        "\"body\":\"%s\","
        "\"head\":\"%s\","
        "\"base\":\"main\"}",
        pr_title, pr_body, branch);

    snprintf(endpoint, sizeof(endpoint), "/repos/%s/pulls", repo);
    if (http_github_api("POST", endpoint, token, req_body, &resp, &http_code) != 0
        || http_code != 201) {
        fprintf(stderr, "[github] Failed to create PR (HTTP %ld)\n", http_code);
        if (resp) { fprintf(stderr, "%s\n", resp); free(resp); }
        return -1;
    }

    char pr_url[512] = {0};
    json_get_str(resp, "html_url", pr_url, sizeof(pr_url));
    free(resp);

    printf("[github] PR opened: %s\n", pr_url);
    return 0;
}
