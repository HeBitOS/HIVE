# HIVE

HIVE (HBOS Interface & Visual Environment) is a lightweight, modular desktop
environment for HBOS. It provides the window manager, compositor, desktop
shell, user-space GUI toolkit, and SDK for building independent graphical
applications. Package management is provided by
[HPT](https://github.com/HeBitOS/HPT), which lives in the HBOS repository
as a Git submodule; HIVE builds against the surrounding HBOS checkout's
`HPT/` directory instead of shipping its own copy.

## Clone

```sh
git clone --recurse-submodules https://github.com/HeBitOS/HIVE.git
cd HIVE
```

HIVE has no submodules of its own. Build it inside an HBOS checkout whose
`HPT/` submodule is initialized:

```sh
git clone --recurse-submodules https://github.com/HeBitOS/HBOS.git
git -C HBOS/HIVE checkout <revision>
cd HBOS/HIVE
```

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
├── tui.c           TUI (text terminal) control showcase
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

(HPT is a sibling submodule of the HBOS repository, not part of HIVE.)

Applications under `app/` only include the public SDK and keep their UI state
inside `hive_ui_t`. They must not include desktop-private headers such as
`gui_state.h`, `wm.h`, or `winsrv.h`.

### Multi-structure HAX applications

HAX applications are not limited to a single `.c` file. The HBOS multi-structure
build rules accept three layouts under `app/`:

```text
app/<name>.c        Single-file application (original style)
app/<name>/         Multi-file application: every *.c in the directory is
                    compiled and linked into one <name>.hax
app/lib/<lib>/      Standalone library: compiled once into
                    build/app/lib/<lib>.o and shared by any number of apps
```

An application declares which standalone libraries it links by listing one
library name per line in a deps file — `app/<name>/deps` for a directory app,
`app/<name>.deps` for a single-file app (`#` starts a comment). Library headers
are included as `<lib/xxx.h>` (the build adds `-I app/lib` automatically), and
libraries may depend on other libraries through `app/lib/<lib>/deps`.
`HAX_APP(...)` metadata must appear exactly once per application (conventionally
in `main.c`). The staging target copies `app/` recursively so every layout
survives into the HBOS build unchanged.

## Current status

HIVE `0.1-beta5-gui.4` provides Toolkit API 1.4: Label, Button, Textbox,
Checkbox, Radio, Toggle, List, Dropdown, Progress, Slider, Spinbox, Scrollbar,
Menu, Image, Canvas, Panel, Groupbox, and Separator widgets; row/grid layout
and widget trees; keyboard focus; hover/pressed states; pointer capture; text
selection; and UTF-8-safe editing.

The SDK also ships TUI Kit 1.0 (`app/include/hax_tui.h`): text-terminal
controls for `HAX_KIND_TUI` apps — title rules, progress bars, ASCII boxes and
tables, plus line-driven menu, confirm, input, and checkbox interactions. It is
pure user-space (no new syscalls), UTF-8-aware (CJK counted as two display
cells, truncation never splits codepoints), and EOF-safe: every interactive
control returns -1 instead of looping forever when stdin is exhausted.

HIVE uses HPT, its apt-style package manager for disk-installed HAX
applications, which ships as a submodule of the HBOS repository. HPT supports
local and IPv4 HTTP repositories plus CA-verified DNS/IPv4/IPv6 HTTPS
repositories with AAAA-first/A-record fallback, cached metadata, search,
dependencies, SHA-256 verification, install/remove/upgrade, and execution
through `execve`. A text-only (no-GUI) HBOS can still run `hpt` to manually
install HIVE applications. See the
[HPT documentation](https://github.com/HeBitOS/HPT/blob/main/docs/HPT.md).

The registration-based desktop applications are now independent HAX
processes. The desktop discovers them through HAX metadata, launches them
asynchronously, and passes file paths to the image and hexadecimal viewers.
Their former `src/gui/apps/` modules are no longer present or linked.

The repository is still in the physical-split transition for the desktop
runtime itself. Several older tools implemented directly inside
`src/tools/gui.c` (notes, file statistics, snake, browser, code workbench, and
terminal) remain to be extracted while the desktop, resources, and compositor
move from kernel-linked code to a ring3 service.

The built-in browser is currently a lightweight static reader, not a complete
JavaScript/DOM engine. The audited Vue/Pinia/Vite and media requirements for
modern Bilibili, together with the staged dual-backend implementation plan, are
documented in [`docs/BROWSER_BILIBILI_UPGRADE.md`](docs/BROWSER_BILIBILI_UPGRADE.md).
The complete backend pins its upstream revision and separates the Linux
reference build from the final HBOS port under
[`browser/chromium/`](browser/chromium/README.md).

## Test

```sh
make test
```

The host-side test validates the public widget ABI, input state machine, widget
trees, layout helpers, slider dragging, mouse release behavior, text selection,
UTF-8 editing, and the rule that independent applications cannot include
desktop/kernel-private headers. It also runs the TUI kit tests (display-cell
counting, UTF-8-safe truncation, progress/table/box rendering, and menu/confirm
parsing) and HPT's parser, version, SHA-256,
and repository-manifest tests.

The transitional window server still depends on HBOS kernel contracts. Test it
against a local HBOS checkout with:

```sh
make integration-test HBOS_DIR=/path/to/HBOS
```

Build every independent application, including HPT, without building an ISO:

```sh
make apps HBOS_DIR=/path/to/HBOS
```

Generate a local HPT repository from those application artifacts:

```sh
make repo HBOS_DIR=/path/to/HBOS
```

Serve the generated `build/repo` directory on Linux with
[HPT-server](https://github.com/HeBitOS/HPT-server).

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

## PDF reference manual

The compact, ink-saving API reference (`HBOS_HAX_API.pdf`, pure grayscale,
no background fills) is generated from `docs/HBOS_HAX_API.html` with
[WeasyPrint](https://weasyprint.org):

```sh
make pdf
```

The first run bootstraps a local WeasyPrint environment under
`build/pdfenv/` (requires `python3-venv` and network access); afterwards the
target reuses it. Override with `PDF_PYTHON=/path/to/python` to use an
existing WeasyPrint installation. The print stylesheet compacts the layout
(A4, 8.8 pt body) and forces a limited grayscale palette so the printed
manual uses as little ink as possible.

## Synchronizing the transitional mirror

Refresh only the HBOS-owned ABI and libc contract snapshots with:

```sh
make sync HBOS_DIR=/path/to/HBOS
```

The command never overwrites `app/`, the public HIVE SDK, the desktop runtime,
tests, resources, or documentation.

For a complete HBOS image, use the [HBOS repository](https://github.com/HeBitOS/HBOS).
HIVE's `apps`, `image`, `run`, and `run-uefi` targets stage HPT automatically
from the surrounding HBOS checkout and refuse to continue when it is missing.

## License

HIVE is licensed under the GNU Affero General Public License v3.0. See
[`LICENSE`](LICENSE).
