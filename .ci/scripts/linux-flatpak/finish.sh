#!/bin/bash -ex

YUZU_SRC_DIR="/yuzu"
REPO_DIR="$YUZU_SRC_DIR/repo"

# When the script finishes, unmount the repository and delete sensitive files,
# regardless of whether the build passes or fails
umount "$REPO_DIR" || true
rm -rf "$REPO_DIR" "/tmp/*" || true

