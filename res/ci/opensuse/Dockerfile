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
FROM opensuse/tumbleweed:latest

LABEL maintainer="developer@olbaflinx.chm-projects.de"

RUN zypper --non-interactive refresh && \
    zypper --non-interactive install --no-recommends \
        gcc \
        gcc-c++ \
        make \
        cmake \
        ninja \
        git \
        nano \
        libopenssl* \
        libtool \
        libgcrypt20* \
        libxmlsec1* \
        xmlsec1* \
        gettext \
        pkg-config \
        python3 \
        python3-pip \
        pcsc-lite* \
        qt6* \
        patchelf \
        tar \
        xz && \
    zypper clean --all

ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
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

# The Qt this image carries. Tumbleweed ships it, so the value follows the
# package rather than choosing it, and requirements.sh needs it to build the sql
# driver against the same module version.
ARG QT_VERSION=6.9.1
ENV QT_VERSION=${QT_VERSION}

WORKDIR /workspace

COPY ./libchipcard-client.pc.patch .
COPY ./requirements.sh .

RUN ./requirements.sh

RUN patch -p1 /usr/local/lib/pkgconfig/libchipcard-client.pc < ./libchipcard-client.pc.patch

RUN rm -rf ./libchipcard-client.pc.patch
RUN rm -rf ./requirements.sh

CMD ["/bin/bash"]
