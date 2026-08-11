#/**
# * Copyright (C) 2025, Alexander Saal <developer@olbaflinx.chm-projects.de>
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
# The two values this file needs, each written once. They stand before the first
# FROM so that both stages inherit them; a stage that uses one re-declares the
# name without a default, which is how docker carries a global argument in.
#
# The Qt version used to stand at three places in this file and at a fourth in a
# patch file, with two of them disagreeing.
#
# The default is the minimum version the project declares. Building the image
# against it is what makes that declaration worth anything; a newer Qt would
# build too and would silently stop testing the floor. Pass --build-arg
# QT_VERSION=... to get a second image for a newer one.
ARG QT_VERSION=6.8.3

# The docking library. requirements.sh builds it; here the version is only
# needed to reach the installed file by name.
ARG ADS_VERSION=5.0.0

FROM ubuntu:26.04 AS verify

ARG QT_VERSION

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    wget \
    curl \
    ca-certificates \
    libglib-2.0 \
    libxkbcommon-* \
    libgcrypt20-dev \
    libgcrypt20 \
    libgpg-error-dev \
    libgpg-error0 \
    libgnutls28-dev \
    libgnutls30 \
    libgnutls-openssl27 \
    libxmlsec1-gnutls1 \
    libxmlsec1-dev \
    libxmlsec1-nss1 \
    libxmlsec1-openssl1 \
    libxmlsec1-1 \
    xmlsec1 \
    libgl1-mesa-dev \
    libc6-i386 \
    build-essential \
    libgl1-mesa-dev \
    mesa-common-dev \
    libgles2-mesa-dev \
    libxcb-* \
    fakeroot \
    libsqlite3-* \
    sqlite3 \
    python3 \
    python3-pip \
    python3-all \
    python3-all-dev \
    python3-xlrd \
    python3-requests \
    python3-semantic-version \
    python3-texttable \
    python3-defusedxml \
    python3-humanize \
    libpcsclite1 \
    libpcsclite-dev \
    doxygen \
    patch \
    git \
    cmake \
    ninja-build \
    pkgconf \
    libtool \
    automake \
    gettext \
    patchelf

RUN apt-get update && apt-get install -y locales libdbus-1* nano

# What a run driving the application from outside needs. Qt itself needs
# nothing: the accessibility bridge is compiled into QtGui and is not a plugin.
# What is missing is the service that mediates between an application and a
# client, and a screen. The bridge hangs off the xcb platform and does not load
# under the offscreen one this image sets, so the run brings an X server of its
# own. Without a window manager no window holds the input focus and a
# synthesised key arrives nowhere. The last two provide gsettings, which the
# test framework insists on before it will even import.
RUN apt-get update && apt-get install -y --no-install-recommends \
    at-spi2-core \
    xvfb \
    matchbox-window-manager \
    python3-dogtail \
    libglib2.0-bin \
    dconf-cli

RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

# The second one is not for the build. The run that drives the application from
# outside walks its way twice, under two languages, to show that it holds an
# element by an id no translation touches. Without a locale the C library knows,
# Qt falls back to C and both runs speak the same language, which would prove
# nothing. These are the two the project translates.
RUN locale-gen en_US.UTF-8 de_DE.UTF-8
RUN update-locale LC_ALL=en_US.UTF-8 \
    LANG=en_US.UTF-8 \
    LC_LANG=en_US.UTF-8 \
    LC_ADDRESS=en_US.UTF-8 \
    LC_IDENTIFICATION=en_US.UTF-8 \
    LC_MEASUREMENT=en_US.UTF-8 \
    LC_MONETARY=en_US.UTF-8 \
    LC_NAME=en_US.UTF-8 \
    LC_NUMERIC=en_US.UTF-8 \
    LC_PAPER=en_US.UTF-8 \
    LC_TELEPHONE=en_US.UTF-8 \
    LC_TIME=en_US.UTF-8

RUN pip install --break-system-packages aqtinstall
# No extra modules. The project links Concurrent, Core, Gui, LinguistTools,
# Sql, Svg and Widgets, all of which are in the base package. Two of the
# modules installed before, Qt Network Authorization and Qt WebEngine, carry
# licences that would bind the application if it ever used them.
RUN aqt install-qt linux desktop ${QT_VERSION} linux_gcc_64 --outputdir /opt/qt

FROM scratch AS root

# Re-declared without a default so that the values from before the first FROM
# reach this stage.
ARG QT_VERSION
ARG ADS_VERSION

LABEL org.opencontainers.image.title="OlbaFlinx Docker Image CI"
LABEL org.opencontainers.image.description="The officially containerd continuous integration docker image of OlbaFlinx."
LABEL org.opencontainers.image.authors="Alexander Saal <developer@olbaflinx.chm-projects.de> (@chmmou)"
LABEL org.opencontainers.image.url="https://github.com/chmmou/olbaflinx/tree/develop-qt6/res/ci"
LABEL org.opencontainers.image.documentation="https://github.com/chmmou/olbaflinx/tree/develop-qt6/res/ci"
LABEL org.opencontainers.image.source="https://github.com/chmmou/olbaflinx/tree/develop-qt6/res/ci"
LABEL org.opencontainers.image.licenses="GPL-3.0-or-later"
LABEL org.opencontainers.image.version="1.0"
LABEL org.opencontainers.image.revision="1"
LABEL org.opencontainers.image.created="2026-07-10 01:39 pm"

COPY --from=verify / /

ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8
ENV LC_LANG=en_US.UTF-8
ENV LC_ADDRESS=en_US.UTF-8
ENV LC_IDENTIFICATION=en_US.UTF-8
ENV LC_MEASUREMENT=en_US.UTF-8
ENV LC_MONETARY=en_US.UTF-8
ENV LC_NAME=en_US.UTF-8
ENV LC_NUMERIC=en_US.UTF-8
ENV LC_PAPER=en_US.UTF-8
ENV LC_TELEPHONE=en_US.UTF-8
ENV LC_TIME=en_US.UTF-8

ENV QT_QPA_PLATFORM="offscreen"
ENV QT_VERSION=${QT_VERSION}
ENV PATH="/opt/qt/${QT_VERSION}/gcc_64/bin:/opt/qt/${QT_VERSION}/gcc_64/libexec:${PATH}"

WORKDIR /workspace

COPY ./libchipcard-client.pc.patch .
COPY ./requirements.sh .

RUN ./requirements.sh

RUN patch -p1 /usr/lib/pkgconfig/libchipcard-client.pc < ./libchipcard-client.pc.patch
RUN patchelf --set-rpath "\$ORIGIN:/opt/qt/${QT_VERSION}/gcc_64/lib" \
    /usr/lib/x86_64-linux-gnu/libqtadvanceddocking-qt6.so.${ADS_VERSION}

RUN rm ./libchipcard-client.pc.patch
RUN rm ./requirements.sh

CMD ["/bin/bash"]
