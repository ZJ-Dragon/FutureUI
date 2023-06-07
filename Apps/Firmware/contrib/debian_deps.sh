#!/bin/sh -e
dpkg-reconfigure debconf -f noninteractive -p critical
apt update
apt install appstream           \
            build-essential     \
            git                 \
            help2man            \
            gettext             \
            gnutls-bin          \
            gnutls-dev          \
            libfwupd-dev        \
            libgpgme11-dev      \
            libgtk-4-dev        \
            libgudev-1.0-dev    \
            libjson-glib-dev    \
            libxmlb-dev         \
            libadwaita-1-dev    \
            meson               \
            -yq
