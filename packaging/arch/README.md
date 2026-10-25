# Arch Linux package

The package installs the release binary, desktop entry, SVG icon, AppStream
metadata, and the GPL-3.0-only license.

From the repository root:

```bash
git archive --format=tar.gz --prefix=nekoplayer-0.1.0/ \
    -o packaging/arch/nekoplayer-0.1.0.tar.gz HEAD
cd packaging/arch
makepkg -si
```

`makepkg -si` installs the runtime dependencies (`qt6-base`, `qt6-translations`, `mpv`, `ffmpeg`,
`hicolor-icon-theme`) and the build tools (`cmake`, `ninja`, `qt6-tools`,
`pkgconf`) through pacman, then builds and installs the package.

The PKGBUILD uses `sha256sums=('SKIP')` because the archive is generated from
the local checkout. For an AUR or repository release, point `source` at the
published tarball and run `updpkgsums` before uploading.

License: GPL-3.0-only. This was selected because the package links the system
FFmpeg build (GPL-3.0-only) and libmpv (GPL-2.0-or-later/LGPL-2.1-or-later).
