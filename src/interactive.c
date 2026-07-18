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

    char line[dstsz > 2048 ? 2048 : dstsz];
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

int interactive_fill(struct manifest *m, char *local_file, size_t lf_sz) {
    printf("\n╔══════════════════════════════════════╗\n");
    printf(  "║   mlx-pkg-submit — interactive mode  ║\n");
    printf(  "╚══════════════════════════════════════╝\n\n");
    printf("Leave blank to keep the default value shown in [brackets].\n\n");

    prompt("Package name",        m->name,        m->name,        sizeof(m->name));
    prompt("Version",             m->version,     m->version,     sizeof(m->version));

    char rel_str[8];
    snprintf(rel_str, sizeof(rel_str), "%d", m->release ? m->release : 1);
    char rel_in[8] = {0};
    prompt("Release number",      rel_str,        rel_in,         sizeof(rel_in));
    if (rel_in[0]) m->release = atoi(rel_in);

    printf("  Categories: system | internet | desktop | devel | media | games | utils\n");
    prompt("Category",            m->category,    m->category,    sizeof(m->category));
    prompt("Short summary",       m->summary,     m->summary,     sizeof(m->summary));
    prompt("Description (opt)",   m->description, m->description, sizeof(m->description));
    prompt("SPDX License",        m->license,     m->license,     sizeof(m->license));
    prompt("Download URL",        m->url,         m->url,         sizeof(m->url));

    printf("  Provide local file path to hash (faster), or leave empty to download:\n");
    prompt("Local file path (opt)", "",            local_file,     lf_sz);

    printf("  Install types: rpm | tarball | appimage | flatpak\n");
    prompt("Install type",        m->install_type, m->install_type, sizeof(m->install_type));
    prompt("Architecture",        m->arch[0] ? m->arch : "x86_64",
                                  m->arch,        sizeof(m->arch));
    prompt("Dependencies (comma-separated, opt)",
                                  m->deps_raw,    m->deps_raw,    sizeof(m->deps_raw));
    prompt("Maintainer email",    m->maintainer,  m->maintainer,  sizeof(m->maintainer));
    prompt("Homepage URL (opt)",  m->homepage,    m->homepage,    sizeof(m->homepage));
    prompt("Source URL (opt)",    m->source_url,  m->source_url,  sizeof(m->source_url));

    printf("\n[*] Fields collected.\n");
    return 0;
}
