# IIDX BGM Bridge

[简体中文](README.zh-CN.md) · [Downloads](https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge/releases) · [Developer guide](docs/development.md)

A Windows x64 Spice SDK plugin that brings profile-based system BGM to IIDX 33 and 34. The included SINOBUZ profile selects one of seven daily arrangements and keeps its three stems synchronized across menu transitions.

**No game executables or audio are included.** Generate runtime audio locally from your own IIDX 18–24 or 33 audio and a supported IIDX 33/34 installation. The importer does not download game data.

## Features

- IIDX 18–23 audio, including both PENDUAL PRESENT and FUTURE arrangements.
- Seven daily SINOBUZ arrangements, or a fixed weekday selection.
- Sparkle Shower audio with its original selection intro and loop boundaries.
- Continuous three-stem playback with configurable stage levels and fades.
- Complete-mix compatibility profile for the original host scheduler.
- Daily music-confirmation and card-out sounds, plus full mixes for alternate selection modes.
- Separate resource directory; original game audio files remain intact.
- Profile-driven resource mapping and a dedicated host adapter for future extensions.

## Compatibility

| Component | Requirement |
| --- | --- |
| OS | Windows x64 |
| Host | Supported IIDX 33/34 builds listed below |
| Loader | Spice2x with Spice SDK plugin support and the `-k` DLL option |
| Audio input | Local IIDX 18–24 or 33 resources (see below), plus the target installation |
| Python | Not required for the release ZIP; required when building from source |

### Supported DLL game versions

**beatmania IIDX 34 ZINRAI**

- `2026091700, 2026091600`

**beatmania IIDX 33 Sparkle Shower**

- `2026081900`

### Supported audio resource versions

**beatmania IIDX 18 Resort Anthem**

- `2011071200`

**beatmania IIDX 19 Lincle**

- `2012062500`

**beatmania IIDX 20 tricoro**

- `2013090900`

**beatmania IIDX 21 SPADA**

- `2014071600`

**beatmania IIDX 22 PENDUAL**

- `2015080500`

**beatmania IIDX 23 copula**

- `2016083100`

**beatmania IIDX 24 SINOBUZ**

- `2017082800`

**beatmania IIDX 33 Sparkle Shower**

- `2026081900`

The resource version identifies the tested source installation. The importer validates the required files and audio layout rather than enforcing that version string. Other game builds and resource revisions are not currently listed as supported.

Host selection uses the ten-digit game version, not a whole-DLL hash. It reads `prop/bootstrap.xml` release_code first, then runtime AVS ext if available, then `prop/ea3-config.xml` ext. This accommodates patched DLLs with the same supported audio layout. The plugin still validates audio entry points and call relationships; changing the version label does not make an incompatible executable supported.

Other game versions are rejected. Custom BGM and forced special states continue through the host's original logic. Offline tests cover playback, detours and resource generation; these do not establish compatibility with every game mode or replace listening tests in game. The transition follows the three-stem arrangement while retaining the host's volume updates; it is not an exact emulation of every IIDX 24 frame.

## Install

1. Close the game. Download and extract the Windows ZIP from [Releases](https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge/releases), then copy its contents into the target IIDX 33/34 root directory. Do not use GitHub's source-code ZIP as the binary package.
2. Copy `modules/bgm_bridge.example.ini` to `modules/bgm_bridge.ini`. Updates ship an example file so your existing configuration is not overwritten.
3. Run `tools/import-resources.exe`. Choose an audio profile, then enter the source path and target game root. For Sparkle Shower use IIDX 33’s `data/sound/system` directory. Importing may take a while. The selected resource set is fully prepared and validated before installation.
4. Add `-k bgm_bridge.dll` to your existing Spice2x launch command, preserving its other options. For example, append that argument to the command you already use. The plugin DLL belongs in `modules/`.
5. Start the game. Check `bgm_bridge.log` in the game root if the profile is not active.

For unattended local importing:

```bat
tools\import-resources.exe --source-root "D:\IIDX24\data\sound\system\0" --game-root "D:\IIDX34"
```

To import Sparkle Shower audio for either supported host:

```bat
tools\import-resources.exe --profile sparkle33 --source-root "D:\IIDX33\data\sound\system" --game-root "D:\IIDX34"
```

Then set `profile=sparkle33` and `fallback_profile=sparkle33` in the main INI. Importing generates resources without changing the selected profile.

Generated files are placed under `data/sound/system/bgm_bridge/<profile>/`. Reimporting preserves the previous set in a sibling `<profile>.backup-*` directory. Use `--output-root` to generate a separate tree for inspection instead of installing the resources into the game directory. Close the game before importing.

## IIDX 18–23 audio

