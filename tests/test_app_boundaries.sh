#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
expected=(calc clock files hexview imgview settings shortcuts taskmgr)

for app in "${expected[@]}"; do
    source="$root/app/$app.c"
    [[ -f "$source" ]] || {
        echo "missing independent application: app/$app.c" >&2
        exit 1
    }
    grep -q '^[[:space:]]*#include <hive\.h>' "$source" || {
        echo "app/$app.c does not include the public HIVE SDK" >&2
        exit 1
    }
    grep -q "HIVE_APP(\"$app\"" "$source" || {
        echo "app/$app.c has no matching HAX metadata" >&2
        exit 1
    }
done

if grep -R -n -E \
    'gui_state\.h|gui_draw\.h|gui_app\.h|winsrv\.h|wm\.h|src/(core|gui|tools)/' \
    "$root/app" --include='*.c'; then
    echo "an application depends on desktop/kernel-private headers" >&2
    exit 1
fi

for legacy in \
    "$root/src/gui/gui_app.h" \
    "$root/src/gui/gui_apps.c" \
    "$root/src/gui/apps"
do
    [[ ! -e "$legacy" ]] || {
        echo "legacy kernel-linked application path still exists: $legacy" >&2
        exit 1
    }
done

if grep -q -E 'gui_apps\.c|gui/apps/app_' "$root/mk/hbos-integration.mk"; then
    echo "integration build still links private application modules" >&2
    exit 1
fi

echo "HIVE independent application boundary tests passed"
