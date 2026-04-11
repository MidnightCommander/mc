#!/bin/bash

set -euo pipefail

TS_GRAMMARS_VERSION='2026.04.11'
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE_URL='https://github.com/jtyr/tree-sitter-grammars/releases/download'

usage() {
    cat <<'USAGE'
Usage: ts-grammars-download.sh [OPTIONS]

Download tree-sitter grammar files from GitHub releases.

Options:
  --source              Download source tarball (for static builds from source)
  --shared              Download shared library tarball (.so/.dylib/.dll)
  --static              Download static library tarball (.a)
  --latest              Use latest release instead of pinned version
  --platform=<platform> Override platform auto-detection
                        Supported: x86_64-linux, aarch64-linux,
                                   aarch64-macos, x86_64-macos,
                                   x86_64-windows
  -h, --help            Show this help message

At least one of --source, --shared, or --static must be specified.
USAGE
}

detect_platform() {
    local arch os

    arch="$(uname -m)"
    os="$(uname -s)"

    case "$arch/$os" in
        x86_64/Linux)
            echo 'x86_64-linux'
            ;;
        aarch64/Linux)
            echo 'aarch64-linux'
            ;;
        arm64/Darwin)
            echo 'aarch64-macos'
            ;;
        x86_64/Darwin)
            echo 'x86_64-macos'
            ;;
        x86_64/MINGW*)
            echo 'x86_64-windows'
            ;;
        *)
            echo "Error: unsupported platform: $arch/$os" >&2
            exit 1
            ;;
    esac
}

