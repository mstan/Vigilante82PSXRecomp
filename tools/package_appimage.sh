#!/usr/bin/env bash
# Native Linux payload, shared staging rules and pinned AppImage tooling.
set -euo pipefail
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
[[ "$(uname -s)" == Linux && "$(uname -m)" == x86_64 ]] || {
    echo 'AppImage packaging requires native Linux x86_64' >&2; exit 1;
}
root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:?Usage: package_appimage.sh BUILD_DIR RECOMPILER_BUILD [OUTPUT_DIR]}
recompiler=${2:?Supply the native Linux recompiler build}
out=${3:-"$root/dist"}
python=${V82_PYTHON:-python3}
jobs=${V82_APPIMAGE_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid AppImage jobs' >&2; exit 2; }
fw=${PSXRECOMP_ROOT:-"$root/psxrecomp-v4"}
version=$(tr -d '\r\n' < "$root/packaging/release/VERSION")
work=$(mktemp -d "${TMPDIR:-/tmp}/v82-appimage.XXXXXX")
appdir=$work/AppDir
payload=$appdir/usr/share/Vigilante82PSXRecomp
toolcache=${XDG_CACHE_HOME:-$HOME/.cache}/recomp-appimage-tools
mkdir -p "$payload" "$appdir/usr/bin" "$out" "$toolcache"
"$python" "$root/tools/package_release.py" --build-dir "$build" --stage "$payload" \
    --recompiler-build "$recompiler" --cache-dir "$build/cache" \
    --download-cache "$toolcache" --platform linux --framework "$fw"
mv "$payload/Vigilante82PSXRecomp" "$appdir/usr/bin/Vigilante82PSXRecomp"
ln -s ../share/Vigilante82PSXRecomp/assets "$appdir/usr/bin/assets"
"$python" - "$root" "$appdir" "$version" "$fw" <<'PY'
from pathlib import Path
from PIL import Image
import sys
root, appdir = map(Path, sys.argv[1:3])
text = (root / 'packaging/release/AppRun.template').read_text()
for key, value in dict(VERSION=sys.argv[3], APP_NAME='Vigilante82PSXRecomp',
                       EXE_NAME='Vigilante82PSXRecomp', PAYLOAD_DIR='Vigilante82PSXRecomp',
                       ENV_PREFIX='V82', ARTIFACT_NAME='Vigilante82PSXRecomp').items():
    text = text.replace('@' + key + '@', value)
(appdir / 'AppRun').write_text(text)
(appdir / 'AppRun').chmod(0o755)
Image.open(Path(sys.argv[4]) / 'assets/psxrecomp.png').resize((256, 256)).save(appdir / 'Vigilante82PSXRecomp.png')
(appdir / 'Vigilante82PSXRecomp.desktop').write_text('''[Desktop Entry]
Type=Application
Name=Vigilante82PSXRecomp
Exec=Vigilante82PSXRecomp
Icon=Vigilante82PSXRecomp
Categories=Game;
Terminal=false
''')
PY
fetch() {
    "$python" "$fw/tools/release_stage.py" fetch-pinned --url "$1" --sha256 "$2" --destination "$3"
    chmod +x "$3"
}
linuxdeploy=$toolcache/linuxdeploy-x86_64.AppImage
appimagetool=$toolcache/appimagetool-x86_64.AppImage
# Match the verified versioned tooling in the pinned shared AppImage wrapper.
fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d "$linuxdeploy"
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
    ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0 "$appimagetool"
# Deploy dependencies of the runtime, frozen importer, encoder and native
# emitters. AppRun's LD_LIBRARY_PATH also serves their payload-tree originals.
export NO_STRIP=1
"$linuxdeploy" --appimage-extract-and-run --appdir "$appdir" \
    --executable "$appdir/usr/bin/Vigilante82PSXRecomp" \
    --executable "$payload/media-tools/v82-media" \
    --executable "$payload/media-tools/psxavenc" \
    --executable "$payload/overlay_toolchain/psxrecomp-game" \
    --executable "$payload/overlay_toolchain/psxrecomp-bios" \
    --desktop-file "$appdir/Vigilante82PSXRecomp.desktop" \
    --icon-file "$appdir/Vigilante82PSXRecomp.png"
# Deployment may rewrite the runtime RPATH. Bind the receipt to these final
# bytes; the payload's prior digest cannot certify the rewritten executable.
"$python" "$fw/tools/release_stage.py" stage-execution \
    --binary "$appdir/usr/bin/Vigilante82PSXRecomp" \
    --manifest "$payload/Vigilante82PSXRecomp.execution.json" \
    --output "$appdir/usr/bin/Vigilante82PSXRecomp.execution.json"
rm -f "$payload/Vigilante82PSXRecomp.execution.json"
# Preserve attribution for Ubuntu-built dependencies that linuxdeploy copies.
mkdir -p "$payload/licenses/linux-dependencies"
for lib in "$appdir"/usr/lib/*; do
    name=$(basename "$lib")
    package=$(dpkg-query -S "*/$name" 2>/dev/null | head -1 | cut -d: -f1 || true)
    if [ -n "$package" ] && [ -f "/usr/share/doc/$package/copyright" ]; then
        cp "/usr/share/doc/$package/copyright" "$payload/licenses/linux-dependencies/$package.txt"
    fi
done
export ARCH=x86_64
export SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-$(git -C "$root" show -s --format=%ct HEAD)}
find "$appdir" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
artifact=$out/Vigilante82PSXRecomp-$version-linux-x64.AppImage
"$appimagetool" --appimage-extract-and-run \
    --mksquashfs-opt -processors --mksquashfs-opt "$jobs" "$appdir" "$artifact"
chmod +x "$artifact"
sha256sum "$artifact" > "$artifact.sha256"
printf 'AppDir retained for smoke checks: %s\n' "$appdir"
