# Grain: Ember Vale

一个独立的 RHI 展示程序。一座微体素（micro-voxel）岛屿以稀疏 brickmap 的形式保存在 GPU 显存中，
通过 raymarching 渲染并带一次全局光照弹射，同时作为一个小型动作 RPG 来游玩。没有引擎渲染器、
没有 `CWorld`、磁盘上没有任何资源。每个 shader 都内嵌在二进制中，每个模型都在启动时由方块搭建而成。

构建并运行：

```bash
./LuminaBuild.bat Build Grain -TargetType=Program -Configuration=Development
```

```bash
./Binaries/Windows64/Grain-Program-Development.exe -novalidation -unlocked
```

## 玩法

你是漂流到一座岛上的探索者，岛上有三座未点亮的信标。开采水晶、在黑夜带来的东西中活下来，
并把四块碎片带到每座信标处以点亮它。

| 输入 | 操作 |
| --- | --- |
| `W A S D` | 移动 |
| `Space` | 跳跃，或向上游动 |
| `Left Shift` | 冲刺，消耗体力 |
| 鼠标 | 视角 |
| 鼠标左键 | 挥击。命中弧形范围内的任何东西，否则挖掘地形 |
| 鼠标右键 | 施放 ember bolt。沿弧线飞行、引爆、炸出弹坑 |
| `E` | 点亮你正站着的信标 |
| `Tab` | 释放鼠标 |
| `Escape` | 退出 |

Husk 会贴近并攻击。Wisp 悬停在近战范围之外，且只在视线通畅时才开火，因此掩体有效。
Golem 行动缓慢、攻击很重，并掉落三块碎片。击杀和碎片会给予经验，升级会提升生命、体力、专注
以及一次挥击造成的伤害。太阳在八分钟内完成一圈，山谷越暗，生成越快。

## 调试按键

`H` 切换 overlay，`T` 切换弹射累积，`F` 切换空间滤波，`G` 切换时间解析，`V` 切换自由飞行相机。

## 标志

| 标志 | 效果 |
| --- | --- |
| `-seed N` | 世界与布点随机种子 |
| `-time N` | 从一天中 N% 处开始，25 是正午，75 是午夜 |
| `-daylength N` | 一个完整循环的秒数 |
| `-screenshot -frames N` | 以固定的六十步运行一段脚本化演示，并在第 N 帧写出 `Grain.png` |
| `-unlocked` | 启用 mailbox，因为默认会把每次测量都钉在刷新率上 |
| `-gputimes` | 最后一帧每个 pass 的 GPU 耗时细分 |
| `-nosim` | 跳过高密度水体体积 |
| `-nohud` `-nofilter` `-notemporal` `-noaa` | 关闭某一个阶段 |
| `-debugmat` `-debugnormal` | 主射线命中后立即返回 |
| `-freecam` | 自由飞行浏览场景，而不是跟随探索者 |

用 `-gputimes` 和 `-unlocked` 一起测量，覆盖 200 帧以上。每个 pass 的细分只涵盖最后一帧，
往往不具代表性，因此请相信平均帧时间，并用细分来看比例。
