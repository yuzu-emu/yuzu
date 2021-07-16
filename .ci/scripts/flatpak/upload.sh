#!/bin/bash -ex

. .ci/scripts/common/pre-upload.sh

REV_NAME="yuzu-flatpak-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.xz"
COMPRESSION_FLAGS="-cJvf"

if [ "${RELEASE_NAME}" = "mainline" ]; then
    DIR_NAME="${REV_NAME}"
else
    DIR_NAME="${REV_NAME}_${RELEASE_NAME}"
fi

mkdir "$DIR_NAME"

cp org.yuzu_emu.yuzu.flatpak "$DIR_NAME"

. .ci/scripts/common/post-upload.sh
