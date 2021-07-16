#!/bin/bash -ex

# Exit on error, rather than continuing with the rest of the script.
set -e

cd /yuzu

ccache -s

mkdir build || true && cd build
cmake .. \
      -DBoost_USE_STATIC_LIBS=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/usr/lib/ccache/g++ \
      -DCMAKE_C_COMPILER=/usr/lib/ccache/gcc \
      -DCMAKE_INSTALL_PREFIX="/usr" \
      -DDISPLAY_VERSION=$1 \
      -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON \
      -DENABLE_QT_TRANSLATION=ON \
      -DUSE_DISCORD_PRESENCE=ON \
      -DYUZU_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} \
      -DYUZU_USE_BUNDLED_FFMPEG=ON

make -j$(nproc)

ccache -s

ctest -VV -C Release

make install DESTDIR=AppDir
rm -vf AppDir/usr/bin/yuzu-cmd AppDir/usr/bin/yuzu-tester

# Download tools needed to build an AppImage
wget -nc https://github.com/yuzu-emu/ext-linux-bin/raw/main/appimage/linuxdeploy-x86_64.AppImage
wget -nc https://github.com/yuzu-emu/ext-linux-bin/raw/main/appimage/linuxdeploy-plugin-qt-x86_64.AppImage
wget -nc https://github.com/yuzu-emu/ext-linux-bin/raw/main/appimage/AppRun-patched-x86_64
wget -nc https://github.com/yuzu-emu/ext-linux-bin/raw/main/appimage/exec-x86_64.so
# Set executable bit
chmod 755 \
    AppRun-patched-x86_64 \
    exec-x86_64.so \
    linuxdeploy-x86_64.AppImage \
    linuxdeploy-plugin-qt-x86_64.AppImage

# Workaround for https://github.com/AppImage/AppImageKit/issues/828
export APPIMAGE_EXTRACT_AND_RUN=1

mkdir -p AppDir/usr/optional
mkdir -p AppDir/usr/optional/libstdc++
mkdir -p AppDir/usr/optional/libgcc_s

# Deploy yuzu's needed dependencies
./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt

# Workaround for building yuzu with GCC 10 but also trying to distribute it to Ubuntu 18.04 et al.
# See https://github.com/darealshinji/AppImageKit-checkrt
cp exec-x86_64.so AppDir/usr/optional/exec.so
cp AppRun-patched-x86_64 AppDir/AppRun
cp --dereference /usr/lib/x86_64-linux-gnu/libstdc++.so.6 AppDir/usr/optional/libstdc++/libstdc++.so.6
cp --dereference /lib/x86_64-linux-gnu/libgcc_s.so.1 AppDir/usr/optional/libgcc_s/libgcc_s.so.1
