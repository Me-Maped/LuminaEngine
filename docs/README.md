# Lumina Engine 架构文档

本目录收录 Lumina Engine 的架构与使用文档。所有结论均来自仓库中的实际源码，每条都附带对应的源码路径，便于读者自行核对。

| 文档 | 内容 | 适合谁读 |
| --- | --- | --- |
| [引擎概览](EngineOverview.md) | 仓库布局、模块与 target、启动流程、模块系统、主循环与更新阶段、核心子系统一览、扩展点 | 第一次接触仓库、想知道「东西都在哪」的人 |
| [渲染架构](RenderingArchitecture.md) | RHI 分层、设备与交换链、内存与资源、帧编排、shader 与管线缓存、场景渲染的 pass 顺序、材质、网格、纹理流式加载、GPU 计时、线程模型 | 要改动渲染器或排查渲染问题的人 |
| [ECS 指南](ECSGuide.md) | `FEntity` 句柄、`FRegistry`、组件存储与布局 trait、typed view、生命周期信号与 `FCommandBus`、如何定义新组件、性能注意事项 | 写游戏逻辑、定义组件与系统的人 |
| [反射系统](ReflectionSystem.md) | 标记宏、Reflector 工具与 libclang 解析、生成的代码、运行时元数据、序列化、编辑器属性 UI、C# 绑定生成、热重载 | 需要让类型接入序列化/编辑器/C# 的人 |

## 学习与实践

想按步骤学习并最终扩展自己的 module，请从 [学习路线](learn/README.md) 开始。首次上手直接阅读 [第一天操作指南](learn/StartHere.md)，再按逐周任务完成 Interaction plugin。

其他相关文档：

- [README.md](../README.md) — 引擎总览、功能特性、安装与构建、故障排查。
- [CONTRIBUTING.md](../CONTRIBUTING.md) — 贡献流程、编码规范与命名约定。
- [DEPENDENCIES.md](../DEPENDENCIES.md) — 外部依赖包的组成、版本、来源与许可证。
- [Engine/Tools/LuminaBuildTool/README.md](../Engine/Tools/LuminaBuildTool/README.md) — 构建与项目生成系统。

## 阅读约定

- 文档正文使用简体中文，**专业名词保留英文原文**（类型名、宏、文件名、路径、以及 Vulkan / ECS / PBR / shader / pipeline 这类技术术语），以免与源码中的标识符脱节。
- 代码块中的标识符与注释按源码原样保留。
- 形如 `Engine/Source/Runtime/Source/World/ECS/Registry.h` 的行内代码表示仓库内的真实路径，可直接用于跳转与检索。
- 文档描述的是某个时间点的代码。若发现文档与代码不一致，以代码为准，并欢迎提 issue 或 PR 修正。
