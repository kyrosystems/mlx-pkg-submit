#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "mlx_submit.h"

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} membuf_t;

static size_t write_to_file(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

static size_t write_to_mem(void *ptr, size_t size, size_t nmemb, void *userp) {
    membuf_t *b = (membuf_t *)userp;
    size_t add = size * nmemb;
    if (b->len + add + 1 > b->cap) {
        b->cap = (b->len + add + 1) * 2;
        b->data = realloc(b->data, b->cap);
        if (!b->data) return 0;
    }
    memcpy(b->data + b->len, ptr, add);
    b->len += add;
    b->data[b->len] = '\0';
    return add;
}

int http_download(const char *url, const char *dest_path) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) { curl_easy_cleanup(curl); return -1; }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "mlx-pkg-submit/1.0");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? 0 : -1;
}

/* Internal: perform a GitHub API request, return response body in out_buf */
int http_github_api(const char *method, const char *endpoint,
                    const char *token, const char *body,
                    char **out_buf, long *out_http_code) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    membuf_t resp = {0};
    resp.data = malloc(4096);
    resp.cap  = 4096;
    resp.len  = 0;

    char auth_hdr[320];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);

    char url[512];
    snprintf(url, sizeof(url), "https://api.github.com%s", endpoint);

    struct curl_slist *hdrs = NULL;
    hdrs = curl_slist_append(hdrs, auth_hdr);
    hdrs = curl_slist_append(hdrs, "Accept: application/vnd.github+json");
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    hdrs = curl_slist_append(hdrs, "X-GitHub-Api-Version: 2022-11-28");
    hdrs = curl_slist_append(hdrs, "User-Agent: mlx-pkg-submit/1.0");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_mem);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    if (strcmp(method, "GET") == 0) {
        /* default */
    } else if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
        }
    } else if (strcmp(method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
        }
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    CURLcode res = curl_easy_perform(curl);

    if (out_http_code) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_http_code);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) { free(resp.data); return -1; }

    if (out_buf) *out_buf = resp.data;
    else free(resp.data);
    return 0;
}
