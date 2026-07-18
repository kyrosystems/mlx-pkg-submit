#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mlx_submit.h"

/* Declared in http.c */
extern int http_github_api(const char *method, const char *endpoint,
                           const char *token, const char *body,
                           char **out_buf, long *out_http_code);

/* Naive base64 encoder */
static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char *in, size_t inlen,
                          char *out, size_t outsz) {
    size_t i = 0, j = 0;
    unsigned char buf[3], tmp[4];
    while (inlen >= 3 && j + 5 < outsz) {
        buf[0]=in[i++]; buf[1]=in[i++]; buf[2]=in[i++]; inlen-=3;
        tmp[0]=(buf[0]>>2)&0x3F;
        tmp[1]=((buf[0]<<4)|(buf[1]>>4))&0x3F;
        tmp[2]=((buf[1]<<2)|(buf[2]>>6))&0x3F;
        tmp[3]=buf[2]&0x3F;
        for(int k=0;k<4&&j+1<outsz;k++) out[j++]=b64chars[tmp[k]];
    }
    if (inlen > 0 && j + 5 < outsz) {
        buf[0]=in[i]; buf[1]=(inlen>1)?in[i+1]:0; buf[2]=0;
        tmp[0]=(buf[0]>>2)&0x3F;
        tmp[1]=((buf[0]<<4)|(buf[1]>>4))&0x3F;
        tmp[2]=((buf[1]<<2))&0x3F;
        out[j++]=b64chars[tmp[0]];
        out[j++]=b64chars[tmp[1]];
        out[j++]=(inlen>1)?b64chars[tmp[2]]:'=';
        out[j++]='=';
    }
    out[j]='\0';
}

/* Extract first string value for a JSON key */
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

/* Lowercase a string in-place */
static void str_tolower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* Try to get the blob SHA of a file that may already exist on a branch.
   Returns 0 and fills blob_sha[41] if found, -1 if not found. */
static int github_get_file_sha(const char *repo, const char *token,
                               const char *file_path, const char *branch,
                               char *blob_sha) {
    char endpoint[1024];
    snprintf(endpoint, sizeof(endpoint),
             "/repos/%s/contents/%s?ref=%s", repo, file_path, branch);

    char *resp = NULL;
    long code  = 0;
    if (http_github_api("GET", endpoint, token, NULL, &resp, &code) != 0
        || code != 200) {
        free(resp);
        return -1; /* file doesn't exist yet — that's fine */
    }

    int rc = json_get_str(resp, "sha", blob_sha, 41);
    free(resp);
    return rc;
}

