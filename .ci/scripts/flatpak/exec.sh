#!/bin/bash -ex

mkdir -p "ccache"  || true
chmod a+x ./.ci/scripts/flatpak/docker.sh
docker run -e ENABLE_COMPATIBILITY_REPORTING -e CCACHE_DIR=/yuzu/ccache -v $(pwd):/yuzu yuzuemu/build-environments:linux-flatpak /bin/bash /yuzu/.ci/scripts/flatpak/docker.sh $1
