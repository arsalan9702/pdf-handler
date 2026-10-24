# CLAUDE.md

Guidance for Claude when working in this repository.

## Project

Cross-platform desktop PDF toolkit written in C++. Features: extract specific pages, merge, split, organize (reorder/rotate/delete), compress, encrypt/decrypt, edit metadata, images-to-PDF, PDF-to-images.

Targets: Windows, macOS, Linux. The UI must look polished and identical on all three.

## Stack

| Concern | Library | Notes |
|---|---|---|
| GUI | Qt 6, Qt Quick (QML), Qt Quick Controls `Basic` style + custom theme | No Qt Widgets in the app layer |
| Rendering | Qt PDF (`QPdfDocument`) | Thumbnails and previews only |
| Manipulation | QPDF (Apache 2.0) | All structural writes go through QPDF |
| Image re-encode | `QImage` + Qt JPEG plugin | Lossy compression pass only |
| Build | CMake (>= 3.24) + vcpkg manifest mode | `vcpkg.json` at repo root |
| Tests | Catch2 v3 | Core library only |

**Licensing constraints (hard rules):**
- Never add MuPDF, Ghostscript, poppler, or any GPL/AGPL dependency.
- Qt is LGPL: link dynamically. Never static-link Qt.
- New dependencies must be MIT, BSD, Apache 2.0, ISC, Zlib, or OFL (fonts). Ask before adding any dependency.

## Layout

```
core/       libpdfcore - pure C++23, depends on QPDF only. NO Qt includes.
  ops/      one file per operation (Merge, Split, Extract, Rotate, Compress, Encrypt, Metadata)
app/        Qt Quick application
  models/   QAbstractListModel subclasses exposed to QML
  services/ ThumbnailProvider, JobRunner
  commands/ QUndoCommand subclasses
  qml/      theme/, components/, pages/
cli/        optional CLI over core
tests/      Catch2 tests + fixtures/
```

## Build and test

```sh
cmake --preset dev            # configure (vcpkg toolchain via CMakePresets.json)
cmake --build --preset dev    # build
ctest --preset dev            # run tests
```

Validate any PDF produced by a test or manual run with `qpdf --check <file>`.

## Architecture rules

1. **Core is UI-independent.** `core/` must compile and test without Qt. Anything the UI needs from core goes through plain C++ types.
2. **Non-destructive editing.** The UI edits a `Project` (ordered list of `PageRef{sourceDocId, pageIndex, rotation}`). Nothing touches disk until Export, which runs a `Job`.
3. **One page-copy path.** Merge, extract, and organize-export all build output from a list of `PageRef`s.
   - Single-source extraction: use `QPDFJob` with `pages()->pageSpec(...)`. It preserves annotations, links, and form fields.
   - Multi-source / reordered output: use `QPDFPageDocumentHelper::addPage`. Keep every source `QPDF` alive until `QPDFWriter::write()` returns.
4. **Never block the GUI thread.** All QPDF work and rendering runs via `JobRunner` (QThreadPool). Jobs report progress per page and check a `CancelToken` per page.
5. **Undo everything.** Every user-visible edit to a `Project` is a `QUndoCommand`.
6. **Page indices are 0-based internally, 1-based in the UI and in range strings.** Convert only at the boundary (`parsePageRange` and display code).
7. **Fail safe on unsupported PDF content.** In the lossy compression pass, skip images with unsupported color spaces (CMYK, ICC edge cases, Indexed), JBIG2, or complex masks rather than risk corruption. Only replace an image if the result is smaller.
8. **Errors:** core throws (QPDF throws `QPDFExc`); `JobRunner` catches at the job boundary and emits `failed(QString)`. Surface QPDF warnings to the user, do not swallow them.

## UI rules

- All colors, spacing, radii, type sizes, and durations come from the `Theme` singleton (`app/qml/theme/Theme.qml`). No hard-coded values in components or pages.
- Spacing on an 8px grid. Radii: 8 for cards, 6 for controls.
- Light and dark mode follow the system (`Qt.styleHints.colorScheme`) with a manual override in settings.
- Font: bundled Inter. Icons: Lucide SVGs in `app/qml/icons/`.
- Animations 150-200 ms via `Behavior`. Prefer toasts over modal dialogs for success; modals only for destructive or blocking decisions.
- Use native file dialogs (`QtQuick.Dialogs.FileDialog`).
- Large documents: thumbnail grid must stay virtualized (`GridView`), thumbnails render lazily at low DPI with an LRU cache.

## Code style

- C++23 (for `std::expected`). `clang-format` (config at repo root) before committing. `clang-tidy` clean for new code.
- Naming: `PascalCase` types, `camelCase` functions and variables, `m_` prefix for Qt class members, `kConstant` for constants.
- Headers use `#pragma once`. Prefer `std::expected` (or `tl::expected`) for parse/validation results; exceptions for I/O and QPDF failures.
- No raw `new`/`delete` outside Qt parent-ownership. Prefer `std::unique_ptr`.
- QML: one component per file, `required property` for inputs, no JavaScript business logic (keep it in C++ models).

## Testing

- Every operation in `core/ops/` has tests in `tests/` covering: normal case, page-range edge cases (first, last, single page, open-ended `8-`, duplicates, out-of-range), encrypted input, and a malformed fixture.
- Assertions check page count, page order (via a per-page marker in fixtures), and that `qpdf --check` passes on the output.
- Keep fixtures small (< 200 KB each). Generate new ones with a script in `tests/fixtures/gen/`, not by hand.

## Working in this repo

- **Inspect the actual source before writing code.** Read the relevant headers and existing implementations first. Do not assume an API, signature, or file exists.
- Verify QPDF and Qt APIs against the installed headers/versions, not memory. APIs change between major versions.
- Keep changes scoped: one feature or fix per change. Add or update tests with every core change.
- Do not edit generated files (`build/`, `vcpkg_installed/`).
- Ask before changing the dependency set, the build system layout, or public core interfaces.