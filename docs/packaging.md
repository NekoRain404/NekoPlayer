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
| Build | `cmake`, `ninja`, `pkgconf`, `qt6-tools` (`lrelease`); the `qt6-base`, `mpv`, and `ffmpeg` headers come from the runtime dependencies |

`base-devel` is assumed by `makepkg`.

Building NekoPlayer unconditionally requires Qt 6 Core/Concurrent/Widgets/
OpenGLWidgets and LinguistTools (`lrelease`), the libmpv client headers, and the
FFmpeg development libraries (`libavformat`, `libavcodec`, `libavutil`,
`libswscale`). In this PKGBUILD they come from `qt6-base`, `qt6-tools`, `mpv`,
and the `ffmpeg` package. `ffmpeg` is also a runtime dependency because
thumbnail decoding links it, so `-DBUILD_TESTING=OFF` does not remove the FFmpeg
requirement. Qt Test and Qt Network are needed only by the test suite, which
this package disables.

## Local build and install

The release workflow (`.github/workflows/release.yml`) publishes a
`nekoplayer-<version>.tar.gz` source archive plus `SHA256SUMS` for every `v*`
tag. The PKGBUILD tracks the most recently published asset; at the time of
writing that is v0.1.0, while the repository prepares the v0.2.0 pre-release.
Update `pkgver`, the source URL and `sha256sums` only after the new asset is
available; do not commit a placeholder checksum.

The PKGBUILD builds from the published v0.1.0 GitHub release asset:

```text
https://github.com/NekoRain404/NekoPlayer/releases/download/v0.1.0/nekoplayer-0.1.0.tar.gz
```

and pins that asset's SHA-256:

```text
c55795e1d251224c3f957fc401d20bc03c8122ab532508826600ac1119677bf8
```

A `git archive` of the current working tree is not the published release
asset, so `makepkg` rejects it against the pinned checksum. To build the current
working tree, generate the archive under the name the PKGBUILD expects and
choose one of the two options below.

```bash
# from the repository root
git archive --format=tar.gz --prefix=nekoplayer-0.1.0/ \
    -o packaging/arch/nekoplayer-0.1.0.tar.gz HEAD
cd packaging/arch
```

Option A: regenerate the checksum for the local archive, then build
(`updpkgsums` is provided by `pacman-contrib`):

```bash
updpkgsums
makepkg -si
```

Option B: temporarily skip the checksum for an unshared local build:

```bash
sed -i "s/^sha256sums=.*/sha256sums=('SKIP')/" PKGBUILD
makepkg -si
```

Restore the committed `PKGBUILD` afterwards (`git restore packaging/arch/PKGBUILD`
from the repository root). `makepkg -si` resolves and installs missing
dependencies with pacman, builds a Release binary, installs the package, and
then removes the build directory. Do not publish a `SKIP` checksum: for a
release or AUR upload, keep the pinned checksum for the published asset and
update it with `updpkgsums` whenever that asset changes.

## Installed files

- `/usr/bin/NekoPlayer`
- `/usr/share/applications/nekoplayer.desktop`
- `/usr/share/icons/hicolor/scalable/apps/nekoplayer.svg`
- `/usr/share/metainfo/nekoplayer.metainfo.xml`
- `/usr/share/licenses/nekoplayer/LICENSE`

`appstreamcli validate data/nekoplayer.metainfo.xml` passes (pedantic notes
only).

## Desktop integration

The desktop entry launches `Exec=NekoPlayer %F`, so `~/.local/bin` must be on
`PATH` when installing under `~/.local`. If the entry or icon does not appear,
refresh the user databases:

```bash
update-desktop-database ~/.local/share/applications
gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor
```

Use `cmake --install build/debug --prefix /tmp/neko-install` to inspect the
install layout without touching the user profile.

## Verification boundary

The package proves installation and linkage through pacman. Native Wayland
visual/keyboard/pointer acceptance, HDR output, GPU hardware decoding, physical
audio devices, mixed-scale multi-monitor behavior, and Windows remain separate
acceptance items; see `docs/verification.md`.
