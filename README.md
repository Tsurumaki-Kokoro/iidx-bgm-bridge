# IIDX 34 BGM profiles

附加 DLL 形式的 BGM 插件。当前支持本机验证过的 IIDX 34 20260916 构建，主程序 SHA256 在 `include/host_iidx34.hpp` 中固定校验。通过现有 Spice SDK 加载机制使用，不修改原始游戏文件。

## 使用

安装后重启游戏即可。`modules/daily24.ini`：

```ini
[daily24]
enabled=1
weekday=auto
smooth_transition=1
profile=sinobuz24
fallback_profile=sinobuz24_compat
```

`weekday` 可为 auto 或 sun/mon/tue/wed/thu/fri/sat，每次启动固定一次。配置示例在 `config/daily24.example.ini`。

- `sinobuz24`：24 代七套原始三轨，模式选择至选曲保持播放位置，按阶段渐入声部。350ms 插值可在对应 profile 的 `transition_ms` 调整；34 代自身的音量更新仍存在，因此不是 24 代逐帧行为的完全复制。
- `sinobuz24_compat`：使用原有完整混音资源和 600ms 衔接，作为兼容方案。
- `smooth_transition=0`：选择兼容资源并交回原生调度。`enabled=0`：关闭插件。

主配置旧 `transition_ms` 已移至各 profile。非默认 BGM、特殊强制模式交由游戏处理。缺少主 profile 或资源时尝试 fallback；挂钩校验失败时选择兼容完整混音并使用原生调度；主程序 hash 不匹配时不安装挂钩。日志在游戏根目录 `daily24.log`。

当前已完成离线音量、资源、同步逻辑和挂钩测试；真实游戏切屏听感需要运行游戏确认。

## 架构与接入其他代目

- `include/playback.hpp`：通用同步、阶段增益和渐变，不含游戏地址。
- `profiles/*.ini`：资源映射、日程、声部数、阶段目标音量和策略。
- `include/host_iidx34.hpp`：34 代地址、状态读取、音频调用与挂钩适配。
- `src/plugin.cpp`：SDK 生命周期、配置、文件重定向和回退。

新增普通三轨 BGM 通常只需新增 profile 和资源，沿用 `layered_sync`；完整混音可用 `crossfade_fullmix`，仅替换资源可用 `original`。`schedule=weekday` 用七天目录，`schedule=fixed` 配合 `fixed_variant=default` 用固定目录。资源根目录中的 `{day}` 会替换为当天或固定目录名。`layer_count` 支持 1–3；layered_sync 的 state0–state3 必须各有四个 0–127 数值，未用声部必须为 0。四个状态对应本机 34 代默认 BGM 控制器，当前 24 profile 将末状态映射为三轨齐奏，不叠加第四轨完整混音。

`[map]` 左侧为相对 `/data/sound/system/` 的原始路径，右侧为相对 resource_root 或 `/data/sound/system/` 下绝对虚拟路径。禁止目录穿越。参考现有两个 profile；三轨必须有一致的采样率、长度和循环区间，当前适配器要求 44.1kHz、零起点循环。

新代目若依赖分段接续、额外声部、独有事件或不同循环方式，仍须增加对应播放策略；换宿主程序版本则需新的 host adapter。profile 化不会自动复刻所有代目的特殊行为。

## 构建和部署

Windows x64，Visual Studio 2022 C++ 工具链；`build.cmd` 通过 vswhere 查找安装目录，也可设置 `VCVARS64`。运行：

```bat
build.cmd
python tools/prepare_sinobuz.py --source-root "原版24/data/sound/system/0" --game-root "游戏根目录"
python tools/deploy.py --game-root "游戏根目录" --validate-only
python tools/deploy.py --game-root "游戏根目录"
```

Python 资源工具需要 numpy；部署工具只使用标准库。不要以 Python -O 执行部署工具。部署前关闭游戏。工具先校验主程序、全部 profile 资源、三轨循环和 DLL 导出，再备份已有 DLL/配置并安装。构建产物位于 build/；构建会运行 C++ 测试，包括真实 MinHook 安装/还原测试。

资源准备保持 SD9 压缩音频 payload 不变，使用 24 原始头部增益的有效权重，按 34 原版选曲声响度校准共同增益，并限制各阶段采样峰值留出余量。头部增益有量化，峰值限制也可能使个别日期低于参考；每一天的实测结果在生成资源的 manifest.json 中，不能把离线 LUFS 等同于实机最终响度。

**当前安装迁移的前提**：已有 `data/sound/system/daily24/` 的 v4 完整混音、模式预览、选定音效等资源仍供两个 profile 共用。prepare_sinobuz 只生成新的 42 个原始声部资源，不重建历史 v4 资源。新克隆必须从自己的既有安装保留该资源目录；deploy 会检查缺失项并停止。游戏资源及音频不随源码分发。

## 版本管理

Git 只收录源码、配置模板、profile、必要工具、测试、构建脚本及第三方许可。历史研究文档、旧脚本、反汇编、视频、音频资源、备份、日志和 build/work 输出仍保留在本地，但由 .gitignore 排除。新增顶层维护目录时需相应更新白名单。

更新前 v5 的源码和 DLL/配置保存在 `backups/before-framework-v6/`，部署工具也会按时间备份。回退时关闭游戏，恢复备份中的 DLL 和 INI 即可；旧资源未被替换。
