#!/bin/bash -ex

cd /yuzu

ccache -s

sed -e s/@DISPLAY_VERSION@/$1/ -e s/@COMPATIBILITY_REPORTING@/${ENABLE_COMPATIBILITY_REPORTING:-"OFF"}/ dist/org.yuzu_emu.yuzu.yml.in > org.yuzu_emu.yuzu.yml

flatpak-builder build --ccache --repo /yuzu/repo org.yuzu_emu.yuzu.yml

ccache -s

# build a bundle
flatpak build-bundle --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo /yuzu/repo/ org.yuzu_emu.yuzu.flatpak org.yuzu_emu.yuzu