int github_submit(const struct manifest *m, const char *json_content,
                  const char *token, const char *repo) {
    char endpoint[512];
    char *resp     = NULL;
    long http_code = 0;
    char req_body[16384];
    char b64_content[16384];

    /* Lowercase name for branch / path to avoid case-sensitivity surprises */
    char name_lc[FIELD_SM];
    strncpy(name_lc, m->name, sizeof(name_lc) - 1);
    name_lc[sizeof(name_lc)-1] = '\0';
    str_tolower(name_lc);

    char cat_lc[FIELD_SM];
    strncpy(cat_lc, m->category, sizeof(cat_lc) - 1);
    cat_lc[sizeof(cat_lc)-1] = '\0';
    str_tolower(cat_lc);

    /* --- Step 1: Get main branch SHA --- */
    snprintf(endpoint, sizeof(endpoint), "/repos/%s/git/ref/heads/main", repo);
    if (http_github_api("GET", endpoint, token, NULL, &resp, &http_code) != 0
        || http_code != 200) {
        fprintf(stderr, "[github] Failed to get main branch ref (HTTP %ld)\n", http_code);
        if (resp) { fprintf(stderr, "%s\n", resp); free(resp); }
        return -1;
    }
    
    char base_sha[64] = {0};
    if (json_get_str(resp, "sha", base_sha, sizeof(base_sha)) != 0) {
        const char *sha_p = strstr(resp, "\"sha\":");
        if (sha_p) {
            sha_p += 6;
            while (*sha_p == ' ' || *sha_p == '"') sha_p++;
            size_t k = 0;
            while (sha_p[k] && sha_p[k] != '"' && sha_p[k] != ',' && sha_p[k] != '}' && k < 63) { 
                base_sha[k] = sha_p[k]; 
                k++; 
            }
            base_sha[k] = '\0';
        }
    }
    free(resp); resp = NULL;
    
    if (!base_sha[0] || strlen(base_sha) < 40) { 
        fprintf(stderr, "[github] Could not parse main SHA\n"); 
        return -1; 
    }
    printf("[github] main SHA: %.12s...\n", base_sha);

    /* --- Step 2: Create branch pkg/<name_lc>-<version> --- */
    char branch[128];
    snprintf(branch, sizeof(branch), "pkg/%s-%s", name_lc, m->version);

    for (int i = 0; branch[i]; i++) {
        if (branch[i] == ' ' || branch[i] == '\\' || branch[i] == '"') {
            branch[i] = '-'; 
        }
    }

    snprintf(req_body, sizeof(req_body),
        "{\"ref\":\"refs/heads/%s\",\"sha\":\"%s\"}", branch, base_sha);
        
    snprintf(endpoint, sizeof(endpoint), "/repos/%s/git/refs", repo);
    if (http_github_api("POST", endpoint, token, req_body, &resp, &http_code) != 0) {
        fprintf(stderr, "[github] Network error creating branch\n");
        free(resp); return -1;
    }
    if (http_code == 422) {
        /* Branch already exists — not a fatal error, reuse it */
        printf("[github] Branch already exists, reusing: %s\n", branch);
    } else if (http_code != 201) {
        fprintf(stderr, "[github] Failed to create branch (HTTP %ld)\n%s\n",
                http_code, resp ? resp : "");
        free(resp); return -1;
    } else {
        printf("[github] Branch created: %s\n", branch);
    }
    free(resp); resp = NULL;

    /* --- Step 3: Build file path and base64 encode content --- */
    base64_encode((const unsigned char *)json_content, strlen(json_content),
                  b64_content, sizeof(b64_content));

    char file_path[512];
    snprintf(file_path, sizeof(file_path),
             "manifests/%s/%s/%s.json", cat_lc, name_lc, m->version);

    char commit_msg[256];
    snprintf(commit_msg, sizeof(commit_msg),
             "feat: add %s %s", name_lc, m->version);

    /* --- Step 3a: Check if file already exists on the branch (need its blob SHA for update) --- */
    char existing_blob_sha[41] = {0};
    int file_exists = (github_get_file_sha(repo, token, file_path, branch,
                                           existing_blob_sha) == 0
                       && existing_blob_sha[0]);

    if (file_exists) {
        printf("[github] File already exists on branch, will update it (blob SHA: %.8s...)\n",
               existing_blob_sha);
        /* Include sha field for update */
        snprintf(req_body, sizeof(req_body),
            "{\"message\":\"%s\","
            "\"content\":\"%s\","
            "\"sha\":\"%s\","
            "\"branch\":\"%s\"}",
            commit_msg, b64_content, existing_blob_sha, branch);
    } else {
        /* New file — no sha field */
        snprintf(req_body, sizeof(req_body),
            "{\"message\":\"%s\","
            "\"content\":\"%s\","
            "\"branch\":\"%s\"}",
            commit_msg, b64_content, branch);
    }

    snprintf(endpoint, sizeof(endpoint),
             "/repos/%s/contents/%s", repo, file_path);

    if (http_github_api("PUT", endpoint, token, req_body, &resp, &http_code) != 0
        || (http_code != 201 && http_code != 200)) {
        fprintf(stderr, "[github] Failed to create/update file (HTTP %ld)\n%s\n",
                http_code, resp ? resp : "");
        free(resp); return -1;
    }
    free(resp); resp = NULL;
    printf("[github] Manifest committed: %s\n", file_path);

    /* --- Step 4: Open Pull Request (or report if already exists) --- */
    char pr_title[256], pr_body_escaped[2048];
    snprintf(pr_title, sizeof(pr_title), "[pkg] Add %s %s", name_lc, m->version);

    /* Build PR body with \n escaped for JSON */
    snprintf(pr_body_escaped, sizeof(pr_body_escaped),
        "## New Package: %s %s\\n\\n"
        "- **Category:** %s\\n"
        "- **License:** %s\\n"
        "- **Maintainer:** %s\\n"
        "- **SHA-256:** `%s`\\n"
        "- **Install type:** %s\\n"
        "- **Architecture:** %s\\n\\n"
        "Submitted via `mlx-pkg-submit`.",
        name_lc, m->version,
        cat_lc, m->license, m->maintainer,
        m->sha256, m->install_type, m->arch);

    snprintf(req_body, sizeof(req_body),
        "{\"title\":\"%s\","
        "\"body\":\"%s\","
        "\"head\":\"%s\","
        "\"base\":\"main\"}",
        pr_title, pr_body_escaped, branch);

    snprintf(endpoint, sizeof(endpoint), "/repos/%s/pulls", repo);
    if (http_github_api("POST", endpoint, token, req_body, &resp, &http_code) != 0) {
        fprintf(stderr, "[github] Network error creating PR\n");
        free(resp); return -1;
    }

    if (http_code == 201) {
        char pr_url[512] = {0};
        json_get_str(resp, "html_url", pr_url, sizeof(pr_url));
        printf("[github] PR opened: %s\n", pr_url);
    } else if (http_code == 422) {
        /* PR already exists for this branch */
        printf("[github] PR already exists for branch '%s' — manifest was updated in existing PR.\n",
               branch);
    } else {
        fprintf(stderr, "[github] Failed to create PR (HTTP %ld)\n%s\n",
                http_code, resp ? resp : "");
        free(resp); return -1;
    }
    free(resp);
    return 0;
}
