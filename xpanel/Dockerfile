# Build image for OpenAPI-XPanel (Nuitka onefile, Linux x86_64).
# Built on Ubuntu 20.04 (glibc 2.31) so that the executable also runs on Ubuntu 22.04 and 24.04.
# The system Python is 3.8, which is why PySide6 is pinned to 6.6.3.1 (see xpanel/requirements.txt).
# xpanel/build.sh builds this image automatically as xpanel-build-focal:latest. The image does not
# contain any repository files; the repository is mounted at run time.
# If archive.ubuntu.com is not reachable, set XPANEL_APT_MIRROR when running build.sh,
# e.g. XPANEL_APT_MIRROR=<your apt mirror>/ubuntu ./xpanel/build.sh V2.0.0
FROM ubuntu:20.04
ARG APT_MIRROR=

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

RUN if [ -n "${APT_MIRROR}" ]; then \
        sed -i "s|http://archive.ubuntu.com/ubuntu|${APT_MIRROR}|g; s|http://security.ubuntu.com/ubuntu|${APT_MIRROR}|g" /etc/apt/sources.list; \
    fi \
    && apt-get update && apt-get install -y --no-install-recommends \
        python3 python3-venv python3-pip python3-dev \
        build-essential ca-certificates \
        libgl1 libegl1 libxkbcommon0 libfontconfig1 libdbus-1-3 libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu 20.04 ships pip 20.0.2 / setuptools 45, which cannot build pyproject.toml projects, and
# patchelf 0.10, which Nuitka's PySide6 plugin rejects (needs >= 0.12), so patchelf comes from pip.
# The versions below are the ones the packaging was verified with (Python 3.8.10).
RUN python3 -m venv /opt/xpanel-venv \
    && /opt/xpanel-venv/bin/pip install -U pip setuptools wheel \
    && /opt/xpanel-venv/bin/pip install "PySide6==6.6.3.1" "nuitka==4.3.5" "ordered-set==4.1.0" "zstandard==0.23.0" "patchelf==0.19.3.0" \
    && /opt/xpanel-venv/bin/pip list
