# IIDX BGM Bridge

[English](README.md) · [下载](https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge/releases) · [开发指南](docs/development.zh-CN.md)

基于 Windows x64 Spice SDK 的 IIDX 33/34 系统 BGM 插件。内置 SINOBUZ profile，按星期选择七套编曲，并在菜单切换时保持三个声部同步播放。

**项目不包含游戏程序或音频。** 请使用自己的 IIDX 18–24 或 33 音频，以及受支持的 IIDX 33/34 本地安装生成运行资源。导入工具不会下载游戏数据。

## 功能

- IIDX 18–23 音频，包括 PENDUAL 的 PRESENT、FUTURE 两套编曲。
- SINOBUZ 七套每日编曲，也可以固定星期。
- Sparkle Shower 音频，保留原有选曲前奏和循环边界。
- 三轨连续播放，可配置阶段音量和渐变时间。
- 完整混音兼容 profile，可使用宿主原始调度。
- 每日选定音乐音效、退出音效，以及其他选曲模式使用的完整混音。
- 独立资源目录，不覆盖原始游戏音频。
- 通过 profile 管理资源映射，将宿主适配独立，便于扩展。

## 兼容范围

| 组件 | 要求 |
| --- | --- |
| 系统 | Windows x64 |
| 宿主 | 下方列出的 IIDX 33/34 构建 |
| 加载器 | 支持 Spice SDK 插件及 `-k` DLL 参数的 Spice2x |
| 音频来源 | 本地 IIDX 18–24 或 33 资源（见下文），以及目标游戏安装 |
| Python | 使用 Release 压缩包无需安装；从源码构建时需要 |

### DLL 支持的游戏版本

**beatmania IIDX 34 ZINRAI**

- `2026091700, 2026091600`

**beatmania IIDX 33 Sparkle Shower**

- `2026081900`

### 支持的音频资源版本

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

资源版本对应已验证的来源安装。导入工具检查所需文件和音频格式，不强制校验该版本字符串。其他游戏构建及资源修订版暂不列为支持。

宿主按十位游戏版本号识别，不检查整份 DLL 的 hash。优先读取 `prop/bootstrap.xml` 的 release_code，其次可用的运行时 AVS ext，最后 `prop/ea3-config.xml` 的 ext。支持音频布局未改变的已打补丁 DLL。插件仍校验音频函数入口和调用关系；修改版本标签不能让不兼容程序获得支持。

其他游戏版本会被拒绝。自定义 BGM 和强制特殊状态继续使用宿主逻辑。离线测试覆盖播放、挂钩和资源生成，但不代表所有游戏模式均已验证，也不能替代实机听感测试。衔接采用三轨编曲，同时保留宿主的音量更新机制，并非逐帧完整模拟 IIDX 24。

## 安装

1. 关闭游戏。从 [Releases](https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge/releases) 下载并解压 Windows 压缩包，将内容复制到目标 IIDX 33/34 根目录。GitHub 自动生成的源码 ZIP 不是可直接使用的二进制包。
2. 将 `modules/bgm_bridge.example.ini` 复制为 `modules/bgm_bridge.ini`。更新包只提供示例配置，避免覆盖现有设置。
3. 运行 `tools/import-resources.exe`，选择音频 profile，再填写来源路径和目标游戏根目录。Sparkle Shower 使用 IIDX 33 的 `data/sound/system`。所选资源全部生成并通过检查后才会安装。
4. 在现有 Spice2x 启动命令后添加 `-k bgm_bridge.dll`，保留其他参数。DLL 应放在 `modules/` 目录。
5. 启动游戏。若没有生效，请查看游戏根目录的 `bgm_bridge.log`。

也可以通过命令行导入：

```bat
tools\import-resources.exe --source-root "D:\IIDX24\data\sound\system\0" --game-root "D:\IIDX34"
```

将 Sparkle Shower 音频导入任一受支持宿主：

```bat
tools\import-resources.exe --profile sparkle33 --source-root "D:\IIDX33\data\sound\system" --game-root "D:\IIDX34"
```

随后在主 INI 中设置 `profile=sparkle33` 和 `fallback_profile=sparkle33`。导入工具仅生成资源，不改变 profile 选择。

生成文件位于 `data/sound/system/bgm_bridge/<profile>/`。重新导入会将上一套资源保存在旁边的 `<profile>.backup-*` 目录。可使用 `--output-root` 指定单独的输出目录，先检查再安装。导入前请关闭游戏。

## IIDX 18–23 音频

| Profile | 音频 | 播放方式 |
| --- | --- | --- |
| `resort18` | IIDX 18 Resort Anthem | SSP / 独立阶段曲目 |
| `lincle19` | IIDX 19 Lincle | SSP / 独立阶段曲目 |
| `tricoro20` | IIDX 20 tricoro | 同步声部 |
| `spada21` | IIDX 21 SPADA | 同步声部 |
| `pendual22` | IIDX 22 PENDUAL PRESENT | 同步声部 |
| `pendual22_future` | IIDX 22 PENDUAL FUTURE | 同步声部 |
| `copula23` | IIDX 23 copula | 同步声部 |

