# Modern Web acceptance

This page is served only from loopback and pins Vue 3.5.40 in `package-lock.json`.
It verifies DOM, CSS Grid/custom properties, ES modules, async fetch/JSON,
WebAssembly, Vue hydration/reactivity, and the MSE/Worker API surfaces.

After building the pinned Chromium `content_shell`:

```sh
HIVE_CONTENT_SHELL=/path/to/chromium/src/out/hbos-host/content_shell \
  tests/browser/modern-web/run_content_shell.sh
```

A run counts for G2 only when the output contains `HIVE_MODERN_WEB: PASS`.
