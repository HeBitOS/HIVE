#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 /path/to/HBOS" >&2
    exit 2
}

[[ $# -eq 1 ]] || usage

hbos_dir=$(cd "$1" && pwd)
hive_dir=$(cd "$(dirname "$0")/.." && pwd)

[[ -f "$hbos_dir/Makefile" && -d "$hbos_dir/src/gui" ]] || {
    echo "error: not an HBOS source tree: $hbos_dir" >&2
    exit 1
}

copy_file() {
    local relative=$1
    local source="$hbos_dir/$relative"
    local target="$hive_dir/$relative"

    [[ -f "$source" ]] || {
        echo "error: missing HBOS source: $relative" >&2
        exit 1
    }
    mkdir -p "$(dirname "$target")"
    cp "$source" "$target"
}

# HIVE owns app/, the SDK, desktop runtime, resources, tests and documentation.
# Synchronization is intentionally one-way only for HBOS-owned contract
# snapshots; it must never restore kernel-linked application modules.

# HBOS-owned ABI/libc snapshots needed by HIVE applications.
copy_file src/api/gui_service.h
while IFS= read -r source; do
    relative=${source#"$hbos_dir/"}
    copy_file "$relative"
done < <(find "$hbos_dir/src/user/libc" -type f -name '*.h' | sort)

echo "Synced HBOS ABI/libc contract snapshots from $hbos_dir"
