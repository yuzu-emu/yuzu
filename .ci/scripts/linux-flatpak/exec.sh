#!/bin/bash -ex
mkdir -p "$HOME/.ccache"

# Configure docker and call the script that generates application data and build scripts
docker run --env-file .ci/scripts/linux-flatpak/azure-ci.env --env-file .ci/scripts/linux-flatpak/azure-ci-flatpak.env -v $(pwd):/yuzu -v "$HOME/.ccache":/root/.ccache -v "$HOME/.ssh":/root/.ssh --privileged yuzuemu/build-environments:linux-flatpak /bin/bash -ex /yuzu/.ci/scripts/linux-flatpak/docker.sh

