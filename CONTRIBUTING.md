Contributing to LVK Sprite Animator
====================================

Thanks for the interest. This document describes the conventions used by
the 2026 modernization fork. The code style, branch policy, and test
policy below all match what the CI matrix enforces, so if your patch
follows them it should be green on first push.

## Code style

We use **`.clang-format`** at the repo root (LLVM base with project-local
tweaks) as the single source of truth. Run it before sending a patch:

```sh
clang-format -i src/<files-you-touched>.cpp src/<files-you-touched>.h
```

We also run **`.clang-tidy`** in CI with the `modernize-*`,
`readability-*`, and `bugprone-*` check families enabled. Local invocation:

```sh
clang-tidy -p build src/yourfile.cpp
```

Editor settings live in `.editorconfig` (4-space indent, LF line endings,
trim trailing whitespace, final newline). Most editors pick this up
automatically.

Additional conventions, not all of which `clang-format`/`clang-tidy` can
enforce:

- **C++17.** `std::optional`, `if constexpr`, structured bindings are all
  fair game. We do **not** use C++20 features yet -- the CI matrix
  includes a Qt 6.4 build that may run on older compilers.
- **`override` on every virtual.** The Phase-2 sweep added these
  everywhere; please keep new overrides annotated.
- **Smart pointers over raw `new`/`delete`** except where Qt's parent
  ownership tree already manages the lifetime (e.g. `new QLabel(this)`).
- **Qt PMF connect syntax** (`connect(obj, &Class::signal, ...)`) instead
  of the legacy `SIGNAL()`/`SLOT()` string macros. Use `QOverload<...>::of`
  to disambiguate overloaded signals.
- `tr()` every user-facing string, even though we ship no translations
  today -- this keeps the door open for a `.ts` workflow later.

## Test policy

**Every bug fix MUST land with a regression test.** This was the single
biggest lesson of the 10-agent upgrade: latent UB sat in
`SpriteState::addAframe` and `SpriteState::aframes(Id)` for over a decade
because there were no tests pinning the data classes. We now require:

- Bug-fix patches: at least one Qt Test case that **fails** on the
  pre-fix code and **passes** on the patched code. The test belongs under
  `tests/unit/` (data classes), `tests/format/` (file-format round-trips),
  or `tests/security/` (sandbox / input-validation cases).
- New feature patches: a happy-path test plus at least one error-path
  test. For new exporters add a golden-file round-trip if the output
  format is supposed to be byte-stable.
- UI-only patches do not require a test; CI smoke-tests the GUI by
  loading `examples/mario.lvks` and exporting it.

Tests live under `tests/`, wired into the CMake build by
`tests/CMakeLists.txt`. The supported subdirectories are:

| Directory          | Purpose                                              |
| ------------------ | ---------------------------------------------------- |
| `tests/unit/`      | Data-class CRUD, undo, `fromString`/`toString` round-trips. |
| `tests/format/`    | Golden-file round-trips, version-header parsing.     |
| `tests/security/`  | Sandbox / shell-injection / file-validation tests.   |

Running locally:

```sh
cmake --preset=default
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

For tests that need an actual `QApplication` (most do, because they touch
`QPixmap`), set `QT_QPA_PLATFORM=offscreen` -- CI does this for you.

For memory work, the `asan` preset enables AddressSanitizer + UBSan:

```sh
cmake --preset=asan
cmake --build build -j$(nproc)
ASAN_OPTIONS=detect_leaks=1 QT_QPA_PLATFORM=offscreen \
    ctest --test-dir build --output-on-failure
```

## Branch policy

- **`master`** -- legacy Qt 4 line. Read-only; receives no new commits.
- **`upgrade-sprite-animator-O2rKz`** -- the Qt 6 modernization
  trunk. All new work merges here.
- Feature branches: short topic name (e.g. `fix-export-encoding`).

We expect PRs to be:

1. **Linear history.** Rebase, don't merge, before submitting.
2. **Green CI** on Linux, macOS, and Windows. The matrix is in
   `.github/workflows/`.
3. **Reviewed by at least one other contributor** if non-trivial.

Commit message style: short imperative subject (<= 72 chars), blank
line, free-form body. If your change resolves an `UPGRADE_NOTES.md`
item, reference the item number.

## Reporting bugs

Open an issue with:

- A minimal `.lvks` file (or a description of how to generate one) that
  reproduces the problem.
- The exact CLI invocation and the build preset (`default` / `debug` /
  `asan`).
- Qt and CMake versions (`./build/LvkSpriteEditor --version`,
  `cmake --version`).

Security-sensitive issues: please **do not** open a public issue. Email
`contact@lvklabs.com` instead (or, for the modernization fork, use the
GitHub Security Advisory workflow on the upgrade repo).
