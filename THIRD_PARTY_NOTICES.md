# Third-party notices / 第三方许可说明

IIDX BGM Bridge project code is licensed under GPL-3.0-only; see LICENSE. 游戏数据不包含在本项目中，其权利不因项目许可而改变。

- **MinHook** — BSD-style license; source and complete notice in `vendor/minhook/LICENSE.txt`.
- **Spice2x SDK interface** — `include/spicesdk.h` was copied from the locally supplied Spice2x SDK. The upstream GPL-3.0 license is preserved in `vendor/spicesdk/LICENSE`.
- **Release resource importer** — built using Python (PSF license), NumPy (BSD-3-Clause, including its wheel dependency notices), and PyInstaller (GPL with its bootloader exception). Their notices and packaging dependency licenses are collected in the release `licenses/` directory. See those complete texts for terms.
- **Windows ACM** — the codec tool calls the operating system's installed MS ADPCM codec; that system codec is not bundled.

Complete corresponding project source is available at https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge under the release tag. Original game files and audio are separately supplied local inputs and are excluded from both source and binary distributions.
