#!/bin/sh -e

meson   -Dlibxmlb:gtkdoc=false                  \
        -Dlibxmlb:introspection=false           \
        -Dlibjcat:man=false                     \
        -Dlibjcat:gtkdoc=false                  \
        -Dlibjcat:introspection=false           \
        -Dlibjcat:tests=false                   \
        -Dfwupd:gtkdoc=false                    \
        -Dfwupd:introspection=false             \
        -Dfwupd:build=library                   \
        -Dfwupd:man=false                       \
        -Dfwupd:tests=false                     \
        -Dgcab:docs=false                       \
        -Dgcab:introspection=false              \
        -Dgcab:vapi=false                       \
        -Dgcab:tests=false                      \
        -Dman=true                              \
        build .
ninja -v -C build
