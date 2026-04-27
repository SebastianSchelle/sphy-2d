#!/usr/bin/env bash
# Convert common raster images under a root folder to uncompressed DDS (no mipmaps)
# using ImageMagick 7+ (`magick`). Original files are never deleted.
#
# Usage:
#   ./convert-dds.sh [--recursive|-r] <root-folder>
#
# Requires: magick (ImageMagick 7+)

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: convert-dds.sh [--recursive|-r] <root-folder>

  <root-folder>   Directory to scan for images (required).

  --recursive, -r Descend into subdirectories (default: only files directly in <root-folder>).

  -h, --help      Show this help.

Original images are kept. Each matching file produces a sibling <name>.dds
(same basename, .dds extension).

Requires ImageMagick 7+: `magick` on PATH.
EOF
}

if ! command -v magick >/dev/null 2>&1; then
    echo "error: 'magick' not found (install ImageMagick 7+)" >&2
    exit 1
fi

recursive=0
root=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -r | --recursive)
            recursive=1
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        -*)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            if [[ -n "$root" ]]; then
                echo "error: multiple paths given; expected one root folder" >&2
                usage >&2
                exit 1
            fi
            root="$1"
            shift
            ;;
    esac
done

if [[ -z "$root" ]]; then
    echo "error: missing <root-folder>" >&2
    usage >&2
    exit 1
fi

if [[ ! -d "$root" ]]; then
    echo "error: not a directory: $root" >&2
    exit 1
fi

root=$(cd "$root" && pwd)

# ImageMagick DDS: uncompressed, no mipmaps (see `magick -list format` / define docs).
magick_dds_opts=(
    -compress none
    -define "dds:compression=none"
    -define "dds:mipmaps=0"
)

# Common extensions (case-insensitive via find -iname).
find_expr=(
    \(
    -iname "*.png"
    -o -iname "*.jpg"
    -o -iname "*.jpeg"
    -o -iname "*.gif"
    -o -iname "*.bmp"
    -o -iname "*.tga"
    -o -iname "*.tif"
    -o -iname "*.tiff"
    -o -iname "*.webp"
    -o -iname "*.ico"
    \)
)

find_args=( "$root" )
if [[ "$recursive" -eq 0 ]]; then
    find_args+=( -maxdepth 1 )
fi
find_args+=( -type f "${find_expr[@]}" )

count=0
errors=0

while IFS= read -r -d '' src; do
    dir=$(dirname -- "$src")
    base=$(basename -- "$src")
    stem="${base%.*}"
    out="${dir}/${stem}.dds"

    # Skip if source is already DDS (should not match find, but be safe).
    if [[ "${base,,}" == *.dds ]]; then
        continue
    fi

    if ! magick "$src" "${magick_dds_opts[@]}" "$out"; then
        echo "warn: failed: $src -> $out" >&2
        errors=$((errors + 1))
        continue
    fi
    echo "ok: $src -> $out"
    count=$((count + 1))
done < <(find "${find_args[@]}" -print0)

echo "done: converted $count file(s)$([[ $errors -gt 0 ]] && echo ", $errors error(s)")"
