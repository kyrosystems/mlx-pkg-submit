#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include "mlx_submit.h"

#define VERSION "1.1.0"
#define TARGET_REPO "kyrosystems/makulinux-packages"

static void print_usage(const char *prog) {
    fprintf(stderr,
        "mlx-pkg-submit v" VERSION " — MakuLinux package submission tool\n"
        "Usage: %s [OPTIONS]\n\n"
        "  -n, --name <name>         Package name\n"
        "  -v, --version <ver>       Package version\n"
        "  -r, --release <num>       Release number (default: 1)\n"
        "  -c, --category <cat>      Category (system/internet/desktop/devel/media/games/utils)\n"
        "  -u, --url <url>           Download URL\n"
        "  -f, --file <path>         Local file to hash (skips downloading URL for hash)\n"
        "  -s, --summary <text>      Short summary\n"
        "  -l, --license <spdx>      SPDX license (e.g. GPL-3.0)\n"
        "  -m, --maintainer <email>  Maintainer email\n"
        "  -H, --homepage <url>      Homepage URL\n"
        "  -d, --deps <d1,d2,...>    Comma-separated dependencies\n"
        "  -t, --type <type>         Install type: rpm|tarball|appimage|flatpak\n"
        "  -o, --output <file>       Write manifest to file (no GitHub)\n"
        "  -i, --interactive         Interactive mode\n"
        "  -T, --token <token>       GitHub token (or GITHUB_TOKEN env var)\n"
        "  -D, --dry-run             Print JSON, don't submit\n"
        "  -I, --init <path>         Create a blank template manifest at <path>\n"
        "                            Parent directories are created automatically.\n"
        "  -b, --batch <f1> [f2...]  Submit multiple pre-rendered JSON manifest\n"
        "                            files at once (must come after all other flags).\n"
        "  -h, --help                Show this help\n"
        "  -V, --version-info        Show version\n",
        prog);
}

