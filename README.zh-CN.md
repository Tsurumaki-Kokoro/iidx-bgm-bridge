# IIDX BGM Bridge

[English](README.md) · [下载](https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge/releases) · [开发指南](docs/development.zh-CN.md)

基于 Windows x64 Spice SDK 的 IIDX 34 系统 BGM 插件。内置 SINOBUZ profile，按星期选择七套编曲，并在菜单切换时保持三个声部同步播放。

**项目不包含游戏程序或音频。** 请使用自己的 IIDX 24 和受支持的 IIDX 34 本地文件生成运行资源。导入工具不会下载游戏数据。

## 功能

- SINOBUZ 七套每日编曲，也可以固定星期。
- 三轨连续播放，可配置阶段音量和渐变时间。
- 完整混音兼容 profile，可使用宿主原始调度。
- 每日选定音乐音效、退出音效，以及其他选曲模式使用的完整混音。
- 独立资源目录，不覆盖原始游戏音频。
- 通过 profile 管理资源映射，将宿主适配独立，便于扩展。

## 兼容范围

| 组件 | 要求 |
| --- | --- |
| 系统 | Windows x64 |
| 宿主 | 标识为 `iidx34-20260916` 的 IIDX 34 构建 |
| 加载器 | 支持 Spice SDK 插件及 `-k` DLL 参数的 Spice2x |
| 音频来源 | 本地 IIDX 24 SINOBUZ `data/sound/system/0` 目录及受支持的 IIDX 34 安装 |
| Python | 使用 Release 压缩包无需安装；从源码构建时需要 |

### DLL 支持的游戏版本

**beatmania IIDX 34 ZINRAI**

- `2026091600`

### 支持的音频资源版本

**beatmania IIDX 24 SINOBUZ**

- `2017082800`

资源版本对应已验证的来源安装。导入工具检查所需文件和音频格式，不强制校验该版本字符串。其他游戏构建及资源修订版暂不列为支持。

受支持的 `modules/bm2dx.dll` SHA-256：

```text
3755587f44785effe5008f74cfe7dccbdf423ad09feead48e00a90f78a42fe83
```

其他程序修订版会被拒绝。自定义 BGM 和强制特殊状态继续使用宿主逻辑。离线测试覆盖播放、挂钩和资源生成，但不代表所有游戏模式均已验证，也不能替代实机听感测试。衔接采用三轨编曲，同时保留 IIDX 34 的音量更新机制，并非逐帧完整模拟 IIDX 24。

## 安装

1. 关闭游戏。从 [Releases](https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge/releases) 下载并解压 Windows 压缩包，将内容复制到 IIDX 34 根目录。GitHub 自动生成的源码 ZIP 不是可直接使用的二进制包。
2. 将 `modules/bgm_bridge.example.ini` 复制为 `modules/bgm_bridge.ini`。更新包只提供示例配置，避免覆盖现有设置。
3. 运行 `tools/import-resources.exe`，按提示填写 IIDX 24 的 `data/sound/system/0` 路径和 IIDX 34 根目录。导入需要一定时间，七天资源全部生成并通过检查后才会安装。
4. 在现有 Spice2x 启动命令后添加 `-k bgm_bridge.dll`，保留其他参数。DLL 应放在 `modules/` 目录。
5. 启动游戏。若没有生效，请查看游戏根目录的 `bgm_bridge.log`。

也可以通过命令行导入：

```bat
tools\import-resources.exe --source-root "D:\IIDX24\data\sound\system\0" --game-root "D:\IIDX34"
```

生成文件位于 `data/sound/system/bgm_bridge/sinobuz24/`。重新导入会将上一套资源保存在旁边的 `sinobuz24.backup-*` 目录。可使用 `--output-root` 指定单独的输出目录，先检查再安装。导入前请关闭游戏。

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
| `profile` | `sinobuz24` 使用同步三轨，`sinobuz24_compat` 使用完整混音 |
| `fallback_profile` | 主 profile 或其资源无法加载时使用的配置 |
| `weekday` | `auto`、`sun`、`mon`、`tue`、`wed`、`thu`、`fri`、`sat` |
| `smooth_transition` | `1` 使用选定的控制策略；`0` 使用兼容资源及宿主原始调度 |

星期根据电脑本地日期选择，启动后固定至下次重启。渐变时间在各 profile 的 `[playback] transition_ms` 中设置，不属于主 INI。默认三轨渐变为 350 ms，兼容交叉渐变为 600 ms。修改配置后需重启。

缺少 profile 或资源时尝试回退。控制器校验失败时，使用兼容资源和原始调度。初始化不能完成时撤销已安装的挂钩。程序 hash 不匹配时插件不启用。

## 更新与卸载

更新时关闭游戏，替换 DLL、工具和 profiles，保留现有 `bgm_bridge.ini`。资源生成逻辑或 profile 路径变化时，重新运行导入工具。保留上一版压缩包以便回退。

卸载时移除启动参数，再删除 `modules/bgm_bridge.dll`、对应 INI/示例、两个内置 profiles 和生成的 `data/sound/system/bgm_bridge/sinobuz24` 目录。导入工具若没有其他安装需要，也可删除。项目不会改写原始游戏音频。

## 常见问题

- **仍播放原版 BGM：** 查看 `bgm_bridge.log`，确认 `-k` 参数、程序 hash 和资源导入结果。
- **导入提示缺少文件：** 应选择 IIDX 24 的 `data/sound/system/0`，不是游戏根目录。需要完整七天的声部、退出及选定音乐音效；也需要 IIDX 34 原始系统文件。
- **音量偏小或衔接异常：** 反馈星期、profile、宿主 hash、模式，并尽可能提供录制片段。附上生成的 `manifest.json` 校准值和相关日志；不要将游戏音频文件上传到 issue。
- **需要原来的调度行为：** 设置 `smooth_transition=0` 并重启。

## 开发与发布

构建、测试、profile 格式、资源校准和自动发布见 [中文开发指南](docs/development.zh-CN.md) 或 [English developer guide](docs/development.md)。行为变化涉及文档时，请同步更新两种语言。游戏文件、本地配置、生成音频、日志和研究工作目录均不进入版本管理或发布包。

## 许可

项目代码采用 [GPL-3.0-only](LICENSE)。第三方组件见 [许可说明](THIRD_PARTY_NOTICES.md) 及发布包内的许可证。游戏数据由用户单独提供，不受项目许可证覆盖。本项目独立开发，与 KONAMI 无隶属或背书关系。
