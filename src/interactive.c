#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mlx_submit.h"

/* Prompt helper: shows prompt, reads line into dst (trims newline) */
static void prompt(const char *label, const char *def,
                   char *dst, size_t dstsz) {
    if (def && def[0])
        printf("  %s [%s]: ", label, def);
    else
        printf("  %s: ", label);
    fflush(stdout);

    char line[2048];
    if (!fgets(line, sizeof(line), stdin)) { dst[0] = '\0'; return; }

    /* Strip newline */
    size_t len = strlen(line);
    if (len > 0 && line[len-1] == '\n') line[--len] = '\0';

    if (len == 0 && def && def[0]) {
        strncpy(dst, def, dstsz - 1);
        dst[dstsz - 1] = '\0';
    } else {
        strncpy(dst, line, dstsz - 1);
        dst[dstsz - 1] = '\0';
    }
}

/* Same as prompt but does NOT fall back to default if empty — keeps asking */
static void prompt_required(const char *label, char *dst, size_t dstsz) {
    while (1) {
        printf("  %s (required): ", label);
        fflush(stdout);
        char line[2048];
        if (!fgets(line, sizeof(line), stdin)) continue;
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len > 0) {
            strncpy(dst, line, dstsz - 1);
            dst[dstsz - 1] = '\0';
            return;
        }
        printf("  [!] This field is required, please enter a value.\n");
    }
}

int interactive_fill(struct manifest *m, char *local_file, size_t lf_sz) {
    printf("\n\u2554");
    for (int i = 0; i < 38; i++) printf("\u2550");
    printf("\u2557\n");
    printf(  "\u2551   mlx-pkg-submit \u2014 interactive mode  \u2551\n");
    printf(  "\u255a");
    for (int i = 0; i < 38; i++) printf("\u2550");
    printf("\u255d\n\n");
    printf("Leave blank to keep the default value shown in [brackets].\n");
    printf("Fields marked (required) must not be empty.\n\n");

    /* --- Required fields --- */
    if (!m->name[0])
        prompt_required("Package name", m->name, sizeof(m->name));
    else
        prompt("Package name", m->name, m->name, sizeof(m->name));

    if (!m->version[0])
        prompt_required("Version", m->version, sizeof(m->version));
    else
        prompt("Version", m->version, m->version, sizeof(m->version));

    char rel_str[8];
    snprintf(rel_str, sizeof(rel_str), "%d", m->release ? m->release : 1);
    char rel_in[32] = {0};
    prompt("Release number", rel_str, rel_in, sizeof(rel_in));
    if (rel_in[0]) m->release = atoi(rel_in);
    else if (!m->release) m->release = 1;

    /* --- Category: SEPARATE prompt, never skipped --- */
    printf("  Categories: system | internet | desktop | devel | media | games | utils\n");
    if (!m->category[0]) {
        prompt_required("Category", m->category, sizeof(m->category));
    } else {
        /* Show current and allow change */
        char cat_in[FIELD_SM] = {0};
        printf("  Category [%s]: ", m->category);
        fflush(stdout);
        char line[256];
        if (fgets(line, sizeof(line), stdin)) {
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
            if (len > 0) strncpy(m->category, line, sizeof(m->category) - 1);
        }
    }

    if (!m->summary[0])
        prompt_required("Short summary", m->summary, sizeof(m->summary));
    else
        prompt("Short summary", m->summary, m->summary, sizeof(m->summary));

    prompt("Description (opt)",   m->description, m->description, sizeof(m->description));

    if (!m->license[0])
        prompt_required("SPDX License", m->license, sizeof(m->license));
    else
        prompt("SPDX License", m->license, m->license, sizeof(m->license));

    if (!m->url[0])
        prompt_required("Download URL", m->url, sizeof(m->url));
    else
        prompt("Download URL", m->url, m->url, sizeof(m->url));

    printf("  Provide local file path to hash (faster), or leave empty to download:\n");
    prompt("Local file path (opt)", "", local_file, lf_sz);

    printf("  Install types: rpm | tarball | appimage | flatpak\n");
    prompt("Install type",
           m->install_type[0] ? m->install_type : "rpm",
           m->install_type, sizeof(m->install_type));
    if (!m->install_type[0]) strncpy(m->install_type, "rpm", sizeof(m->install_type) - 1);

    prompt("Architecture",
           m->arch[0] ? m->arch : "x86_64",
           m->arch, sizeof(m->arch));
    if (!m->arch[0]) strncpy(m->arch, "x86_64", sizeof(m->arch) - 1);

    prompt("Dependencies (comma-separated, opt)",
           m->deps_raw, m->deps_raw, sizeof(m->deps_raw));

    if (!m->maintainer[0])
        prompt_required("Maintainer email", m->maintainer, sizeof(m->maintainer));
    else
        prompt("Maintainer email", m->maintainer, m->maintainer, sizeof(m->maintainer));

    prompt("Homepage URL (opt)",  m->homepage,   m->homepage,   sizeof(m->homepage));
    prompt("Source URL (opt)",    m->source_url, m->source_url, sizeof(m->source_url));

    printf("\n[*] Fields collected.\n");
    return 0;
}
