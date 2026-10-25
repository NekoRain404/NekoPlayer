# Arch Linux Packaging

NekoPlayer ships an Arch Linux `PKGBUILD` under `packaging/arch/`.

## License

The package is licensed **GPL-3.0-only**. This is required by the current
distribution linkage: the binary links the Arch `ffmpeg` package, which is
`GPL-3.0-only`, and the Arch `mpv` package, which is
`GPL-2.0-or-later`/`LGPL-2.1-or-later`. Qt 6 is available under
`LGPL-3.0-only`/`GPL-3.0-only` and is dynamically linked. The license text is
installed as `/usr/share/licenses/nekoplayer/LICENSE`.

Changing the license requires re-evaluating the linked dependency licenses and
the distributed source archive.

## Runtime and build dependencies

| Kind | Arch packages |
| --- | --- |
| Runtime | `qt6-base`, `qt6-translations`, `mpv`, `ffmpeg`, `hicolor-icon-theme` |
| Build | `cmake`, `ninja`, `qt6-tools` (`lrelease`), `pkgconf` |

`base-devel` is assumed by `makepkg`.

## Local build and install

From the repository root:

```bash
git archive --format=tar.gz --prefix=nekoplayer-0.1.0/ \
    -o packaging/arch/nekoplayer-0.1.0.tar.gz HEAD
cd packaging/arch
makepkg -si
```

`makepkg -si` resolves and installs missing dependencies with pacman, builds a
Release binary, installs the package, and then removes the build directory.

The PKGBUILD intentionally uses `sha256sums=('SKIP')` for the locally generated
archive. For an AUR upload, point `source` at the published release tarball and
run `updpkgsums` before uploading.

## Installed files

- `/usr/bin/NekoPlayer`
- `/usr/share/applications/nekoplayer.desktop`
- `/usr/share/icons/hicolor/scalable/apps/nekoplayer.svg`
- `/usr/share/metainfo/nekoplayer.metainfo.xml`
- `/usr/share/licenses/nekoplayer/LICENSE`

`appstreamcli validate data/nekoplayer.metainfo.xml` passes (pedantic notes
only).

## Verification boundary

The package proves installation and linkage through pacman. Native Wayland
visual/keyboard/pointer acceptance, HDR output, GPU hardware decoding, physical
audio devices, mixed-scale multi-monitor behavior, and Windows remain separate
acceptance items; see `docs/verification.md`.