fetch_latest_version() {
    local version

    version="$(curl -s 'https://api.github.com/repos/jtyr/tree-sitter-grammars/releases/latest' \
        | grep -o '"tag_name": "[^"]*"' \
        | cut -d'"' -f4)"

    if [[ -z $version ]]; then
        echo 'Error: failed to fetch latest version from GitHub API' >&2
        exit 1
    fi

    echo "$version"
}

download_and_verify() {
    local version=$1
    local filename=$2
    local dest_dir=$3

    local tarball_url="$BASE_URL/$version/$filename"
    local checksum_url="$BASE_URL/$version/tree-sitter-grammars.sha256"
    local tarball_path="$dest_dir/$filename"
    local checksum_path="$dest_dir/tree-sitter-grammars.sha256"

    echo "Downloading $tarball_url ..."
    curl -fSL -o "$tarball_path" "$tarball_url"

    echo "Downloading $checksum_url ..."
    curl -fSL -o "$checksum_path" "$checksum_url"

    echo 'Verifying checksum ...'
    local expected
    expected="$(grep "$filename" "$checksum_path" | awk '{print $1}')"

    if [[ -z $expected ]]; then
        echo "Error: checksum not found for $filename" >&2
        exit 1
    fi

    local actual
    actual="$(sha256sum "$tarball_path" | awk '{print $1}')"

    if [[ $actual != "$expected" ]]; then
        echo "Error: checksum mismatch for $filename" >&2
        echo "  Expected: $expected" >&2
        echo "  Actual:   $actual" >&2
        exit 1
    fi

    echo 'Checksum OK.'
}

extract_source() {
    local version=$1
    local tmp_dir

    tmp_dir="$(mktemp -d)"
    trap "rm -rf '$tmp_dir'" EXIT

    local filename='tree-sitter-grammars-src.tar.gz'

    download_and_verify "$version" "$filename" "$tmp_dir"

    echo 'Extracting source tarball ...'
    tar -xzf "$tmp_dir/$filename" -C "$tmp_dir"

    local grammars_dest="$REPO_ROOT/src/editor/ts-grammars"
    local count=0

    # Tarball contains a top-level directory; iterate grammar dirs inside it
    for lang_dir in "$tmp_dir"/tree-sitter-grammars-*/*/; do
        local lang
        lang="$(basename "$lang_dir")"

        # Skip if not a grammar directory
        [[ -f "$lang_dir/src/parser.c" ]] || continue

        # Skip C++ scanners
        if [[ -f "$lang_dir/src/scanner.cc" ]]; then
            echo "  Skipping $lang (C++ scanner)"
            continue
        fi

        echo "  Extracting source: $lang"

        mkdir -p "$grammars_dest/$lang"
        cp "$lang_dir"/src/* "$grammars_dest/$lang/"

        count=$((count + 1))
    done

    echo "Source extraction complete: $count grammars."
}

extract_shared() {
    local version=$1
    local platform=$2
    local tmp_dir

    tmp_dir="$(mktemp -d)"
    # Only set trap if not already set by extract_source
    trap "rm -rf '$tmp_dir'" EXIT

    local filename="tree-sitter-grammars-$platform-shared.tar.gz"

    download_and_verify "$version" "$filename" "$tmp_dir"

    echo 'Extracting shared tarball ...'
    tar -xzf "$tmp_dir/$filename" -C "$tmp_dir"

    local shared_dest="$REPO_ROOT/ts-grammars-shared"
    local count=0

    mkdir -p "$shared_dest"

    # Tarball contains a top-level directory; iterate grammar dirs inside it
    for lang_dir in "$tmp_dir"/tree-sitter-grammars-*/*/; do
        local lang
        lang="$(basename "$lang_dir")"

        echo "  Extracting shared: $lang"

        mkdir -p "$shared_dest/$lang"

        # Copy shared library files
        for ext in so dylib dll; do
            for lib in "$lang_dir"/*."$ext"; do
                [[ -f $lib ]] && cp "$lib" "$shared_dest/$lang/"
            done
        done

        count=$((count + 1))
    done

    echo "Shared extraction complete: $count grammars."
}

extract_static() {
    local version=$1
    local platform=$2
    local tmp_dir

    tmp_dir="$(mktemp -d)"
    trap "rm -rf '$tmp_dir'" EXIT

    local filename="tree-sitter-grammars-$platform-static.tar.gz"

    download_and_verify "$version" "$filename" "$tmp_dir"

    echo 'Extracting static tarball ...'
    tar -xzf "$tmp_dir/$filename" -C "$tmp_dir"

    local grammars_dest="$REPO_ROOT/src/editor/ts-grammars"
    local count=0

    mkdir -p "$grammars_dest"

    # Tarball contains a top-level directory; iterate grammar dirs inside it
    for lang_dir in "$tmp_dir"/tree-sitter-grammars-*/*/; do
        local lang
        lang="$(basename "$lang_dir")"

        # Skip if no .a file
        [[ -f "$lang_dir/$lang.a" ]] || continue

        echo "  Extracting static: $lang"

        mkdir -p "$grammars_dest/$lang"
        cp "$lang_dir/$lang.a" "$grammars_dest/$lang/"

        count=$((count + 1))
    done

    echo "Static extraction complete: $count grammars."
}

main() {
    local do_source=0
    local do_shared=0
    local do_static=0
    local use_latest=0
    local platform=''

    while [[ $# -gt 0 ]]; do
        case $1 in
            --source)
                do_source=1
                shift
                ;;
            --shared)
                do_shared=1
                shift
                ;;
            --static)
                do_static=1
                shift
                ;;
            --latest)
                use_latest=1
                shift
                ;;
            --platform=*)
                platform="${1#--platform=}"
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                echo "Error: unknown option: $1" >&2
                usage >&2
                exit 1
                ;;
        esac
    done

    if [[ $do_source -eq 0 && $do_shared -eq 0 && $do_static -eq 0 ]]; then
        echo 'Error: at least one of --source, --shared, or --static must be specified' >&2
        usage >&2
        exit 1
    fi

    local version=$TS_GRAMMARS_VERSION

    if [[ $use_latest -eq 1 ]]; then
        echo 'Fetching latest version ...'
        version="$(fetch_latest_version)"
    fi

    echo "Using version: $version"

    if [[ ($do_shared -eq 1 || $do_static -eq 1) && -z $platform ]]; then
        platform="$(detect_platform)"
        echo "Detected platform: $platform"
    fi

    if [[ $do_source -eq 1 ]]; then
        extract_source "$version"
    fi

    if [[ $do_shared -eq 1 ]]; then
        extract_shared "$version" "$platform"
    fi

    if [[ $do_static -eq 1 ]]; then
        extract_static "$version" "$platform"
    fi

    echo 'Done.'
}

main "$@"