18/19 代可选择来源游戏根目录、`sys_sd` 目录或 `sys_sound.ssp` 文件；导入工具直接读取原始 SSP，不需要额外解包工具。20–23 代可选择来源游戏根目录或 `data/sound/system`。

```bat
tools\import-resources.exe --profile tricoro20 --source-root "D:\IIDX20" --game-root "D:\IIDX34"
```

随后在主 INI 设置 `profile=tricoro20`、`fallback_profile=tricoro20_compat`。其他 20–23 代 profile 使用各自的 `<profile>_compat` 回退。18/19 代则将两个字段设为相同 ID（例如 `resort18`），并保持 `smooth_transition=1`：独立阶段曲目需要播放控制器。控制器不能初始化时，插件会保留游戏原始音频，避免将不同曲目叠加播放。

18/19 代保留原始前奏和循环边界，在卡片进入、模式选择、选曲曲目之间交叉渐变；导入选定音乐音效，退出音效保留宿主版本。20–23 代默认菜单使用同步声部，独立选曲模式使用生成的完整混音。只有明确选择 PENDUAL FUTURE profile 时才使用 `_02` 资源；不会将历史残留文件作为 copula 编曲导入。新增的是音频 profile，不包括 18–23 代游戏宿主支持。

## 配置

编辑 `modules/bgm_bridge.ini`：

```ini
[bgm_bridge]
enabled=1
profile=sinobuz24
fallback_profile=sinobuz24_compat
weekday=auto
smooth_transition=1
```

| 设置 | 可选值及行为 |
| --- | --- |
| `enabled` | `0` 关闭插件，`1` 开启 |
| `profile` | `sinobuz24` 使用同步三轨，`sinobuz24_compat` 使用完整混音；`sparkle33` 使用 Sparkle Shower；18–23 代见上表 |
| `fallback_profile` | 主 profile 或其资源无法加载时使用的配置 |
| `weekday` | `auto`、`sun`、`mon`、`tue`、`wed`、`thu`、`fri`、`sat` |
| `smooth_transition` | `1` 使用选定的控制策略；`0` 使用兼容资源及宿主原始调度 |

`sparkle33` 保留两个来源变体及原始选曲前奏和循环边界，由宿主原始调度负责衔接，不使用 SINOBUZ 渐变。两套音频均可用于任一受支持宿主。

星期根据电脑本地日期选择，启动后固定至下次重启。渐变时间在各 profile 的 `[playback] transition_ms` 中设置，不属于主 INI。默认三轨渐变为 350 ms，兼容交叉渐变为 600 ms。修改配置后需重启。

缺少 profile 或资源时尝试回退。控制器校验失败时，使用兼容资源和原始调度。初始化不能完成时撤销已安装的挂钩。游戏版本不支持或音频函数布局不兼容时，插件不启用。

## 更新与卸载

更新时关闭游戏，替换 DLL、工具和 profiles，保留现有 `bgm_bridge.ini`。资源生成逻辑或 profile 路径变化时，重新运行导入工具。保留上一版压缩包以便回退。

卸载时移除启动参数，再删除 `modules/bgm_bridge.dll`、对应 INI/示例、内置 profiles 和 `data/sound/system/bgm_bridge/` 下生成的各 profile 目录。导入工具若没有其他安装需要，也可删除。项目不会改写原始游戏音频。

## 常见问题

- **仍播放原版 BGM：** 查看 `bgm_bridge.log`，确认 `-k` 参数、游戏版本和资源导入结果。
- **导入提示缺少文件：** SINOBUZ 选择 IIDX 24 的 `data/sound/system/0`，需要完整七天资源；Sparkle Shower 选择 IIDX 33 的 `data/sound/system`，包含两个变体目录。还需要目标游戏的原始系统文件。
- **音量偏小或衔接异常：** 反馈星期、profile、宿主版本、模式，并尽可能提供录制片段。附上生成的 `manifest.json` 校准值和相关日志；不要将游戏音频文件上传到 issue。
- **需要原来的调度行为：** 设置 `smooth_transition=0` 并重启。

## 开发与发布

构建、测试、profile 格式、资源校准和自动发布见 [中文开发指南](docs/development.zh-CN.md) 或 [English developer guide](docs/development.md)。行为变化涉及文档时，请同步更新两种语言。游戏文件、本地配置、生成音频、日志和研究工作目录均不进入版本管理或发布包。

## 许可

项目代码采用 [GPL-3.0-only](LICENSE)。第三方组件见 [许可说明](THIRD_PARTY_NOTICES.md) 及发布包内的许可证。游戏数据由用户单独提供，不受项目许可证覆盖。本项目独立开发，与 KONAMI 无隶属或背书关系。
