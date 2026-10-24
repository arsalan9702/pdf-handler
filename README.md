# PDF Handler

A fast, private desktop PDF toolkit for Windows, macOS and Linux. Everything runs locally.

- **Organize pages**: reorder by dragging, rotate, delete, duplicate, extract a selection, combine several PDFs, with undo/redo
- **Merge** files (with page ranges per file and optional bookmarks) and **split** them (every page, every N pages, or by ranges)
- **Compress**: lossless, balanced or smallest
- **Protect** with AES-256 and permissions; **unlock** files you can open
- **Properties**: title, author, subject, keywords
- **Images to PDF** (JPEGs embedded without quality loss) and **PDF to images** (PNG/JPEG at any DPI)

**Safety:** every output is written to a temporary file and checked with QPDF (the same check as `qpdf --check`, plus a page-count check) before it replaces anything. If something fails, or you cancel, existing files are left as they were. Damaged PDFs are repaired where possible, and the repairs are reported.

## Install

Download the installer for your system from the [Releases](../../releases) page:

| System | File | Notes |
|---|---|---|
| Windows | `PDF-Handler-<version>-Setup.exe` | Start menu entry, "Open with" for PDFs, optional `pdfh` on PATH. Unsigned, so SmartScreen asks once: *More info → Run anyway*. |
| macOS | `PDF-Handler-<version>-macOS.dmg` | Drag to Applications. Unsigned: the first time, right-click the app → *Open*. |
| Linux | `PDF-Handler-<version>-x86_64.AppImage` | `chmod +x` it and run it, then *Settings → Add to launcher* (menu entry, "Open with", `pdfh` command). |

To install on Linux from a source build using the system Qt:

```sh
cmake --preset release && cmake --build --preset release
cmake --install build/release --prefix ~/.local/opt/pdf-handler
~/.local/opt/pdf-handler/bin/pdf-handler --install-desktop     # add --default to make it the PDF default
```

`--uninstall-desktop` removes the launcher entry, icons and the `pdfh` command again.

## Command line

`pdfh` runs every operation from a terminal (`pdf-handler --cli …` does the same):

```sh
pdfh info report.pdf
pdfh extract report.pdf "1-3,8-" -o summary.pdf
pdfh merge -o all.pdf a.pdf b.pdf --pages 2-5 c.pdf --bookmarks
pdfh split big.pdf -d parts/ --every 10
pdfh rotate scan.pdf 90 "2,4" -o fixed.pdf
pdfh encrypt contract.pdf -o locked.pdf --user secret --no-copy
pdfh decrypt locked.pdf -o open.pdf --password secret
pdfh compress slides.pdf -o smaller.pdf
pdfh images -o photos.pdf *.jpg --page a4 --margin 20
```

Page ranges are 1-based: `8-` means page 8 to the end, `z` is the last page, and `5-3` runs backwards.

## Development

See [CLAUDE.md](CLAUDE.md) for architecture and rules. Requirements: CMake 3.24+, Ninja, a C++23 compiler, Qt 6.9+ (Quick, Quick Controls 2, PDF, SVG), and vcpkg (`VCPKG_ROOT`) for QPDF and Catch2.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev   # debug build + tests
cmake --preset asan && cmake --build --preset asan && ctest --preset asan # sanitizers (core)
scripts/package-linux.sh build/release      # AppImage (build on an old distro for portability)
```

`PDFH_SCREENSHOT=out.png PDFH_PAGE=<tool> QT_QPA_PLATFORM=offscreen pdf-handler [files]` renders one screen to a PNG and exits. It's useful for checking the UI.

Releases: push a `v*` tag, and the *Package* workflow builds all three installers and publishes them.

## Licenses

Built with Qt (LGPL-3.0, linked dynamically), QPDF (Apache-2.0), PDFium via Qt PDF (BSD-3-Clause), the Inter typeface (OFL-1.1) and Lucide icons (ISC).
