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

# HIVE-owned public SDK.
for relative in \
    app/include/hax.h \
    app/include/hax_widgets.h \
    app/include/hive.h
do
    copy_file "$relative"
done

# Transitional runtime mirror. src/gui/nogui.c remains HBOS-owned because it is
# the kernel's no-GUI implementation of the service contract.
while IFS= read -r source; do
    relative=${source#"$hbos_dir/"}
    [[ "$relative" == "src/gui/nogui.c" ]] || copy_file "$relative"
done < <(find "$hbos_dir/src/gui" -type f | sort)

for relative in \
    src/gpu.c \
    src/gpu.h \
    src/graphics/gui_font.c \
    src/graphics/gui_font.h \
    src/graphics/gui_glyph.asm \
    src/graphics/gui_icons.c \
    src/graphics/gui_icons.h \
    src/graphics/gui_iconsimg.asm \
    src/graphics/gui_wall.c \
    src/graphics/gui_wall.h \
    src/graphics/gui_wallimg.asm \
    src/tools/gui.c \
    tools/genfont.py \
    tools/genicon.py \
    tools/genwall.py
do
    copy_file "$relative"
done

# HBOS-owned ABI/libc snapshots needed by HIVE applications.
copy_file src/api/gui_service.h
while IFS= read -r source; do
    relative=${source#"$hbos_dir/"}
    copy_file "$relative"
done < <(find "$hbos_dir/src/user/libc" -type f -name '*.h' | sort)

# Public documentation and host-side regression tests.
copy_file docs/HBOS_HAX_API.html
copy_file docs/HIVE_DESKTOP_API.md
copy_file docs/REPOSITORY_SPLIT_BOUNDARIES.md
cp "$hbos_dir/scripts/test_hive_ui.c" "$hive_dir/tests/test_hive_ui.c"
cp "$hbos_dir/scripts/test_hive_winsrv.c" "$hive_dir/tests/test_hive_winsrv.c"

echo "Synced HIVE-owned sources and HBOS contract snapshots from $hbos_dir"
