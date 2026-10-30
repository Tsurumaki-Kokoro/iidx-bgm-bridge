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
| `hosts/iidx34.hpp` | 构建 hash、地址、状态映射和音频调用 |
| `profiles/` | BGM 资源映射和播放参数 |
| `tools/` | 本地导入、编解码、测量、部署和无音频打包 |
| `tests/` | 合成音频及播放/挂钩测试 |
| `vendor/` | 第三方源码、接口和许可 |

## Profile 格式 1

完整示例见现有 profile。安装位置为 `modules/bgm_profiles/<id>.ini`。

- `[profile]`：`schema=1`、合法 `id`、`host`、`resource_root`，可选 `fallback`。`schedule=weekday` 选择 sun–sat；`schedule=fixed` 使用 `fixed_variant`，默认 `default`。`{day}` 替换为选中目录名。ID 仅支持 ASCII 字母、数字、下划线和连字符。
- `[playback]`：`strategy` 为 `layered_sync`、`crossfade_fullmix` 或 `original`；`transition_ms` 为 100–2000；`layer_count` 为 1–3。
- `[levels]`：三轨策略需要 state0–state3，每行四个逗号分隔的有限数值，范围 0–127，未用声部必须为 0。槽位对应宿主适配器的默认 BGM 逻辑槽。SINOBUZ 将最终宿主状态映射为三轨，保持第 4 槽静音。
- `[map]`：左侧相对 `/data/sound/system/`；右侧相对 resource_root，或为该系统资源目录下的绝对虚拟路径。不允许目录穿越及 Windows 文件系统路径。映射数量为 1–128。

当前适配器的受控策略要求三个默认循环路径，采样率 44.1 kHz、零起点、相同帧数和整段循环边界。`original` 无需播放挂钩。相同三轨模型的编曲可以只新增 profile；分段衔接、更多声部或额外事件需要新策略，更换宿主构建需要验证地址及保护条件并增加适配器。

## 音频准备

导入工具先校验所有输入，在临时目录生成并检查全部映射，最后仅替换自己的资源目录，并保留上一套。原始声部保留压缩音频 payload。源文件头部增益转换为 24 代有效权重，与宿主原始选曲 BGM 校准，并通过共同增益限制峰值。头部量化可能导致响度不能完全匹配，结果记录在本地 manifest。

完整混音由校准后的声部生成，使用 Windows ACM MS ADPCM 编码。尾部不足整块时补入循环开头采样，保留原始 fact/循环长度，再检查回读 SNR 和采样峰值。有符号 SD9 增益补偿原始调度器的目标音量。新生成的兼容资源替代了对历史 v4 预制资源的依赖，并非与其逐字节一致。选定音乐音效继承宿主头部增益。输入音频不提交或上传。

## CI 与发布

`.github/workflows/build.yml` 在 main 提交、PR 和手动触发时构建测试。PyInstaller 将导入程序打包为独立 EXE，终端用户无需 Python。打包使用明确的文件列表，包含依赖许可，生成 ZIP 和 SHA-256，并检查解压后的程序。Actions 运行页面可下载二进制产物。

推送 `v0.1.0` 等版本标签后执行同样的检查，仅在构建成功后创建 GitHub Release。只有发布任务具有 `contents: write`，其他任务使用只读权限。第三方 Actions 固定到提交 hash。CI 没有游戏资源，也没有音频凭据。确定发布标签前，应完成本地集成测试和实机听感检查。已发布版本不要重新打同名标签，应发布新版本。

## 贡献

同步维护中英文文档。宿主地址不应进入通用播放核心。新增调度行为应使用生成数据补充测试。不要提交游戏程序、音频、个人配置、录制文件或提取的逆向研究工作目录。遵循项目 GPL-3.0-only 许可，并保留依赖声明。
