# 开发指南

[English](development.md) · [用户文档](../README.zh-CN.md)

## 构建与测试

使用 Windows x64、Visual Studio 2022 C++ 桌面开发工具和 Windows SDK。仓库自带 MinHook 和 Spice SDK 接口，编译及 CI 测试无需游戏程序。

```bat
build.cmd
python -m pip install -r tools/requirements.txt
python -m unittest discover -s tests -p "test_*.py" -v
python tools/package.py --version dev-local
python tests/check_package.py
```

`build.cmd` 通过 vswhere 查找 Visual Studio，可用 `VCVARS64` 指定环境脚本。产物位于 `build/`，插件为 `bgm_bridge.dll`。发布构建使用 Python 3.12 x64，主要打包依赖固定在 `tools/requirements.txt`。

也支持 CMake：

```bat
cmake -S . -B build/cmake -A x64
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

CMake 产物位于 `build/cmake/`；Python 工具目前从 `build/` 查找二进制，资源导入和发布打包请使用 `build.cmd`。

C++ 测试覆盖 profile、校验、阶段音量、相位同步、旁路、兼容渐变和真实 MinHook 安装/还原。Python 测试使用生成的正弦波，验证 ADPCM 尾块编码，同时检查资源路径、SD9 有符号增益及非法输入。压缩包检查覆盖目录、音频排除、SHA-256 和解压后导入程序的启动，均不需要游戏文件。

使用自己的本地文件进行私有集成测试：

```bat
python tools/import_resources.py --source-root "D:\IIDX24\data\sound\system\0" --game-root "D:\IIDX34" --output-root "build\private-assets"
python tools/deploy.py --game-root "D:\IIDX34" --asset-root "build\private-assets" --validate-only
```

`tools/deploy.py --game-root "D:\IIDX34"` 会校验已有生成资源，备份插件及配置，再安装源码构建和 profiles，保留现有配置值，不修改启动参数。部署前关闭游戏。

## 目录结构

| 路径 | 职责 |
| --- | --- |
| `src/plugin.cpp` | SDK 生命周期、INI、资源重定向和回退 |
| `include/playback.hpp` | 与宿主无关的同步及音量渐变 |
| `include/profile.hpp` | Profile 格式和校验 |
| `hosts/adapter.hpp + hosts/builds.hpp` | 构建 hash、地址、状态映射和音频调用 |
| `profiles/` | BGM 资源映射和播放参数 |
| `tools/` | 本地导入、编解码、测量、部署和无音频打包 |
| `tests/` | 合成音频及播放/挂钩测试 |
| `vendor/` | 第三方源码、接口和许可 |

## Profile 格式 1

完整示例见现有 profile。安装位置为 `modules/bgm_profiles/<id>.ini`。

- `[profile]`：`schema=1`、合法 `id`、`host`（逗号分隔的宿主系列或构建 ID）、`resource_root`，可选 `fallback`。`schedule=weekday` 选择 sun–sat；`schedule=fixed` 使用 `fixed_variant`，默认 `default`。`{day}` 替换为选中目录名。ID 仅支持 ASCII 字母、数字、下划线和连字符。
- `[playback]`：`strategy` 为 `layered_sync`、`crossfade_fullmix`、`staged_tracks` 或 `original`；`transition_ms` 为 100–2000；`layer_count` 为 1–3。
- `[levels]`：三轨及独立阶段策略需要 state0–state3，每行四个逗号分隔的有限数值，范围 0–127，未用声部必须为 0。槽位对应宿主适配器的默认 BGM 逻辑槽。SINOBUZ 将最终宿主状态映射为三轨，保持第 4 槽静音。
- `[map]`：左侧相对 `/data/sound/system/`；右侧相对 resource_root，或为该系统资源目录下的绝对虚拟路径。不允许目录穿越及 Windows 文件系统路径。映射数量为 1–128。

IIDX 33/34 共用适配器按程序 SHA-256 选择编译时地址表，受控策略要求三个默认循环路径，同步/混音渐变策略要求采样率 44.1 kHz、零起点、相同帧数和整段循环边界；独立阶段策略保留不同长度和非零循环起点。`original` 无需播放挂钩。相同三轨模型的编曲可以只新增 profile；分段衔接、更多声部或额外事件需要新策略，更换宿主构建需要验证地址及保护条件并增加适配器。

## 音频准备

导入工具先校验所有输入，在临时目录生成并检查全部映射，最后仅替换自己的资源目录，并保留上一套。原始声部保留压缩音频 payload。源文件头部增益转换为 24 代有效权重，与宿主原始选曲 BGM 校准，并通过共同增益限制峰值。头部量化可能导致响度不能完全匹配，结果记录在本地 manifest。

完整混音由校准后的声部生成，使用 Windows ACM MS ADPCM 编码。尾部不足整块时补入循环开头采样，保留原始 fact/循环长度，再检查回读 SNR 和采样峰值。有符号 SD9 增益补偿原始调度器的目标音量。新生成的兼容资源替代了对历史 v4 预制资源的依赖，并非与其逐字节一致。选定音乐音效继承宿主头部增益。输入音频不提交或上传。

## CI 与发布

`.github/workflows/build.yml` 在 main 提交、PR 和手动触发时构建测试。PyInstaller 将导入程序打包为独立 EXE，终端用户无需 Python。打包使用明确的文件列表，包含依赖许可，生成 ZIP 和 SHA-256，并检查解压后的程序。构建输出仅临时保留在 runner，不上传 Actions Artifact。可下载的压缩包仅通过 Releases 发布。

推送 `v0.1.0` 等版本标签后执行同样的检查，仅在构建成功后创建 GitHub Release。发布任务会重新构建并在本机打包，直接将 ZIP 和校验文件上传至 Release，不通过 Artifact 中转。只有发布任务具有 `contents: write`，其他任务使用只读权限。第三方 Actions 固定到提交 hash。CI 没有游戏资源，也没有音频凭据。确定发布标签前，应完成本地集成测试和实机听感检查。已发布版本不要重新打同名标签，应发布新版本。

## 贡献

同步维护中英文文档。宿主地址不应进入通用播放核心。新增调度行为应使用生成数据补充测试。不要提交游戏程序、音频、个人配置、录制文件或提取的逆向研究工作目录。遵循项目 GPL-3.0-only 许可，并保留依赖声明。

## Sparkle Shower 与宿主验证

`sparkle33` 使用 fixed/original 策略，在 IIDX 33 和 34 上保留两套原始音频变体，以及 l2 前奏和非零循环起点。导入时仅替换目标的循环次数、声像与声音 ID，保留来源增益、循环区间及压缩数据，不做三轨重混。指定 `--profile sparkle33 --source-root "D:\IIDX33\data\sound\system"` 生成资源，校验/部署也可使用 `--profile sparkle33`。无需同时导入 SINOBUZ 资源。

`hosts/builds.hpp` 保存宿主地址和 hash，`tools/host_builds.json` 为工具提供相同构建列表；测试检查两者一致。33 代当前验证安装的 release code 为 2026081900。支持以列出的精确 hash 为准，修改属性文件版本号不会让其他 DLL 获得支持。

`build/host-image-tests.exe` 是可选私有测试工具，参数为宿主系列、游戏 DLL 和三个已生成 SINOBUZ 声部路径。它使用 DONT_RESOLVE_DLL_REFERENCES 映射 DLL，实际安装并还原播放挂钩，不解析游戏依赖、不执行入口或游戏函数。仅对已核对 hash 的本地 DLL 运行。不能替代游戏内测试。

## IIDX 18–23 音频适配

`tools/legacy_audio.py` 管理来源规则，宿主 hash 列表仍只有 33/34。18/19 SSP 表头为 72 字节，后接 250 组、每组 80 个 uint32 偏移。读取时检查表头、索引、条目边界、SD9 格式及声音 ID。默认选择组 14 子项 1，模式选择为子项 0；卡片进入为 18 代子项 11、19 代子项 8；确认音效为组 20 子项 0。这些索引来自对应程序的名称表核对，不依赖音频内容扫描猜测。

18/19 的 `staged_tracks` 将 entry、mode、selection 作为独立曲目渐变，选曲从头播放，不做跨曲目相位同步；不要求三轨等长。保留源循环区间及压缩数据，按宿主参考响度校准阶段增益，检查采样峰值。该策略不允许无控制器的原生调度回退；不可用时保留宿主音频。旧来源没有单独退出音效映射，因此该音效不替换。

20–23 使用与 SINOBUZ 相同的同步三轨模型，但日程为 fixed。主 profile 和对应 compat profile 共用独立生成资源，所有原始声部 payload 保留。PENDUAL PRESENT/FUTURE 分别使用默认文件和 `_02` 文件；其他代目不自动采用继承文件。完整混音采用标准立体声 MS ADPCM；必要时缩短编码块至 128/64 字节，要求回读 SNR >20 dB、峰值通过检查。实际编码参数和校准值写入本地 manifest。源码与发布包均不含音频。

私有测试可逐个调用导入工具，再用 `tools/deploy.py --profile <id> --validate-only --asset-root <输出根目录> --game-root <目标游戏>` 验证；仅检查选中 profile 和所需回退资源。`host-image-tests.exe` 最后可附加 profile INI 路径，验证独立阶段策略的挂钩安装/还原。CI 使用生成数据验证 SSP 边界、ADPCM 尾块/短块及阶段切换，不需要旧游戏文件。
