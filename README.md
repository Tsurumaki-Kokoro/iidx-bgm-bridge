# IIDX BGM Bridge

[简体中文](README.zh-CN.md) · [Downloads](https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge/releases) · [Developer guide](docs/development.md)

A Windows x64 Spice SDK plugin that brings profile-based system BGM to IIDX 34. The included SINOBUZ profile selects one of seven daily arrangements and keeps its three stems synchronized across menu transitions.

**No game executables or audio are included.** Generate runtime audio locally from your own IIDX 24 and supported IIDX 34 files. The importer does not download game data.

## Features

- Seven daily SINOBUZ arrangements, or a fixed weekday selection.
- Continuous three-stem playback with configurable stage levels and fades.
- Complete-mix compatibility profile for the original host scheduler.
- Daily music-confirmation and card-out sounds, plus full mixes for alternate selection modes.
- Separate resource directory; original game audio files remain intact.
- Profile-driven resource mapping and a dedicated host adapter for future extensions.

## Compatibility

| Component | Requirement |
| --- | --- |
| OS | Windows x64 |
| Host | IIDX 34 build identified as `iidx34-20260916` |
| Loader | Spice2x with Spice SDK plugin support and the `-k` DLL option |
| Audio input | Local IIDX 24 SINOBUZ `data/sound/system/0` directory and the supported IIDX 34 installation |
| Python | Not required for the release ZIP; required when building from source |

Supported `modules/bm2dx.dll` SHA-256:

```text
3755587f44785effe5008f74cfe7dccbdf423ad09feead48e00a90f78a42fe83
```

Other executable revisions are rejected. Custom BGM and forced special states continue through the host's original logic. Offline tests cover playback, detours and resource generation; these do not establish compatibility with every game mode or replace listening tests in game. The transition follows the three-stem arrangement while retaining IIDX 34's volume updates; it is not an exact emulation of every IIDX 24 frame.

## Install

1. Close the game. Download and extract the Windows ZIP from [Releases](https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge/releases), then copy its contents into the IIDX 34 root directory. Do not use GitHub's source-code ZIP as the binary package.
2. Copy `modules/bgm_bridge.example.ini` to `modules/bgm_bridge.ini`. Updates ship an example file so your existing configuration is not overwritten.
3. Run `tools/import-resources.exe`. Enter your IIDX 24 `data/sound/system/0` path and IIDX 34 root path when prompted. Importing may take a while. All seven days are prepared and validated before the resource set is installed.
4. Add `-k bgm_bridge.dll` to your existing Spice2x launch command, preserving its other options. For example, append that argument to the command you already use. The plugin DLL belongs in `modules/`.
5. Start the game. Check `bgm_bridge.log` in the game root if the profile is not active.

For unattended local importing:

```bat
tools\import-resources.exe --source-root "D:\IIDX24\data\sound\system\0" --game-root "D:\IIDX34"
```

The generated files are placed under `data/sound/system/bgm_bridge/sinobuz24/`. Reimporting preserves the previous set in a sibling `sinobuz24.backup-*` directory. Use `--output-root` to generate a separate tree for inspection instead of installing the resources into the game directory. Close the game before importing.

## Configuration

Edit `modules/bgm_bridge.ini`:

```ini
[bgm_bridge]
enabled=1
profile=sinobuz24
fallback_profile=sinobuz24_compat
weekday=auto
smooth_transition=1
```

| Setting | Values / behavior |
| --- | --- |
| `enabled` | `0` disables the plugin; `1` enables it |
| `profile` | `sinobuz24` for synchronized stems; `sinobuz24_compat` for complete mixes |
| `fallback_profile` | Profile selected if the requested profile or its assets cannot load |
| `weekday` | `auto`, `sun`, `mon`, `tue`, `wed`, `thu`, `fri`, `sat` |
| `smooth_transition` | `1` enables the selected controller; `0` selects fallback assets with original host scheduling |

The weekday is selected using the computer's local date and remains fixed until restart. Fade duration belongs to each profile's `[playback] transition_ms`, not the main INI. The default layered transition is 350 ms; the compatibility crossfade is 600 ms. Restart after configuration changes.

A missing profile or asset triggers fallback. If controller validation fails, the plugin uses compatibility resources with original scheduling. If initialization cannot complete, installed hooks are rolled back. An unsupported executable hash leaves the plugin inactive.

## Upgrade and removal

For updates, close the game, replace the DLL/tools/profiles, and keep your existing `bgm_bridge.ini`. Rerun the importer when resource generation or profile paths change. Keep the previous package if you need a rollback.

To uninstall, remove the launch argument, then remove `modules/bgm_bridge.dll`, its INI/example, the two included profiles and the generated `data/sound/system/bgm_bridge/sinobuz24` directory. Remove importer tools only if no other installation needs them. Original game audio is not modified by this project.

## Troubleshooting

- **Original BGM still plays:** inspect `bgm_bridge.log`, confirm the `-k` argument and supported executable hash, and check that resource import completed.
- **Importer cannot find a file:** choose IIDX 24's `data/sound/system/0`, not its game root. All seven sets of stems, card-out sounds and music-confirmation sounds are required. IIDX 34's original system files are also required.
- **Audio is too quiet or a transition sounds wrong:** report the weekday, profile, host hash, mode and a recording if possible. Include the generated `manifest.json` calibration values and relevant log lines; do not upload game audio files to an issue.
- **Need the old scheduling behavior:** set `smooth_transition=0` and restart.

## Development and releases

See the [English developer guide](docs/development.md) or [中文开发指南](docs/development.zh-CN.md) for builds, tests, profile schema, resource calibration and release automation. Pull requests should update both language versions when changing documented behavior. Game files, local configuration, generated audio, logs and research workspaces are excluded from version control and release packaging.

## License

Project code is distributed under [GPL-3.0-only](LICENSE). See [third-party notices](THIRD_PARTY_NOTICES.md) and the licenses bundled with releases. Game data remains separately supplied and is not covered by this project's license. This is an independent project, not affiliated with or endorsed by KONAMI.
