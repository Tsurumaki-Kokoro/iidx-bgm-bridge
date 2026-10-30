# Development guide

[简体中文](development.zh-CN.md) · [User guide](../README.md)

## Build and test

Use Windows x64 with Visual Studio 2022's C++ desktop tools and a Windows SDK. The repository vendors MinHook and the Spice SDK interface; no game binary is needed to compile or run CI tests.

```bat
build.cmd
python -m pip install -r tools/requirements.txt
python -m unittest discover -s tests -p "test_*.py" -v
python tools/package.py --version dev-local
python tests/check_package.py
```

`build.cmd` discovers Visual Studio through vswhere. Set `VCVARS64` to override its environment script. Outputs are in `build/`; the plugin is `bgm_bridge.dll`. Python 3.12 x64 is used for release builds. Pinned primary packaging dependencies are in `tools/requirements.txt`.

CMake is also supported:

```bat
cmake -S . -B build/cmake -A x64
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

CMake binaries are in `build/cmake/`. The Python tools currently expect binaries in `build/`; use `build.cmd` for resource import and release packaging.

The C++ suite tests profiles, validation, stage gains, phase synchronization, bypasses, compatibility fades, and a real MinHook detour/restore. Python tests generate a tone to verify partial ADPCM block encoding and check profile containment, signed SD9 gain and invalid inputs. Package checks verify layout, audio exclusion, SHA-256 and an extracted importer's startup. These tests do not require game data.

For a private integration test, use your own local files:

```bat
python tools/import_resources.py --source-root "D:\IIDX24\data\sound\system\0" --game-root "D:\IIDX34" --output-root "build\private-assets"
python tools/deploy.py --game-root "D:\IIDX34" --asset-root "build\private-assets" --validate-only
```

`tools/deploy.py --game-root "D:\IIDX34"` validates existing generated resources, backs up the plugin/configuration, and installs the source build and profiles. It preserves existing configuration values. It does not modify launcher arguments. Close the game first.

## Layout

| Path | Responsibility |
| --- | --- |
| `src/plugin.cpp` | SDK lifecycle, INI loading, resource redirection and fallback |
| `include/playback.hpp` | Host-independent synchronization and level transitions |
| `include/profile.hpp` | Profile schema and validation |
| `hosts/iidx34.hpp` | Build hash, addresses, state translation and audio calls |
| `profiles/` | BGM resource routes and playback parameters |
| `tools/` | Local importing, codec, metering, deployment and audio-free packaging |
| `tests/` | Synthetic audio tests and playback/detour tests |
| `vendor/` | Dependency source/interface and licenses |

## Profile schema 1

Use an existing profile as the complete example. Profiles are installed in `modules/bgm_profiles/<id>.ini`.

- `[profile]`: `schema=1`, safe `id`, `host`, `resource_root`; optional `fallback`. `schedule=weekday` selects sun–sat. `schedule=fixed` uses `fixed_variant` (default `default`). `{day}` expands to the selected variant. IDs accept ASCII letters, digits, `_` and `-`.
- `[playback]`: `strategy` is `layered_sync`, `crossfade_fullmix`, or `original`. `transition_ms` accepts 100–2000. `layer_count` accepts 1–3.
- `[levels]`: layered profiles require `state0` through `state3`, each with four comma-separated finite values in 0–127. Unused layers must be zero. Slots correspond to the host adapter's logical default BGM slots. The SINOBUZ profile maps the host's final state to three stems, keeping slot 3 muted.
- `[map]`: keys are paths relative to `/data/sound/system/`. Values are relative to `resource_root` or absolute virtual paths under `/data/sound/system/`. Traversal and Windows filesystem paths are rejected. There must be 1–128 mappings.

The current adapter requires three default-loop routes for controlled strategies. It accepts 44.1 kHz loops starting at zero, with matching frame counts and full-length boundaries. `original` does not require the playback hooks. Additional arrangements using the same three-stem model can use new profiles. Segmented transitions, more stems or additional events require a new strategy; a different host build requires an adapter with verified addresses and guards.

## Audio preparation

The importer validates all inputs, prepares a temporary output tree, checks every route and then replaces only its own resource directory, retaining the previous set. Native stems retain their compressed payload. Source header gains are converted to the effective IIDX 24 weights, calibrated against the host's original selection BGM, and constrained by a common peak ceiling. Header quantization can reduce the match; calibration values are recorded in the local manifest.

Complete mixes are rendered from the calibrated stems and encoded with Windows ACM MS ADPCM. Partial final blocks are padded with loop-start samples while original fact/loop lengths are retained. The importer validates decode SNR and sample peak. Signed SD9 gain compensates the original scheduler's target level. This regenerated compatibility set replaces the historical dependency on prebuilt v4 assets; it is not byte-identical to that set. Confirmation sounds inherit the host's header gain. No input audio is committed or uploaded.

## CI and releases

`.github/workflows/build.yml` builds and tests on main pushes, pull requests and manual dispatches. It bundles the importer with PyInstaller so end users need no Python installation. Packaging uses an explicit file list, includes dependency notices, produces a ZIP plus SHA-256 file, and checks the extracted executable. Build outputs remain temporary to the runner; no Actions artifacts are uploaded. Downloadable packages are published only through Releases.

Pushing a version tag such as `v0.1.0` runs the same checks and publishes a GitHub Release only after build success. The release job rebuilds and packages locally, then uploads the ZIP and checksum directly to the Release without artifact transfer. The release job alone receives `contents: write`; other jobs use read access. Third-party actions are pinned to commit hashes. CI has no game assets and no audio credentials. Run the local integration test and game listening checks before choosing a release tag. Avoid retagging an existing release; publish a new version instead.

## Contributions

Keep English and Chinese documents aligned. Keep host-specific addresses out of the generic playback core. Add tests for new scheduling behavior using generated fixtures. Do not submit game binaries, audio, personal configurations, recordings or extracted reverse-engineering workspaces. Follow the project GPL-3.0-only license and preserve dependency notices.