int main(int argc, char *argv[]) {
    struct manifest m;
    memset(&m, 0, sizeof(m));
    m.release = 1;
    strncpy(m.install_type, "rpm", sizeof(m.install_type) - 1);
    strncpy(m.arch, "x86_64", sizeof(m.arch) - 1);

    char local_file[1024] = {0};
    char output_file[1024] = {0};
    char github_token[256] = {0};
    char init_path[4096] = {0};
    int interactive = 0;
    int dry_run = 0;
    int do_batch = 0;

    /* Check GITHUB_TOKEN env */
    const char *env_token = getenv("GITHUB_TOKEN");
    if (env_token) strncpy(github_token, env_token, sizeof(github_token) - 1);

    /* Check MLX_MAINTAINER env */
    const char *env_maint = getenv("MLX_MAINTAINER");
    if (env_maint) strncpy(m.maintainer, env_maint, sizeof(m.maintainer) - 1);

    static struct option long_opts[] = {
        {"name",         required_argument, 0, 'n'},
        {"version",      required_argument, 0, 'v'},
        {"release",      required_argument, 0, 'r'},
        {"category",     required_argument, 0, 'c'},
        {"url",          required_argument, 0, 'u'},
        {"file",         required_argument, 0, 'f'},
        {"summary",      required_argument, 0, 's'},
        {"license",      required_argument, 0, 'l'},
        {"maintainer",   required_argument, 0, 'm'},
        {"homepage",     required_argument, 0, 'H'},
        {"deps",         required_argument, 0, 'd'},
        {"type",         required_argument, 0, 't'},
        {"output",       required_argument, 0, 'o'},
        {"interactive",  no_argument,       0, 'i'},
        {"token",        required_argument, 0, 'T'},
        {"dry-run",      no_argument,       0, 'D'},
        {"init",         required_argument, 0, 'I'},
        {"batch",        no_argument,       0, 'b'},
        {"help",         no_argument,       0, 'h'},
        {"version-info", no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };

    int opt, optidx = 0;
    while ((opt = getopt_long(argc, argv, "n:v:r:c:u:f:s:l:m:H:d:t:o:T:I:iDbDhV",
                              long_opts, &optidx)) != -1) {
        switch (opt) {
            case 'n': strncpy(m.name,        optarg, sizeof(m.name) - 1);        break;
            case 'v': strncpy(m.version,     optarg, sizeof(m.version) - 1);     break;
            case 'r': m.release = atoi(optarg);                                   break;
            case 'c': strncpy(m.category,    optarg, sizeof(m.category) - 1);    break;
            case 'u': strncpy(m.url,         optarg, sizeof(m.url) - 1);         break;
            case 'f': strncpy(local_file,    optarg, sizeof(local_file) - 1);    break;
            case 's': strncpy(m.summary,     optarg, sizeof(m.summary) - 1);     break;
            case 'l': strncpy(m.license,     optarg, sizeof(m.license) - 1);     break;
            case 'm': strncpy(m.maintainer,  optarg, sizeof(m.maintainer) - 1);  break;
            case 'H': strncpy(m.homepage,    optarg, sizeof(m.homepage) - 1);    break;
            case 'd': strncpy(m.deps_raw,    optarg, sizeof(m.deps_raw) - 1);    break;
            case 't': strncpy(m.install_type,optarg, sizeof(m.install_type) - 1);break;
            case 'o': strncpy(output_file,   optarg, sizeof(output_file) - 1);   break;
            case 'T': strncpy(github_token,  optarg, sizeof(github_token) - 1);  break;
            case 'I': strncpy(init_path,     optarg, sizeof(init_path) - 1);     break;
            case 'i': interactive = 1;                                            break;
            case 'D': dry_run = 1;                                                break;
            case 'b': do_batch = 1;                                               break;
            case 'h': print_usage(argv[0]); return 0;
            case 'V': printf("mlx-pkg-submit " VERSION "\n"); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Mode: --init — just write a blank template and exit                 */
    /* ------------------------------------------------------------------ */
    if (init_path[0]) {
        return init_template(init_path);
    }

    /* ------------------------------------------------------------------ */
    /* Mode: --batch — submit all remaining positional args as JSON files  */
    /* ------------------------------------------------------------------ */
    if (do_batch) {
        int remaining = argc - optind;
        if (remaining <= 0) {
            fprintf(stderr,
                "[error] --batch requires at least one manifest file path.\n"
                "        Usage: %s --batch [OPTIONS] file1.json file2.json ...\n",
                argv[0]);
            return 1;
        }

        if (!github_token[0]) {
            fprintf(stderr, "[error] No GitHub token. Use -T or set GITHUB_TOKEN.\n");
            return 1;
        }

        const char **batch_files = (const char **)&argv[optind];
        int failures = batch_submit(batch_files, remaining,
                                    github_token, TARGET_REPO);
        return failures > 0 ? 1 : 0;
    }

    /* ------------------------------------------------------------------ */
    /* Mode: single-package submission (original behaviour)               */
    /* ------------------------------------------------------------------ */
    if (interactive) {
        if (interactive_fill(&m, local_file, sizeof(local_file)) != 0) {
            fprintf(stderr, "[error] Interactive mode failed.\n");
            return 1;
        }
    }

    /* Validate required fields */
    if (!m.name[0] || !m.version[0] || !m.url[0] || !m.category[0]) {
        fprintf(stderr, "[error] Required: --name, --version, --url, --category\n"
                        "        Use -i for interactive mode or --batch for bulk.\n");
        return 1;
    }

    /* Compute SHA-256 */
    if (!m.sha256[0]) {
        if (local_file[0]) {
            printf("[*] Computing SHA-256 from local file: %s\n", local_file);
            if (sha256_file(local_file, m.sha256) != 0) {
                fprintf(stderr, "[error] Failed to hash file: %s\n", local_file);
                return 1;
            }
        } else {
            printf("[*] Downloading to compute SHA-256: %s\n", m.url);
            char tmp_path[256];
            snprintf(tmp_path, sizeof(tmp_path), "/tmp/mlx-submit-%s-%s.pkg",
                     m.name, m.version);
            if (http_download(m.url, tmp_path) != 0) {
                fprintf(stderr, "[error] Download failed.\n");
                return 1;
            }
            if (sha256_file(tmp_path, m.sha256) != 0) {
                fprintf(stderr, "[error] Failed to hash downloaded file.\n");
                unlink(tmp_path);
                return 1;
            }
            /* get file size */
            FILE *fp = fopen(tmp_path, "rb");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                m.size_bytes = (long long)ftell(fp);
                fclose(fp);
            }
            unlink(tmp_path);
        }
        printf("[*] SHA-256: %s\n", m.sha256);
    }

    /* Validate SHA-256 length */
    if (strlen(m.sha256) != 64) {
        fprintf(stderr, "[error] SHA-256 must be 64 hex chars, got %zu\n", strlen(m.sha256));
        return 1;
    }

    /* Render manifest JSON */
    char json_buf[8192];
    if (manifest_to_json(&m, json_buf, sizeof(json_buf)) != 0) {
        fprintf(stderr, "[error] Failed to render manifest JSON.\n");
        return 1;
    }

    if (dry_run) {
        printf("\n--- Manifest JSON ---\n%s\n", json_buf);
        return 0;
    }

    if (output_file[0]) {
        FILE *fp = fopen(output_file, "w");
        if (!fp) { perror(output_file); return 1; }
        fputs(json_buf, fp);
        fclose(fp);
        printf("[ok] Manifest written to: %s\n", output_file);
        return 0;
    }

    /* Submit to GitHub */
    if (!github_token[0]) {
        fprintf(stderr, "[error] No GitHub token. Use -T or set GITHUB_TOKEN.\n");
        return 1;
    }

    printf("[*] Submitting to %s ...\n", TARGET_REPO);
    int rc = github_submit(&m, json_buf, github_token, TARGET_REPO);
    if (rc == 0) {
        printf("[ok] Pull request created successfully!\n");
    } else {
        fprintf(stderr, "[error] GitHub submission failed (code %d).\n", rc);
        return 1;
    }

    return 0;
}
