# mlx-pkg-submit

> CLI utility for submitting packages to [makulinux-packages](https://github.com/kyrosystems/makulinux-packages)

Written in C. No runtime dependencies except `libcurl` and `libcrypto` (OpenSSL).

## Features

- Interactively or via flags fills out a package manifest
- Auto-computes SHA-256 from a local file or URL
- Validates the manifest before submitting
- Clones/forks `makulinux-packages`, writes the manifest, creates a branch and opens a PR — all from the terminal
- Can also just generate a manifest JSON without touching GitHub

## Build

```bash
# Dependencies (openSUSE Tumbleweed)
zypper install -y gcc make libcurl-devel openssl-devel

# Or Fedora/RHEL
dnf install -y gcc make libcurl-devel openssl-devel

make
sudo make install   # installs to /usr/local/bin/mlx-pkg-submit
```

## Usage

```
mlx-pkg-submit [OPTIONS]

Options:
  -n, --name <name>          Package name
  -v, --version <ver>        Package version
  -r, --release <num>        Release number (default: 1)
  -c, --category <cat>       Category (system/internet/desktop/devel/media/games/utils)
  -u, --url <url>            Download URL of the package
  -f, --file <path>          Local file to compute SHA-256 from (instead of downloading URL)
  -s, --summary <text>       Short one-line summary
  -l, --license <spdx>       SPDX license identifier (e.g. GPL-3.0)
  -m, --maintainer <email>   Maintainer email
  -H, --homepage <url>       Project homepage URL
  -d, --deps <d1,d2,...>     Comma-separated dependency list
  -t, --type <type>          Install type: rpm|tarball|appimage|flatpak (default: rpm)
  -o, --output <file>        Write manifest to file instead of submitting to GitHub
  -i, --interactive          Interactive mode (prompts for all fields)
  -T, --token <token>        GitHub personal access token (or set GITHUB_TOKEN env var)
  -D, --dry-run              Print manifest JSON, do not submit
  -h, --help                 Show this help
  -V, --version-info         Show version

Examples:
  # Interactive mode
  mlx-pkg-submit -i

  # From local file
  mlx-pkg-submit -n firefox -v 126.0 -c internet -f ./firefox-126.0.x86_64.rpm \
    -u https://example.com/firefox-126.0.rpm -s "Mozilla Firefox" -l MPL-2.0

  # Dry run - just print the manifest
  mlx-pkg-submit -n htop -v 3.3.0 -c utils -u https://... -s "Process viewer" -l GPL-2.0 -D

  # Output to file
  mlx-pkg-submit -i -o ./my-manifest.json
```

## Environment Variables

- `GITHUB_TOKEN` — GitHub PAT with `repo` scope for PR creation
- `MLX_PACKAGES_REPO` — override target repo (default: `kyrosystems/makulinux-packages`)
- `MLX_MAINTAINER` — default maintainer email
