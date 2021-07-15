#!/bin/bash -ex
# This script generates the appdata.xml and org.yuzu.$REPO_NAME.json files
# needed to define application metadata and build yuzu

# Converts "yuzu-emu/yuzu-release" to "yuzu-release"
REPO_NAME=$(echo $AZURE_REPO_SLUG | cut -d'/' -f 2)
# Converts "yuzu-release" to "yuzu Release"
REPO_NAME_FRIENDLY=$(echo $REPO_NAME | sed -e 's/-/ /g' -e 's/\b\(.\)/\u\1/g')

# Generate the correct appdata.xml for the version of yuzu we're building
cat > /tmp/appdata.xml <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<application>
  <id type="desktop">org.yuzu.$REPO_NAME.desktop</id>
  <name>$REPO_NAME_FRIENDLY</name>
  <summary>Nintendo Switch emulator</summary>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>GPL-2.0</project_license>
  <description>
    <p>yuzu is an experimental open-source emulator for the Nintendo Switch from the creators of Citra.</p>
    <p>It is written in C++ with portability in mind, with builds actively maintained for Windows and Linux. The emulator is currently only useful for homebrew development and research purposes.</p>
    <p>yuzu only emulates a subset of Switch hardware and therefore is generally only useful for running/debugging homebrew applications. At this time, yuzu cannot play a majority of commercial games without major problems. yuzu can boot some commercial Switch games to varying degrees of success, but your experience may vary between games and for different combinations of host hardware.</p>
    <p>yuzu is licensed under the GPLv2 (or any later version). Refer to the license.txt file included.</p>
  </description>
  <url type="homepage">https://yuzu-emu.org/</url>
  <url type="donation">https://yuzu-emu.org/donate/</url>
  <url type="bugtracker">https://github.com/yuzu-emu/yuzu/issues</url>
  <url type="faq">https://yuzu-emu.org/wiki/faq/</url>
  <url type="help">https://yuzu-emu.org/wiki/home/</url>
  <screenshot>https://yuzu-emu.org/images/screenshots/001-Super%20Mario%20Odyssey.png</screenshot>
  <screenshot>https://yuzu-emu.org/images/screenshots/004-Super%20Mario%20Odyssey.png</screenshot>
  <screenshot>https://yuzu-emu.org/images/screenshots/019-Pokken%20Tournament.png</screenshot>
  <screenshot>https://yuzu-emu.org/images/screenshots/052-Pokemon%20Let%27s%20Go.png</screenshot>
  <categories>
    <category>Games</category>
    <category>Emulator</category>
  </categories>
</application>
EOF

cat > /tmp/yuzu-wrapper <<EOF
#!/bin/bash

# Discord only accepts activity updates from pids >= 10
for i in 1 2 3 .. 20
do
    # Spawn a new shell
    # This guarantees that a new process is created (unlike with potential bash internals like echo etc.)
    bash -c "true"
    sleep 0
done

# Symlink com.discordapp.Discord ipc pipes if they do not exist yet
for i in {0..9}; do
    test -S \$XDG_RUNTIME_DIR/app/com.discordapp.Discord/discord-ipc-\$i && ln -sf {\$XDG_RUNTIME_DIR/app/com.discordapp.Discord,\$XDG_RUNTIME_DIR}/discord-ipc-\$i;
done

yuzu \$@
EOF

