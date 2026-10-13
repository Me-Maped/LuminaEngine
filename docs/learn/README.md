# Lumina Engine 学习路线

目标：理解引擎架构、各模块的职责与协作方式，最终独立开发可复用的 module/plugin。正文使用简体中文，API、类型、路径与专业术语保持英文。

## 从哪里开始

**今天只做一件事：按 [StartHere.md](StartHere.md) 跑通源码版 Editor 和独立项目。** 不必先读完整套架构文档。

| 文档 | 什么时候读 | 你会得到什么 |
| --- | --- | --- |
| [第一天操作指南](StartHere.md) | 现在 | 环境检查、构建命令、项目创建、断点、常见阻塞处理 |
| [逐周任务](WeeklyPlan.md) | 每周开始前 | 10 周阅读、实验、检查项与降级路线 |
| [Interaction 实践手册](InteractionWorkshop.md) | 第 2 周开始 | Runtime/Editor module、component、system、事件、C#、发布的递进练习 |
| [进度与验收模板](Progress.md) | 第一次操作前复制一份 | 进度表、源码阅读记录、实验记录、测试矩阵与最终验收 |

本目录是学习和实践指导，不是已经执行完成的测试报告。样例代码与命令需要在你的环境中构建验证；不要把阅读源码视为完成运行验收。

## 时间和先备能力

- 推荐 10 周，每周 8～12 小时，分成 3～4 次 session；忙时可按一个阶段两周推进。
- 先备能力：C++ class/template、RAII、指针/引用、基础多线程、Git、基本 debugger 使用。
- C# 不熟也能完成前四周，第五周再补 class、attribute、delegate 与 managed/native 边界。
- Vulkan 不是入门前提。第八周先理解数据流和资源生命周期，不要求自行实现 renderer。

遇到语言基础问题时，补完当前实验涉及的内容再继续，不要为了准备而无限推迟项目实践。

## 如果今天只有 90 分钟

1. **前 15 分钟**：记录 commit、工作区修改、工具链与 GPU；创建自己的学习记录。
2. **接下来 30 分钟**：执行 StartHere 的环境检查和 Setup。安装或下载尚未结束就先停在这里，不跳过错误。
3. **接下来 30 分钟**：尝试 Build；编译等待期间只读 `Launch.cpp` 与 `Application.cpp`，画启动流程草图。
4. **最后 15 分钟**：保存命令和首个错误，写下下一次第一个动作；如果已能启动，再尝试设置一个断点。

第一天没有编译完成不是失败。区分“正在等待构建”和“环境阻塞”，避免用阅读进度掩盖运行问题。

## 学习路线与关卡

| 周 | 主题 | 必须拿得出的证据 |
| --- | --- | --- |
| 1 | 构建、启动、调试 | 源码版 Editor 运行、独立项目、断点调用栈 |
| 2 | Build / Module / Plugin | 自己的 plugin 加载、关闭与禁用记录 |
| 3 | World / ECS / System | 组件查询与候选目标变化可观察 |
| 4 | Object / Reflection / Serialization | Inspector 字段与保存重开后的数据 |
| 5 | Input / Event / C# | 一次完整交互闭环 |
| 6 | Assets / Editor / Cook | Editor 校验工具，Runtime/Editor 依赖检查 |
| 7 | TaskSystem / 生命周期 | 单线程基线、并发约束、生命周期测试 |
| 8 | Renderer / ECS 交界 | 一条 Extract→Render→Submit 数据流与 debug visualization |
| 9 | 其他业务模块 | Physics/Animation/Audio/AI/UI/Networking 的模块设计卡 |
| 10 | 交付与复用 | 测试矩阵、打包验证、第二个项目接入 |

三个关键关卡：

1. **第 2 周**：可以独立添加、调用和禁用一个 module。
2. **第 5 周**：能让自己的 component/system/reflection/script 共同完成一个玩法。
3. **第 10 周**：能交付无需修改引擎核心的可复用 plugin，并说明它的限制。

## 贯穿项目：Interaction

练习项目统一命名为 `EngineLearning`，插件为 `Interaction`，module 为 `InteractionRuntime` 和 `InteractionEditor`。若这些名字已被占用，请统一更名，不要覆盖已有内容。

第一版只做：距离选择目标 → 输入请求 → 校验 → 事件 → 可观察响应。

暂不做：联网同步、复杂碰撞选择、定制 shader、大型资源格式、通用可视化脚本。`SInteractableComponent` 等名称是练习中要创建的类型，不代表引擎已经提供这些 API。

## 工作方式

每次 session 固定执行：

1. 写出一个问题，例如“谁负责注销 module 注册的事件？”
2. 读 public header、一个调用点、实现和一个测试。
3. 先预测行为，再用断点或最小实验验证。
4. 保存日志、调用栈、截图或测试结果。
5. 更新 [进度记录](Progress.md)，写下下一步。

时间分配建议：30% 阅读，50% 实践，20% 验证和总结。完成条件是可重复的证据，不是“文件看过了”。

## 源码阅读参考

- [EngineOverview](../EngineOverview.md)：查模块边界、启动和更新顺序。
- [ECSGuide](../ECSGuide.md)：查 handle、storage、view 和 deferred command。
- [ReflectionSystem](../ReflectionSystem.md)：查宏、生成物、metadata、serialization 和 binding。
- [RenderingArchitecture](../RenderingArchitecture.md)：查 RHI、pass、Extract 和资源释放。
- [LuminaBuildTool](../../Engine/Tools/LuminaBuildTool/README.md)：查规则、依赖传播与增量构建。
- [CONTRIBUTING](../../CONTRIBUTING.md)：查命名与代码风格。

文档和源码可能随版本变化。不要直接套用 UE 的语义，也不要根据类型名推断 GC、线程安全或 hot reload 的支持范围。冲突时读取实际调用链并记录差异。