| Profile | Audio | Playback |
| --- | --- | --- |
| `resort18` | IIDX 18 Resort Anthem | SSP / independent stage tracks |
| `lincle19` | IIDX 19 Lincle | SSP / independent stage tracks |
| `tricoro20` | IIDX 20 tricoro | Synchronized stems |
| `spada21` | IIDX 21 SPADA | Synchronized stems |
| `pendual22` | IIDX 22 PENDUAL PRESENT | Synchronized stems |
| `pendual22_future` | IIDX 22 PENDUAL FUTURE | Synchronized stems |
| `copula23` | IIDX 23 copula | Synchronized stems |

For 18/19, select the source game root, its `sys_sd` directory, or `sys_sound.ssp`. The importer reads the original SSP directly; no external extraction tool is required. For 20–23, select the source game root or `data/sound/system`.

```bat
tools\import-resources.exe --profile tricoro20 --source-root "D:\IIDX20" --game-root "D:\IIDX34"
```

Set `profile=tricoro20` and `fallback_profile=tricoro20_compat` in the main INI. For other 20–23 profiles, use the matching `<profile>_compat` fallback. For 18/19, set both profile fields to the same ID (for example `resort18`) and keep `smooth_transition=1`: independent stage tracks require the playback controller. If that controller cannot initialize, the plugin leaves the game's original audio active rather than overlapping unrelated tracks.

18/19 retain their original intros and loop boundaries and crossfade between independent entry, mode and selection tracks. Their music-confirmation sound is imported; the host's card-out sound is retained. 20–23 keep synchronized source stems for default menus and render complete mixes for standalone selection modes. Only PENDUAL's explicit FUTURE profile uses the `_02` resources; inherited files from older games are not selected as copula arrangements. These additions provide audio profiles, not support for running IIDX 18–23 as hosts.

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
| `profile` | `sinobuz24` for synchronized stems; `sinobuz24_compat` for complete mixes; `sparkle33` for Sparkle Shower; see above for 18–23 |
| `fallback_profile` | Profile selected if the requested profile or its assets cannot load |
| `weekday` | `auto`, `sun`, `mon`, `tue`, `wed`, `thu`, `fri`, `sat` |
| `smooth_transition` | `1` enables the selected controller; `0` selects fallback assets with original host scheduling |

`sparkle33` preserves both source variants, selection intro and loop boundaries, using the original host scheduler rather than the SINOBUZ fade. Both audio families work with either supported host.

The weekday is selected using the computer's local date and remains fixed until restart. Fade duration belongs to each profile's `[playback] transition_ms`, not the main INI. The default layered transition is 350 ms; the compatibility crossfade is 600 ms. Restart after configuration changes.

A missing profile or asset triggers fallback. If controller validation fails, the plugin uses compatibility resources with original scheduling. If initialization cannot complete, installed hooks are rolled back. An unsupported game version or incompatible audio-function layout leaves the plugin inactive.

## Upgrade and removal

For updates, close the game, replace the DLL/tools/profiles, and keep your existing `bgm_bridge.ini`. Rerun the importer when resource generation or profile paths change. Keep the previous package if you need a rollback.

To uninstall, remove the launch argument, then remove `modules/bgm_bridge.dll`, its INI/example, the included profiles and the generated profile directories under `data/sound/system/bgm_bridge/`. Remove importer tools only if no other installation needs them. Original game audio is not modified by this project.

## Troubleshooting

- **Original BGM still plays:** inspect `bgm_bridge.log`, confirm the `-k` argument and supported game version, and check that resource import completed.
- **Importer cannot find a file:** for SINOBUZ choose IIDX 24's `data/sound/system/0`, with all seven days present. For Sparkle Shower choose IIDX 33's `data/sound/system`, including both variants. The target game's original system files are also required.
- **Audio is too quiet or a transition sounds wrong:** report the weekday, profile, host version, mode and a recording if possible. Include the generated `manifest.json` calibration values and relevant log lines; do not upload game audio files to an issue.
- **Need the old scheduling behavior:** set `smooth_transition=0` and restart.

## Development and releases

See the [English developer guide](docs/development.md) or [中文开发指南](docs/development.zh-CN.md) for builds, tests, profile schema, resource calibration and release automation. Pull requests should update both language versions when changing documented behavior. Game files, local configuration, generated audio, logs and research workspaces are excluded from version control and release packaging.

## License

Project code is distributed under [GPL-3.0-only](LICENSE). See [third-party notices](THIRD_PARTY_NOTICES.md) and the licenses bundled with releases. Game data remains separately supplied and is not covered by this project's license. This is an independent project, not affiliated with or endorsed by KONAMI.