# Generate the yuzu flatpak manifest
cat > /tmp/org.yuzu.$REPO_NAME.json <<EOF
{
    "app-id": "org.yuzu.$REPO_NAME",
    "runtime": "org.kde.Sdk",
    "runtime-version": "5.13",
    "sdk": "org.kde.Sdk",
    "command": "yuzu-wrapper",
    "rename-desktop-file": "yuzu.desktop",
    "rename-icon": "yuzu",
    "rename-appdata-file": "org.yuzu.$REPO_NAME.appdata.xml",
    "build-options": {
        "build-args": [
            "--share=network"
        ],
        "env": {
            "AZURE_BRANCH": "$AZURE_BRANCH",
            "AZURE_BUILD_ID": "$AZURE_BUILD_ID",
            "AZURE_BUILD_NUMBER": "$AZURE_BUILD_NUMBER",
            "AZURE_COMMIT": "$AZURE_COMMIT",
            "AZURE_JOB_ID": "$AZURE_JOB_ID",
            "AZURE_REPO_SLUG": "$AZURE_REPO_SLUG",
            "AZURE_TAG": "$AZURE_TAG"
        }
    },
    "finish-args": [
        "--device=all",
        "--socket=x11",
        "--socket=pulseaudio",
        "--share=network",
        "--share=ipc",
        "--filesystem=xdg-config/yuzu-emu:create",
        "--filesystem=xdg-data/yuzu-emu:create",
        "--filesystem=host:ro",	
        "--filesystem=xdg-run/app/com.discordapp.Discord:create",
        "--filesystem=xdg-run/discord-ipc-0:rw",
        "--filesystem=xdg-run/discord-ipc-1:rw",
        "--filesystem=xdg-run/discord-ipc-2:rw",
        "--filesystem=xdg-run/discord-ipc-3:rw",
        "--filesystem=xdg-run/discord-ipc-4:rw",
        "--filesystem=xdg-run/discord-ipc-5:rw",
        "--filesystem=xdg-run/discord-ipc-6:rw",
        "--filesystem=xdg-run/discord-ipc-7:rw",
        "--filesystem=xdg-run/discord-ipc-8:rw",
        "--filesystem=xdg-run/discord-ipc-9:rw"
    ],
    "modules": [
    {
        "name": "python2",
        "sources": [
            {
            "type": "archive",
            "url": "https://www.python.org/ftp/python/2.7.17/Python-2.7.17.tar.xz",
            "sha256": "4d43f033cdbd0aa7b7023c81b0e986fd11e653b5248dac9144d508f11812ba41"
            }
        ],
        "config-opts": [
            "--enable-shared",
            "--with-ensurepip=yes",
            "--with-system-expat",
            "--with-system-ffi",
            "--enable-loadable-sqlite-extensions",
            "--with-dbmliborder=gdbm",
            "--enable-unicode=ucs4"
        ],
        "post-install": [
            "chmod 644 \$FLATPAK_DEST/lib/libpython2.7.so.1.0"
        ],
        "cleanup": [
            "'*'"
        ]
    },
        {
            "name": "yuzu",
            "buildsystem": "cmake-ninja",
            "builddir": true,
            "config-opts": [
                "-DDISPLAY_VERSION=$1",
                "-DYUZU_USE_BUNDLED_UNICORN=ON",
                "-DYUZU_USE_QT_WEB_ENGINE=OFF",
                "-DCMAKE_BUILD_TYPE=Release",
                "-DYUZU_ENABLE_COMPATIBILITY_REPORTING=ON",
                "-DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON",
                "-DUSE_DISCORD_PRESENCE=ON",
                "-DENABLE_VULKAN=Yes"
            ],
            "cleanup": [
              "/bin/yuzu-cmd",
              "/share/man",
              "/share/pixmaps"
            ],
            "post-install": [
                "install -Dm644 ../appdata.xml /app/share/appdata/org.yuzu.$REPO_NAME.appdata.xml",
                "desktop-file-install --dir=/app/share/applications ../dist/yuzu.desktop",
                "install -Dm644 ../dist/yuzu.svg /app/share/icons/hicolor/scalable/apps/yuzu.svg",
                "sed -i 's/Name=yuzu/Name=$REPO_NAME_FRIENDLY/g' /app/share/applications/yuzu.desktop",
                "mv /app/share/mime/packages/yuzu.xml /app/share/mime/packages/org.yuzu.$REPO_NAME.xml",
                "sed 's/yuzu/org.yuzu.$REPO_NAME/g' -i /app/share/mime/packages/org.yuzu.$REPO_NAME.xml",
                'install -D ../yuzu-wrapper /app/bin/yuzu-wrapper',
                "desktop-file-edit --set-key=Exec --set-value='/app/bin/yuzu-wrapper %f' /app/share/applications/yuzu.desktop"
            ],
            "sources": [
                {
                    "type": "git",
                    "url": "https://github.com/yuzu-emu/$REPO_NAME.git",
                    "branch": "$AZURE_BRANCH",
                    "disable-shallow-clone": true
                },
                {
                    "type": "file",
                    "path": "/tmp/appdata.xml"
                },
                {
                    "type": "file",
                    "path": "/tmp/yuzu-wrapper"
                }
            ]
        }
    ]
}
EOF

