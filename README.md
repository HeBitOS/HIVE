# HIVE

HIVE (HBOS Interface & Visual Environment) is a lightweight, modular desktop
environment for HBOS. It provides the window manager, compositor, desktop
shell, user-space GUI toolkit, and SDK for building independent graphical
applications.

## Repository layout

```text
app/
├── include/        Public HAX/HIVE application SDK
├── widgets.c       HIVE 1.1 toolkit showcase
└── wdemo.c         Independent windowed application example
src/
├── api/            Snapshot of the HBOS ↔ HIVE service contract
├── graphics/       GUI font, icon, and wallpaper renderers
├── gui/            Window manager, compositor, runtime, and migration code
├── gpu.c/.h        Display backend used by the current runtime
└── tools/gui.c     Legacy in-kernel desktop shell pending ring3 migration
tests/              Host-side SDK and interaction tests
tools/              Resource generation utilities
docs/               API reference and repository-boundary documentation
```

Applications under `app/` only include the public SDK and keep their UI state
inside `hive_ui_t`. They must not include desktop-private headers such as
`gui_state.h`, `wm.h`, or `winsrv.h`.

## Current status

HIVE 1.1 provides Label, Button, Textbox, Checkbox, List, Progress, and Slider
widgets; row/grid layout; keyboard focus; hover/pressed states; pointer capture;
and UTF-8-safe navigation and deletion for existing textbox content.

This repository is in the physical-split transition. The public toolkit and the
sample HAX applications are already independent. The desktop runtime is kept as
an exact HBOS source-layout mirror while `src/tools/gui.c`, built-in apps, and
resource blobs are migrated from kernel-linked code to a ring3 service. HBOS
temporarily retains an integration copy so its full and no-GUI builds continue
to work throughout that migration.

## Test

```sh
make test
```

The test is host-side and validates the public widget ABI, input state machine,
layout helpers, slider dragging, mouse release behavior, and UTF-8 deletion.

For a complete HBOS image, use the [HBOS repository](https://github.com/HeBitOS/HBOS).
The integration build supports `make core-only`, `make nogui`, and external
applications through `APP_DIR=/path/to/apps`.

## License

HIVE is licensed under the GNU Affero General Public License v3.0. See
[`LICENSE`](LICENSE).
