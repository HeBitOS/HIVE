#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTENT_SHELL="${HIVE_CONTENT_SHELL:-}"
if [[ -z "$CONTENT_SHELL" ]]; then
    echo "set HIVE_CONTENT_SHELL to the built content_shell executable" >&2
    exit 2
fi
if [[ ! -x "$CONTENT_SHELL" ]]; then
    echo "content_shell is not executable: $CONTENT_SHELL" >&2
    exit 2
fi

if [[ ! -f "$TEST_DIR/node_modules/vue/dist/vue.esm-browser.prod.js" ]]; then
    (cd "$TEST_DIR" && npm ci --ignore-scripts --no-audit --no-fund)
fi

PORT="${HIVE_WEB_TEST_PORT:-18765}"
LOG_FILE="$(mktemp)"
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$TEST_DIR" \
    >"$LOG_FILE" 2>&1 &
SERVER_PID=$!
cleanup() {
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    rm -f "$LOG_FILE"
}
trap cleanup EXIT

URL="http://127.0.0.1:$PORT/index.html"
OUTPUT="$(timeout 60s "$CONTENT_SHELL" \
    --run-web-tests --ozone-platform=headless --disable-gpu --no-sandbox \
    "$URL" 2>&1)"
printf '%s\n' "$OUTPUT"
grep -q 'HIVE_MODERN_WEB: PASS' <<<"$OUTPUT"
