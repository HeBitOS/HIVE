# HIVE

HIVE (HBOS Interface & Visual Environment) is a lightweight, modular desktop
environment for HBOS. It provides the window manager, compositor, desktop
shell, user-space GUI toolkit, and SDK for building independent graphical
applications.

## Repository layout

```text
app/
├── include/        Public HAX/HIVE application SDK
├── calc.c          Independent calculator
├── clock.c         Independent uptime clock
├── files.c         Independent file manager
├── hexview.c       Independent hexadecimal viewer
├── imgview.c       Independent BMP viewer
├── settings.c      Independent settings frontend
├── shortcuts.c     Independent shortcut reference
├── taskmgr.c       Independent process-signal frontend
├── widgets.c       HIVE toolkit showcase
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

HIVE `0.1-beta5-gui.3` provides Toolkit API 1.3: Label, Button, Textbox,
Checkbox, List, Progress, Slider, Scrollbar, Menu, Image, Canvas, and Panel
widgets; row/grid layout and widget trees; keyboard focus; hover/pressed states;
pointer capture; text selection; and UTF-8-safe editing.

The eight registration-based desktop applications are now independent HAX
processes. The desktop discovers them through HAX metadata, launches them
asynchronously, and passes file paths to the image and hexadecimal viewers.
Their former `src/gui/apps/` modules are no longer present or linked.

The repository is still in the physical-split transition for the desktop
runtime itself. Several older tools implemented directly inside
`src/tools/gui.c` (notes, file statistics, snake, browser, code workbench, and
terminal) remain to be extracted while the desktop, resources, and compositor
move from kernel-linked code to a ring3 service.

## Test

```sh
make test
```

The host-side test validates the public widget ABI, input state machine, widget
trees, layout helpers, slider dragging, mouse release behavior, text selection,
UTF-8 editing, and the rule that independent applications cannot include
desktop/kernel-private headers.

The transitional window server still depends on HBOS kernel contracts. Test it
against a local HBOS checkout with:

```sh
make integration-test HBOS_DIR=/path/to/HBOS
```

Build every independent application without building an ISO:

```sh
make apps HBOS_DIR=/path/to/HBOS
```

## Run

Keep an HBOS checkout next to this repository, then build and start the HIVE
sources and applications directly in QEMU:

```sh
make run
```

`make start` is an alias for `make run`; use `make run-uefi` for UEFI or
`make image` to build without starting QEMU. If HBOS is elsewhere, pass
`HBOS_DIR=/path/to/HBOS`. Integration artifacts are kept under
`build/hbos/` in this repository.

## Synchronizing the transitional mirror

Refresh only the HBOS-owned ABI and libc contract snapshots with:

```sh
make sync HBOS_DIR=/path/to/HBOS
```

The command never overwrites `app/`, the public HIVE SDK, the desktop runtime,
tests, resources, or documentation.

For a complete HBOS image, use the [HBOS repository](https://github.com/HeBitOS/HBOS).
The integration build supports `make core-only`, `make nogui`, and external
applications through `APP_DIR=/path/to/apps`.

## License

HIVE is licensed under the GNU Affero General Public License v3.0. See
[`LICENSE`](LICENSE).
