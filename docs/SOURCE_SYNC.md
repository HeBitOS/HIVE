# Source synchronization

The initial HIVE source import was taken from HBOS commit `2febb64` on
2026-07-28.

## Ownership

- `app/include/` and the independent applications in `app/` are HIVE-owned
  public interfaces and application code.
- `src/gui/`, the `src/graphics/gui_*` files, `src/gpu.*`, and
  `src/tools/gui.c` are the transitional desktop/runtime mirror.
- `src/api/gui_service.h` and `src/user/libc/*.h` are snapshots of HBOS
  contracts required to compile or test HIVE. HBOS remains authoritative for
  those kernel and HAX platform contracts.

## Transition rule

While the runtime is still linked into HBOS, functional runtime changes must be
kept compatible in both repositories. New applications must target only
`app/include/`; they must not depend on the transitional runtime mirror.

The mirror ends when the desktop loop, compositor, resource loader, and window
server run as a ring3 service against a versioned HBOS Kernel SDK. At that point
HIVE becomes authoritative for the runtime, and HBOS consumes a tagged HIVE
release instead of retaining copied sources.
