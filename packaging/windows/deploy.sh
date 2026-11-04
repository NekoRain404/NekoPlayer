#!/usr/bin/env bash
# Assemble a portable Windows directory from a built NekoPlayer.exe.
# Run inside an MSYS2 MINGW64 shell: packaging/windows/deploy.sh <exe> <outdir>
set -euo pipefail

exe=${1:?usage: deploy.sh <NekoPlayer.exe> <outdir>}
out=${2:?usage: deploy.sh <NekoPlayer.exe> <outdir>}
[[ -f "$exe" ]] || { echo "missing executable: $exe" >&2; exit 1; }

mkdir -p "$out"
cp -f "$exe" "$out/NekoPlayer.exe"

# Qt libraries, platform plugins and the MinGW runtime.
windeployqt --release --compiler-runtime --no-translations "$out/NekoPlayer.exe"

# libmpv/FFmpeg and any other non-Qt DLLs NekoPlayer links. Resolve them with
# ldd instead of hard-coding package layouts so MSYS2 upgrades keep working.
mapfile -t dependencies < <(
    ldd "$out/NekoPlayer.exe" |
        awk '/=> \/mingw64\/bin\// { print $3 }' |
        sort -u
)
for dependency in "${dependencies[@]}"; do
    name=$(basename "$dependency")
    case "$name" in
        libstdc++-6.dll|libgcc_s_seh-1.dll|libwinpthread-1.dll) continue ;;
    esac
    cp -f "$dependency" "$out/$name"
done

# FFmpeg/mpv may load additional codec DLLs at runtime; mirror the ones their
# own imports reference as well.
for dll in "$out"/*.dll; do
    [[ -f "$dll" ]] || continue
    ldd "$dll" 2>/dev/null |
        awk '/=> \/mingw64\/bin\// { print $3 }' |
        sort -u |
        while read -r dependency; do
            [[ -f "$out/$(basename "$dependency")" ]] || cp -f "$dependency" "$out/"
        done
done

cp -f LICENSE "$out/LICENSE"
cp -f packaging/windows/README-dist.txt "$out/README.txt"
echo "Portable build written to $out"
