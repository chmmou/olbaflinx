#/**
# * Copyright (C) 2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
# *
# * This program is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program.  If not, see <https://www.gnu.org/licenses/>.
# */
# The image for the run that drives the application from outside.
#
# It carries everything the test image carries, and on top of it the WebDriver
# that speaks to the accessibility interface. It is a second image rather than
# more lines in the first because the tool brings a second Qt with it: the
# framework packages it needs are built against the Qt of the distribution,
# while the application is built against the Qt this project declares as its
# minimum. Putting both into the image every test run pulls would make that run
# carry six hundred megabytes it never uses.
#
# The two Qt installations do not disturb each other, but only because the
# libraries of the tool are told where to look. See the note at the patchelf
# call below.

ARG QT_VERSION=6.8.3

# The image this one is built on. Named as an argument so that a local build
# can point it at an image built from the recipe next door rather than at the
# published one; the workflow leaves it alone.
ARG BASE_IMAGE=ghcr.io/chmmou/olbaflinx/ci-ubuntu

# The revision of the tool, pinned. A branch name would let a foreign
# repository decide what this image contains, and a tag can be moved.
ARG TOOL_REVISION=d45a21e8f1b3591dc921f0be85f1ecd834cbe413

FROM ubuntu:26.04 AS tool

ARG TOOL_REVISION

ENV DEBIAN_FRONTEND=noninteractive

# Built here rather than in the final stage, and this stage carries no Qt of
# its own beyond the one the distribution ships. In the final image the
# minimum-version Qt sits in the loader path system-wide, and a build against
# the framework packages picks up its libraries and fails at the linker with
# undefined references to symbols of the newer version.
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    cmake \
    ninja-build \
    g++ \
    pkg-config \
    extra-cmake-modules \
    qt6-base-dev \
    qt6-base-private-dev \
    qt6-wayland-dev \
    qt6-wayland-private-dev \
    libkf6windowsystem-dev \
    libkf6coreaddons-dev \
    kwayland-dev \
    libkpipewire-dev \
    plasma-wayland-protocols \
    libwayland-dev \
    libxkbcommon-dev \
    python3-flask \
    python3-lxml \
    python3-gi \
    python3-pyatspi \
    gobject-introspection \
    ruby

RUN git clone https://github.com/KDE/selenium-webdriver-at-spi.git /tmp/tool \
 && git -C /tmp/tool checkout --quiet ${TOOL_REVISION} \
 && cmake -S /tmp/tool -B /tmp/tool/build -G Ninja \
 && cmake --build /tmp/tool/build \
 && cmake --install /tmp/tool/build --prefix /opt/selenium-webdriver-at-spi

FROM ${BASE_IMAGE}:qt${QT_VERSION}

ARG QT_VERSION

LABEL org.opencontainers.image.title="OlbaFlinx Docker Image CI, run from outside"
LABEL org.opencontainers.image.description="The image that drives the built application through the accessibility interface."
LABEL org.opencontainers.image.authors="Alexander Saal <developer@olbaflinx.chm-projects.de> (@chmmou)"
LABEL org.opencontainers.image.source="https://github.com/chmmou/olbaflinx/tree/develop-qt6/res/ci"
LABEL org.opencontainers.image.licenses="GPL-3.0-or-later"

ENV DEBIAN_FRONTEND=noninteractive

# What the run needs beside the tool. Qt itself needs nothing added: the
# accessibility bridge is compiled into QtGui and is not a plugin. What is
# missing is the service that mediates between an application and a client, and
# a screen. The bridge hangs off the xcb platform and does not load under the
# offscreen one this image sets, so the run brings an X server of its own.
# Without a window manager no window holds the input focus and a synthesised
# key arrives nowhere.
RUN apt-get update && apt-get install -y --no-install-recommends \
    at-spi2-core \
    xvfb \
    matchbox-window-manager \
    libkf6windowsystem6 \
    libkf6coreaddons6 \
    libkwaylandclient6 \
    libkpipewire6 \
    libkpipewirerecord6 \
    python3-flask \
    python3-lxml \
    python3-gi \
    python3-pyatspi \
    python3-numpy \
    gir1.2-atspi-2.0 \
    gir1.2-gtk-3.0 \
    ruby \
    python3-venv \
 && apt-get clean && rm -rf /var/lib/apt/lists/*

# The client half, in an environment of its own. The distribution carries no
# Appium bindings, and installing them into the system python fails: pip wants
# to replace packages that apt owns and cannot uninstall them. The server side
# keeps using the system python with the packages above; only the run that
# speaks to it lives here.
#
# With the system packages visible, because one thing the driver does not
# forward is an announcement. The status bar of the application hands out no
# text and announces instead, so the run listens for that on the bus directly,
# next to everything else it does through the driver.
RUN python3 -m venv --system-site-packages /opt/appium-client \
 && /opt/appium-client/bin/pip install --no-cache-dir Appium-Python-Client==4.5.1

COPY --from=tool /opt/selenium-webdriver-at-spi/bin/ /usr/bin/
COPY --from=tool /opt/selenium-webdriver-at-spi/share/selenium-webdriver-at-spi/ \
                 /usr/share/selenium-webdriver-at-spi/
COPY --from=tool /opt/selenium-webdriver-at-spi/share/applications/ /usr/share/applications/

# The one place where the two Qt installations have to be kept apart. The
# programs of the tool are built against the Qt of the distribution, but this
# image puts the minimum-version Qt into the loader path system-wide, so
# without this they would load the wrong libraries and die on a missing symbol.
#
# RPATH rather than RUNPATH, and that is the point of --force-rpath: a RUNPATH
# applies to the binary alone, while the framework libraries these programs
# pull in need to find the same Qt. An RPATH is inherited by that search.
#
# The application is left alone and keeps loading its own Qt through the
# system-wide path, which is what the rest of this image is built for.
RUN patchelf --force-rpath --set-rpath /usr/lib/x86_64-linux-gnu \
        /usr/bin/selenium-webdriver-at-spi-appidlister \
        /usr/bin/selenium-webdriver-at-spi-inputsynth \
        /usr/bin/selenium-webdriver-at-spi-recorder \
        /usr/bin/selenium-webdriver-at-spi-screenshotter

# The launcher of the tool installs its python requirements on every run,
# unless it finds this marker. Left to itself it would reach out to the package
# index each time a test runs, which makes the run depend on a foreign server
# being up and slower for nothing. The requirements are in the image above; the
# marker says so.
RUN touch /tmp/selenium-requirements-installed

CMD ["/bin/bash"]
