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
FROM ubuntu:24.04 AS verify

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
    libxmlsec1-gnutls \
    libxmlsec1-dev \
    libxmlsec1-gcrypt \
    libxmlsec1-nss \
    libxmlsec1-openssl \
    libxmlsec1 \
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

RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

RUN locale-gen en_US.UTF-8
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
RUN aqt install-qt linux desktop 6.9.1 linux_gcc_64 --outputdir /opt/qt -m qt5compat qtnetworkauth qtpdf qtwebchannel qtwebengine qtwebsockets qtwebview

FROM scratch AS root

LABEL org.opencontainers.image.title="OlbaFlinx Docker Image CI"
LABEL org.opencontainers.image.description="The officially containerd continuous integration docker image of OlbaFlinx."
LABEL org.opencontainers.image.authors="Alexander Saal <developer@olbaflinx.chm-projects.de> (@chmmou)"
LABEL org.opencontainers.image.url="https://github.com/chmmou/olbaflinx/tree/develop/res/ci/README.md"
LABEL org.opencontainers.image.documentation="https://github.com/chmmou/olbaflinx/tree/develop/res/ci"
LABEL org.opencontainers.image.source="https://github.com/chmmou/olbaflinx/tree/develop/res/ci"
LABEL org.opencontainers.image.licenses="GPL-3.0-or-later"
LABEL org.opencontainers.image.version="1.0"
LABEL org.opencontainers.image.revision="1"
LABEL org.opencontainers.image.created="2025-06-20 01:56 pm"

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
ENV PATH="/opt/qt/6.9.1/gcc_64/bin:/opt/qt/6.9.1/gcc_64/libexec:${PATH}"

WORKDIR /workspace

COPY ./libchipcard-client.pc.patch .
COPY ./qsqlcipher.patch .
COPY ./requirements.sh .

RUN ./requirements.sh

RUN patch -p1 /usr/lib/pkgconfig/libchipcard-client.pc < ./libchipcard-client.pc.patch
RUN patchelf --set-rpath '$ORIGIN:/opt/qt/6.9.1/gcc_64/lib' /usr/lib/x86_64-linux-gnu/libqtadvanceddocking-qt6.so.4.4.0

RUN rm ./libchipcard-client.pc.patch
RUN rm ./qsqlcipher.patch
RUN rm ./requirements.sh

CMD ["/bin/bash"]
