#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>

#include "mlx_submit.h"

static void print_prompt(const char *s) {
    fputs(s, stdout);
    fflush(stdout);
}

static size_t readline(char *buf, size_t bufsz) {
    buf[0] = '\0';
    if (!fgets(buf, (int)bufsz, stdin)) return 0;

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
    } else if (len == bufsz - 1) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;
    }
    return len;
}

static void prompt(const char *label, const char *def, char *dst, size_t dstsz) {
    char buf[256];
    if (def && def[0])
        snprintf(buf, sizeof(buf), "  %s [%s]: ", label, def);
    else
        snprintf(buf, sizeof(buf), "  %s: ", label);

    print_prompt(buf);

    char line[FIELD_LG];
    size_t len = readline(line, sizeof(line));

    if (len == 0 && def && def[0]) {
        strncpy(dst, def, dstsz - 1);
    } else {
        strncpy(dst, line, dstsz - 1);
    }
    dst[dstsz - 1] = '\0';
}

static void prompt_required(const char *label, char *dst, size_t dstsz) {
    char buf[256];
    snprintf(buf, sizeof(buf), "  %s (required): ", label);

    while (1) {
        print_prompt(buf);
        char line[FIELD_LG];
        size_t len = readline(line, sizeof(line));
        if (len > 0) {
            strncpy(dst, line, dstsz - 1);
            dst[dstsz - 1] = '\0';
            return;
        }
        fputs("  [!] This field cannot be empty.\n", stdout);
        fflush(stdout);
    }
}

static void prompt_choice(const char *label, const char *def,
                          const char **choices,
                          char *dst, size_t dstsz) {
    char line[FIELD_SM];
    while (1) {
        char buf[256];
        if (def && def[0])
            snprintf(buf, sizeof(buf), "  %s [%s]: ", label, def);
        else
            snprintf(buf, sizeof(buf), "  %s: ", label);

        print_prompt(buf);
        size_t len = readline(line, sizeof(line));
        const char *val = (len == 0 && def && def[0]) ? def : line;

        int ok = 0;
        for (int i = 0; choices[i]; i++) {
            if (strcasecmp(val, choices[i]) == 0) {
                strncpy(dst, choices[i], dstsz - 1);
                dst[dstsz - 1] = '\0';
                ok = 1;
                break;
            }
        }
        if (ok) return;

        fputs("  [!] Invalid choice. Valid options: ", stdout);
        for (int i = 0; choices[i]; i++) {
            fputs(choices[i], stdout);
            if (choices[i + 1]) fputs(" | ", stdout);
        }
        fputc('\n', stdout);
        fflush(stdout);
    }
}

static void draw_header(void) {
    puts("");
    puts("+--------------------------------------+");
    puts("|  mlx-pkg-submit - interactive mode   |");
    puts("+--------------------------------------+");
    puts("");
    puts("Leave blank to keep the default shown in [brackets].");
    puts("Fields marked (required) must not be empty.");
    puts("");
    fflush(stdout);
}

int interactive_fill(struct manifest *m, char *local_file, size_t lf_sz) {
    draw_header();

    static const char *CATEGORIES[] = {
        "system", "internet", "desktop", "devel",
        "media", "games", "utils", NULL
    };
    static const char *INSTALL_TYPES[] = {
        "rpm", "tarball", "appimage", "flatpak", NULL
    };

    if (!m->name[0])
        prompt_required("Package name", m->name, sizeof(m->name));
    else
        prompt("Package name", m->name, m->name, sizeof(m->name));

    if (!m->version[0])
        prompt_required("Version", m->version, sizeof(m->version));
    else
        prompt("Version", m->version, m->version, sizeof(m->version));

    char rel_str[16];
    snprintf(rel_str, sizeof(rel_str), "%d", m->release ? m->release : 1);
    char rel_in[16] = {0};
    prompt("Release number", rel_str, rel_in, sizeof(rel_in));
    if (rel_in[0]) {
        int valid = 1;
        for (int i = 0; rel_in[i]; i++)
            if (!isdigit((unsigned char)rel_in[i])) { valid = 0; break; }
        m->release = valid ? atoi(rel_in) : 1;
    } else if (!m->release) {
        m->release = 1;
    }

    puts("  Categories: system | internet | desktop | devel | media | games | utils");
    fflush(stdout);
    prompt_choice("Category",
                  m->category[0] ? m->category : NULL,
                  CATEGORIES,
                  m->category, sizeof(m->category));

    if (!m->summary[0])
        prompt_required("Short summary", m->summary, sizeof(m->summary));
    else
        prompt("Short summary", m->summary, m->summary, sizeof(m->summary));

    prompt("Description (opt)", m->description, m->description,
           sizeof(m->description));

    if (!m->license[0])
        prompt_required("SPDX License (e.g. GPL-3.0, MIT, MPL-2.0)",
                        m->license, sizeof(m->license));
    else
        prompt("SPDX License", m->license, m->license, sizeof(m->license));

    if (!m->url[0])
        prompt_required("Download URL", m->url, sizeof(m->url));
    else
        prompt("Download URL", m->url, m->url, sizeof(m->url));

    puts("  Provide local file path to hash (faster), or leave empty to download:");
    fflush(stdout);
    prompt("Local file path (opt)", "", local_file, lf_sz);

    puts("  Install types: rpm | tarball | appimage | flatpak");
    fflush(stdout);
    prompt_choice("Install type",
                  m->install_type[0] ? m->install_type : "rpm",
                  INSTALL_TYPES,
                  m->install_type, sizeof(m->install_type));

    prompt("Architecture",
           m->arch[0] ? m->arch : "x86_64",
           m->arch, sizeof(m->arch));
    if (!m->arch[0]) strncpy(m->arch, "x86_64", sizeof(m->arch) - 1);

    prompt("Dependencies (comma-separated, opt)",
           m->deps_raw, m->deps_raw, sizeof(m->deps_raw));

    if (!m->maintainer[0])
        prompt_required("Maintainer email", m->maintainer,
                        sizeof(m->maintainer));
    else
        prompt("Maintainer email", m->maintainer, m->maintainer,
               sizeof(m->maintainer));

    prompt("Homepage URL (opt)", m->homepage, m->homepage,
           sizeof(m->homepage));

    prompt("Source URL (opt)", m->source_url, m->source_url,
           sizeof(m->source_url));

    puts("");
    puts("[*] Fields collected.");
    fflush(stdout);
    return 0;
}
