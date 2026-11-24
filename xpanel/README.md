# OpenAPI-XPanel

OpenAPI-XPanel is the graphical control panel for the Lixel OpenAPI: publish requests to a
device and subscribe to its reports interactively. **Linux only** (Ubuntu 20.04 / 22.04 /
24.04, x86_64). It uses the Python client library in [../python](../python/README.md) as a
package dependency. The repository overview is in [../README.md](../README.md).

> English README. 中文版见 [README.zh-CN.md](README.zh-CN.md)。

**All commands on this page are run from the repository root**, so every path starts with
`xpanel/`.

## Features

- Publish panel: topic and message-type drop-downs, JSON templates, one-click publish
- Subscribe panel: topic and wildcard subscriptions, received-message list with a detail view
- Optional OpenAPI Key field in the connection bar
- MQTT / MQTTS (TLS) connection to the device
- English / Chinese UI, switchable at runtime via **Settings → Language**
- One-file executable packaging via Nuitka

## Supported systems and runtime requirements

| Item | Requirement |
|------|-------------|
| OS | Ubuntu 20.04, 22.04 or 24.04 (x86_64) |
| Libraries | `libxcb-cursor0` (needed by Qt 6.5+ for the X11 platform plugin) and the Qt runtime libraries listed below |

```bash
sudo apt install libxcb-cursor0 libxcb-icccm4 libxcb-keysyms1 libxcb-shape0 libxcb-randr0 \
    libxkbcommon0 libxkbcommon-x11-0 libglib2.0-0 libgl1 libegl1 libfontconfig1 libdbus-1-3 \
    libgssapi-krb5-2
```

The executable is built on Ubuntu 20.04 (glibc 2.31), so it also runs on the newer releases; the
highest glibc symbol it uses is `GLIBC_2.29`. It was started with `--self-test` on clean
`ubuntu:20.04`, `ubuntu:22.04` and `ubuntu:24.04` images with the command above, with
`QT_QPA_PLATFORM=offscreen` and on a virtual X display (xvfb). On a desktop installation most of
these packages are already present, and `apt` skips them. Without `libxcb-cursor0` the X11 platform
plugin does not load and the program exits with code 134; `QT_QPA_PLATFORM=offscreen` still works.

## Run from source

```bash
# Create xpanel/venv, install PySide6==6.6.3.1 and the client library (pip install -e python),
# and generate the protobuf module
./xpanel/build-venv.sh

# Run OpenAPI-XPanel (English UI by default; use --lang zh for Chinese)
source xpanel/venv/bin/activate
python xpanel/main.py
```

Running from source needs Python 3.8 to 3.12 (the range PySide6 6.6.3.1 supports) and the
packages above.

Command-line arguments of `xpanel/main.py`:

| Argument | Description |
|----------|-------------|
| `--lang zh` | Start with the Chinese UI (default: English) |
| `--self-test` | Hidden check option: build the main window, print one `XPANEL_SELF_TEST_OK ...` line, exit with code 0 without showing a window. Used to check a packaged executable on a new system, together with `QT_QPA_PLATFORM=offscreen` |

## Building the executable

```bash
./xpanel/build.sh V2.0.0       # version is optional, defaults to V0.0.0
# Output: xpanel/dist/OpenAPI-XPanel, plus LICENSE.txt and THIRD-PARTY-NOTICES.txt next to it
```

`build.sh` needs Docker and network access (if `archive.ubuntu.com` is not reachable from your network,
set `XPANEL_APT_MIRROR`, for example `XPANEL_APT_MIRROR=<your apt mirror>/ubuntu ./xpanel/build.sh V2.0.0`;
the variable is used only when the image is built). It builds inside the image
`xpanel-build-focal:latest` (Ubuntu 20.04, system Python 3.8, Nuitka, PySide6 6.6.3.1), and
builds that image from [Dockerfile](Dockerfile) when it does not exist. It works on a staging
copy of `xpanel/`, `python/`, `proto/` and `LICENSE.txt`: the version and build time are written
into the copy of `openapi_xpanel/_build_info.py`, and the protobuf module is generated inside the
container, so no source file in the checkout is changed.

To check the result on a clean system:

```bash
QT_QPA_PLATFORM=offscreen xpanel/dist/OpenAPI-XPanel --self-test
```

## Why PySide6 6.6.3.1

PySide6 is pinned to `6.6.3.1` in [requirements.txt](requirements.txt). It is the last release
that supports Python 3.8, the system Python of Ubuntu 20.04, and its Linux wheels need
glibc 2.28 or newer. Later releases require Python 3.9 or newer.

## Localization

Source strings are written in English and wrapped with `tr()`. The Chinese translation lives
in `xpanel/openapi_xpanel/i18n/openapi_zh.ts` and is compiled to `.qm`. The interface
descriptions shown in the Publish panel come from `python/example/core/api_catalog.py`, so their
text in the `.ts` file must match it character by character. Edit the `.ts` by hand, because
`lupdate` rewrites the whole file, then compile it:

```bash
bash xpanel/openapi_xpanel/i18n/build_i18n.sh release   # compile .ts -> .qm
```

`build_i18n.sh` needs the system Qt tools `lrelease` (and `lupdate` for the `update` command).

## Directory structure

```
xpanel/
├── main.py                  # Entry point
├── openapi_xpanel/          # XPanel package (internal, not a public API)
│   ├── _build_info.py       # APP_VERSION / APP_BUILD_TIME, stamped by build.sh in its staging copy
│   ├── gui/                 # Control / publish / subscribe panels
│   └── i18n/                # Translations (.ts / .qm) + build_i18n.sh
├── assets/                  # Application icon (icon.ico / icon.png)
├── fonts.conf               # Font configuration, packaged with the executable
├── requirements.txt         # PySide6==6.6.3.1
├── Dockerfile               # Build image (Ubuntu 20.04)
├── build.sh                 # Nuitka packaging in Docker
├── build-venv.sh            # Create the local development environment
└── THIRD-PARTY-NOTICES.txt  # Notices shipped with the executable
```

The release package contains `OpenAPI-XPanel`, `LICENSE.txt` and `THIRD-PARTY-NOTICES.txt`;
`build.sh` fails if either of the two text files is missing.

## License

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
