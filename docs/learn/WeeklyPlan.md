# 10 周引擎架构与 Module 实践计划

## 使用方式与完成标准

- 目标：能从源码解释引擎边界，并独立交付一个可构建、可验证、可复用的 module，而非读完全部源码。
- 每周安排 4 次 session：A 2h、B 3h、C 3h、D 2h，共 10h；忙时缩短 B/C 各 1h，最多各加 1h，保持 8–12h。
- 每次先花 10 分钟回顾证据，最后 15 分钟记录结果；这些时间已计入 session，不额外占用。
- 阅读顺序是接口 → 一个实现 → 一个调用方；只追本次问题，不横向展开整个目录。
- 下文“任务、实验”是学习者后续实施内容，不表示仓库已经有 Interaction 插件或相关功能。
- 练习宿主统一为 `EngineLearning`；`InteractionRuntime` 与 `InteractionEditor` 是拟建模块名；实际落盘位置与脚手架步骤以 [InteractionWorkshop](InteractionWorkshop.md) 为准。
- 首版只做单 World、单玩家、距离判定、一次按键请求与结果反馈；不做复杂 raycast、联网同步、通用技能系统。
- Runtime 拥有交互数据、判定与结果；Editor 只做编辑/诊断适配，不能成为 Runtime 的依赖。
- 距离阈值的单位、边界是否包含、禁用目标、并列候选、失效 Entity 的策略都必须写成测试契约。
- 每周保留四类证据：源码定位、实验输入、实际输出、未解决问题；“预计输出”不能冒充运行结果。
- 文档中的源码链接均相对本目录；构建命令只从当前 [BuildTool README](../../Engine/Tools/LuminaBuildTool/README.md) 与 [StartHere](StartHere.md) 复制并核对。
- 不根据文件夹名推断线程安全、生命周期或可用平台；不根据类似引擎的设计补全本仓库行为。
- 降级只降低实验规模，不把未执行的验收勾选为完成；下一周先补关键前置门槛。

## 第 1 周：运行与调试——建立能返回源码的基线

**周产物：** 环境记录、一次构建/启动证据、启动到退出调用链；尚不修改玩法。

### Session A · 2h：建立环境与目标清单
- 任务/阅读：按 [StartHere](StartHere.md) 核对前置条件；读 [仓库 README](../../README.md)、[架构概览](../EngineOverview.md) 的工具链与 target 部分。
- 实验：记录 OS、编译器、.NET、GPU/驱动与仓库 revision；从已记录的 target 中选择一个启动基线，保存实际构建入口。
- 可观察结果：得到“已满足/缺失/未检查”三列，而不是一句“环境好了”。
- [ ] 验收： 能解释 module 与可构建 target 的区别，并指出自己的目标名称与配置。
- 降级：工具链不满足时只完成差异清单，不安装猜测版本，不把 Linux 环境等同于已经受测的组合。

### Session B · 3h：首次构建与最小启动
- 任务/阅读：读 [BuildTool README](../../Engine/Tools/LuminaBuildTool/README.md)、[Lumina target](../../Engine/Applications/Lumina/Lumina.Target.cs)、[MinimalHost](../../Engine/Applications/MinimalHost/Source/MinimalHost.cpp)。
- 实验：使用 Session A 核实的入口构建并启动一次；记录配置、工作目录、实际二进制路径、首条失败诊断或成功画面。
- 可观察结果：日志能区分 prerequisite、生成、C++ 编译、链接、运行期资源/驱动问题。
- [ ] 验收： 保留完整首错上下文；若成功，保存启动与正常退出的证据。
- 降级：图形启动受阻时先核对 MinimalHost 的实际初始化路径；仅在确认所需参数与依赖后换基线，不凭名称假设它无需 GPU。

### Session C · 3h：断点贯穿启动与一帧
- 任务/阅读：读 [Launch.cpp](../../Engine/Applications/Lumina/Source/Launch.cpp)、[Application.cpp](../../Engine/Source/Runtime/Source/Core/Application/Application.cpp)、[Engine.cpp](../../Engine/Source/Runtime/Source/Core/Engine/Engine.cpp)。
- 实验：在启动、主循环、引擎 Update 与 Shutdown 设置断点，记录调用栈；选择一帧标注 stage 顺序。
- 可观察结果：一张从进程入口到 World 更新的调用链，包含线程身份与退出入口。
- [ ] 验收： 能区分窗口事件泵、引擎更新、World 更新；断点位置均能重新命中。
- 降级：不能运行时以源码调用点画虚线链，并明确标注“静态追踪，未验证调用栈”。

### Session D · 2h：冻结 Interaction 的最小问题
- 任务/阅读：回看 [StartHere](StartHere.md) 的故障排查与 [InteractionWorkshop](InteractionWorkshop.md) 的范围；读 [ModuleInterface.h](../../Engine/Source/Runtime/Source/Core/Module/ModuleInterface.h)。
- 实验：用纸面场景定义玩家、两个候选目标、距离阈值与一次交互输出；列出启动/退出观察点。
- 可观察结果：一页需求与明确不做清单，不要求立刻生成插件代码。
- [ ] 验收： 首版无需 raycast、网络或新 Renderer；环境阻塞已有最小复现与下一步。
- 降级：本周仅完成静态追踪时，下周最多做插件规则审阅，不提前声称运行成功。

## 第 2 周：Build / Module / Plugin——先让边界成立

**周产物：** Interaction 两模块骨架及加载证据；实际创建步骤见 Workshop。

### Session A · 2h：拆解规则与依赖方向
- 任务/阅读：对照 [StartHere](StartHere.md)、[InteractionWorkshop](InteractionWorkshop.md)、[Runtime.Build.cs](../../Engine/Source/Runtime/Runtime.Build.cs)、[Editor.Build.cs](../../Engine/Editor/Editor.Build.cs)。
- 实验：画 target → module → public/private dependency 图；为 InteractionRuntime、InteractionEditor 各写一句职责与必要依赖。
- 可观察结果：图中 Editor 适配依赖 Runtime，Runtime 不反向引用 Editor。
- [ ] 验收： 能用一个头文件可见性案例解释 public/private，而非把所有依赖设为 public。
- 降级：规则复杂时先只读两个现有模块，暂不加入第三方库和自定义构建步骤。

### Session B · 3h：插件描述符与加载时机
- 任务/阅读：读 [Networking.lplugin](../../Engine/Plugins/Networking/Networking.lplugin)、[PluginDescriptor.h](../../Engine/Source/Runtime/Source/Core/Plugin/PluginDescriptor.h)、[Engine.cpp](../../Engine/Source/Runtime/Source/Core/Engine/Engine.cpp)。
- 实验：按 Workshop 建描述符与两模块规则；逐字段核对名字、HostType、Type、LoadingPhase 的实际解析与调用位置。
- 可观察结果：一张“选用阶段/此时可用服务/不可使用服务”表；不照抄 Networking 的 Earliest 作为玩法默认值。
- [ ] 验收： 每个模块名能对应规则类、文件基名和描述符条目；阶段选择有源码依据。
- 降级：暂时不访问 World、不注册 UI，只保留加载/关闭日志，将阶段依赖问题隔离。

### Session C · 3h：构建与加载空骨架
- 任务/阅读：读 [GameplayExtrasRuntimeModule.cpp](../../Engine/Plugins/GameplayExtras/Source/GameplayExtrasRuntime/GameplayExtrasRuntimeModule.cpp)、[ModuleManager.h](../../Engine/Source/Runtime/Source/Core/Module/ModuleManager.h)、[NetworkingEditor.Build.cs](../../Engine/Plugins/Networking/Source/NetworkingEditor/NetworkingEditor.Build.cs)。
- 实验：参照现有模块实现启动/关闭日志；按 BuildTool 文档刷新工程并构建选定 target，重启后观察两模块。
- 可观察结果：日志可分辨 Runtime 与 Editor 的加载/关闭；保存实际产物路径与对应配置。
- [ ] 验收： 骨架成功构建；Editor 模块限制在 Editor host；导出宏来源能追到构建工具。
- 降级：加载失败先验证名字、依赖、配置和产物，再检查模块加载诊断；不要同时新增反射类型。

### Session D · 2h：验证禁用与退出路径
- 任务/阅读：读 [PluginManager.cpp](../../Engine/Source/Runtime/Source/Core/Plugin/PluginManager.cpp)、[ModuleManager.cpp](../../Engine/Source/Runtime/Source/Core/Module/ModuleManager.cpp)、[PluginBrowserEditorTool.cpp](../../Engine/Editor/Source/UI/Tools/PluginBrowserEditorTool.cpp)。
- 实验：通过已核实的插件配置流程禁用并重启，再启用并重启；比较启动、关闭与错误日志。
- 可观察结果：能区分“未启用”“未编译”“加载失败”；明确 C++ 修改走重建/重启而非假设 hot reload。
- [ ] 验收： 禁用不再出现 Interaction 启动日志；启用恢复；原有启动基线不受影响。
- 降级：没有运行环境时提交描述符与依赖检查表，加载、禁用和退出验收保持未完成。

## 第 3 周：World / ECS / System——距离交互的计算核心

**周产物：** 可重复的距离候选选择，不接复杂输入或表现层。

### Session A · 2h：先分清存储与调度
- 任务/阅读：读 [ECSGuide](../ECSGuide.md)、[World.h](../../Engine/Source/Runtime/Source/World/World.h)、[EntitySystem.h](../../Engine/Source/Runtime/Source/World/Entity/Systems/EntitySystem.h)。
- 实验：画 World、Entity、Component、System 与 SystemContext 关系；找出系统注册、配置及移除的真实调用点。
- 可观察结果：明确玩法使用 World facade，系统内部按已有 Context 约定访问存储，不把裸 Registry 扩散为插件公共接口。
- [ ] 验收： 每个关系箭头都附一个声明或调用点；能指出 Entity handle 不等于对象指针。
- 降级：先只研究一个现有 System，不通读全部 ECS 容器实现。

### Session B · 3h：定义数据与判定契约
- 任务/阅读：读 [TransformComponent.h](../../Engine/Source/Runtime/Source/World/Entity/Components/TransformComponent.h)、[KinematicsSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/KinematicsSystem.cpp)、[SystemContext.h](../../Engine/Source/Runtime/Source/World/Entity/Systems/SystemContext.h)。
- 实验：设计最小交互 Component 与串行 System；只读取世界位置及启用状态，计算距离并按明确规则选一个目标。
- 可观察结果：固定输入坐标对应固定候选；并列距离使用显式稳定规则，不依赖 view 遍历顺序。
- [ ] 验收： 写清平方距离/距离的单位、阈值包含策略、负阈值策略与 System 的读写集合。
- 降级：先实现与引擎无关的纯判定函数及样例表，再接入一个 World；本周不做空间加速结构。

### Session C · 3h：接入 World 并处理失效目标
- 任务/阅读：读 [World.cpp](../../Engine/Source/Runtime/Source/World/World.cpp)、[Entity.h](../../Engine/Source/Runtime/Source/World/ECS/Entity.h)、[ECS SelfCheck](../../Engine/Applications/ECSBench/Source/SelfCheck.cpp)。
- 实验：构造近/远两个目标，移动玩家、禁用近目标、删除当前目标；在执行交互前重新验证候选有效性。
- 可观察结果：记录每次候选切换与原因；删除目标后不再访问旧组件引用。
- [ ] 验收： 近、远、阈值边界、禁用、销毁五种情况有实际输入与输出。
- 降级：无法搭场景时用纯逻辑测试覆盖前四项；Entity 销毁必须另记为 World 集成待验证项。

### Session D · 2h：阶段与暂停行为
- 任务/阅读：读 [UpdateStage.h](../../Engine/Source/Runtime/Source/Core/UpdateStage.h)、[WorldManager.cpp](../../Engine/Source/Runtime/Source/World/WorldManager.cpp)、[World.h](../../Engine/Source/Runtime/Source/World/World.h) 的系统调度部分。
- 实验：记录当前 stage、World 与目标；比较运行、暂停、重新开始的候选状态，决定何时清理缓存。
- 可观察结果：同一 World 的候选不会泄漏到另一轮运行，暂停行为与已写契约一致。
- [ ] 验收： 能解释为何选该更新阶段；没有依赖枚举声明顺序推断真实帧顺序。
- 降级：仅保留单 World 串行处理；未验证的并行和多 World 行为明确排除在首版之外。

## 第 4 周：Object / Reflection / Serialization——让数据可配置

**周产物：** 最小交互参数的元数据与一次保存/重载对照。

### Session A · 2h：追踪一个字段的元数据
- 任务/阅读：读 [ReflectionSystem](../ReflectionSystem.md)、[ReflectionMacros.h](../../Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h)、[Class.h](../../Engine/Source/Runtime/Source/Core/Object/Class.h)。
- 实验：选择一个现有 Component 字段，画声明 → Reflector → generated code → 运行期元数据 → consumer 链。
- 可观察结果：区分 Object、反射 struct 与 ECS Component；不因为都有元数据就认为生命周期相同。
- [ ] 验收： 为每一步给出真实文件位置；能说明字段声明与生成文件的职责差异。
- 降级：生成产物尚不存在时只阅读生成器，明确不把预计生成文本当作本机构建产物。

### Session B · 3h：给最小参数添加反射
- 任务/阅读：读 [ReflectionSpecifiers.h](../../Engine/Applications/Reflector/Source/Reflector/ReflectionSpecifiers.h)、[ReflectionStep.cs](../../Engine/Tools/LuminaBuildTool/Graph/ReflectionStep.cs)、[TransformComponent.h](../../Engine/Source/Runtime/Source/World/Entity/Components/TransformComponent.h)。
- 实验：按现有 Component 模式让交互半径、启用状态进入反射；构建后定位输入清单、生成头与生成源。
- 可观察结果：能够在实际生成代码中找到自己的类型与字段；不手工修改 generated 文件。
- [ ] 验收： generated include 顺序正确；一次字段修改后的生成差异与预期相符。
- 降级：只暴露一个数值字段，暂去掉容器、Object 引用和脚本导出，先消除首条 Reflector 错误。

### Session C · 3h：验证 Serialization round-trip
- 任务/阅读：读 [Struct.cpp](../../Engine/Source/Runtime/Source/Core/Object/Class/Struct.cpp)、[JsonStructuredArchive.cpp](../../Engine/Source/Runtime/Source/Core/Serialization/Structured/JsonStructuredArchive.cpp)、[ObjectArchiver.cpp](../../Engine/Source/Runtime/Source/Core/Serialization/ObjectArchiver.cpp)。
- 实验：沿当前可用保存流程保存半径与启用状态，修改内存值后重载；比较保存前、内存改动后、重载后三份数值。
- 可观察结果：确认实际保存的是哪些字段；把当前候选等瞬态状态与持久参数分开。
- [ ] 验收： 至少两个非默认值 round-trip 一致；能定位字段是否序列化的判断处。
- 降级：World 存档链过长时先用已核实的结构化 archive 调用模式测单 struct，不声称已验证完整关卡保存。

### Session D · 2h：修改 schema 与清理边界
- 任务/阅读：读 [Object.h](../../Engine/Source/Runtime/Source/Core/Object/Object.h)、[PropertyTable.cpp](../../Engine/Editor/Source/UI/Properties/PropertyTable.cpp)、[ReflectionSystem 的序列化说明](../ReflectionSystem.md)。
- 实验：在备份样本上增加一个带默认值的字段后重载；检查 Editor 展示与实际存档值是否一致。
- 可观察结果：形成“旧数据/新增字段/默认值/观察结果”表；生命周期结论只引用实际创建与销毁路径。
- [ ] 验收： 不把一次 schema 演进成功推广为任意兼容；未验证的重命名与类型变化列入风险。
- 降级：保留最初 schema 并记录版本策略，不为学习实验破坏唯一项目数据。

## 第 5 周：Input / Event / C#——形成一次交互闭环

**周产物：** 输入请求 → native 判定 → 结果 → C# 观察；每一段可独立诊断。

### Session A · 2h：输入从哪里进入 World
- 任务/阅读：读 [EventProcessor.h](../../Engine/Source/Runtime/Source/Events/EventProcessor.h)、[InputContext.h](../../Engine/Source/Runtime/Source/Input/InputContext.h)、[InputSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/InputSystem.cpp)。
- 实验：从已工作的输入样例追一条按键路径；对比 viewport 有焦点、失焦、Editor UI 消费输入三种情况。
- 可观察结果：日志能区分物理按键、输入层消费、玩法请求，不把持续按住等同于一次触发。
- [ ] 验收： 定义 Interaction 的单次触发语义，并找到实际查询或绑定入口。
- 降级：先用现有可观察输入记录，不新增 action map 编辑器和复杂输入重映射。

### Session B · 3h：把请求与结果解耦
- 任务/阅读：读 [EventDispatcher.h](../../Engine/Source/Runtime/Source/World/ECS/EventDispatcher.h)、[CommandBus.h](../../Engine/Source/Runtime/Source/World/ECS/CommandBus.h)、[ECSGuide](../ECSGuide.md) 的事件与命令部分。
- 实验：依据现有调用方式接入“请求交互”和“成功/失败结果”；明确生产、消费、结构性修改发生的时机。
- 可观察结果：一次有效按键只对应一次结果，目标消失时有明确失败结果而非崩溃或静默重复。
- [ ] 验收： 区分即时 signal、排队 event、延迟 command；绑定与解绑成对且与 World 生命周期匹配。
- 降级：先让请求直接进入串行 native 函数并返回值，标注事件适配未完成，不盲目引入异步。

### Session C · 3h：跨 native / managed 边界
- 任务/阅读：读 [NativeBindings.cs](../../Engine/Source/LuminaSharp/Interop/NativeBindings.cs)、[NativeCallGenerator.cs](../../Engine/Source/LuminaSharp.Generators/NativeCallGenerator.cs)、[EntityScriptSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/EntityScriptSystem.cpp)。
- 实验：先跑通一个现有 C# 调用，再为最小交互结果选择已支持的字段/函数绑定模式；逐层核对生成文件、模块名、导出与 managed 调用。
- 可观察结果：同一请求在 native 和 C# 日志中有可对照的目标与结果；不要求自定义跨语言事件容器。
- [ ] 验收： 至少一次真实 managed → native 调用或 native 结果读取成功；记录实际绑定路径。
- 降级：.NET 受阻时保留 native 闭环与生成物检查，C# 运行验收未完成；不要手写猜测的 P/Invoke 签名。

### Session D · 2h：检查重启与脚本改动
- 任务/阅读：读 [DotNetHost.h](../../Engine/Source/Runtime/Source/Scripting/DotNet/DotNetHost.h)、[ManagedTypeRegistry.h](../../Engine/Source/Runtime/Source/Scripting/ManagedTypeRegistry.h)、[ReflectionSystem](../ReflectionSystem.md) 的热重载部分。
- 实验：先用完整重启验证两轮交互；环境支持时再做一次小型脚本修改，观察订阅、结果计数与类型切换。
- 可观察结果：重复进入运行态没有重复回调；区分 C# reload 与 native module 重建/重启。
- [ ] 验收： 记录解绑责任方；脚本编译失败时也能说明当前哪一版仍在运行。
- 降级：只保证完整重启路径，脚本 reload 列为后续兼容性工作，不把它设为首版必要条件。

## 第 6 周：Assets / Editor / Cook——区分开发数据与交付数据

**周产物：** 可编辑参数、一个资源引用追踪表和最小 Cook 验证记录。

### Session A · 2h：跟踪一个已有资产
- 任务/阅读：读 [AssetRef.h](../../Engine/Source/Runtime/Source/Assets/AssetRef.h)、[AssetRegistry.h](../../Engine/Source/Runtime/Source/Assets/AssetRegistry/AssetRegistry.h)、[AssetManager.h](../../Engine/Source/Runtime/Source/Assets/AssetManager/AssetManager.h)。
- 实验：选一份已能加载的项目资产，记录磁盘位置、逻辑路径、引用、加载者与释放责任；不要先创建新 asset 类型。
- 可观察结果：能区分 registry 元信息、引用与已加载对象，不把路径字符串当永久存活保证。
- [ ] 验收： 追到一次真实加载调用和失败处理；注明 Interaction 首版是否真的需要外部资源。
- 降级：找不到可靠素材就维持纯数值参数与日志反馈，资源依赖保持为空。

### Session B · 3h：Editor 只做适配
- 任务/阅读：读 [PropertyTable.cpp](../../Engine/Editor/Source/UI/Properties/PropertyTable.cpp)、[EditorModule.h](../../Engine/Editor/Source/Module/EditorModule.h)、[NetworkingEditorModule.cpp](../../Engine/Plugins/Networking/Source/NetworkingEditor/NetworkingEditorModule.cpp)。
- 实验：先验证通用属性面板能否满足半径编辑；确需专用工具时，沿现有注册方式添加最小诊断，并记录移除位置。
- 可观察结果：InteractionEditor 可检查参数/目标，真正的判定仍在 InteractionRuntime。
- [ ] 验收： 修改半径后结果随之变化；没有把交互规则写进 UI draw 回调。
- 降级：只用现有属性面板，不为展示一个数值新建完整 Editor tool；Editor 模块仍保留明确适配职责。

### Session C · 3h：沿 Cook 根追踪可达资源
- 任务/阅读：读 [AssetCooker.h](../../Engine/Editor/Source/Cooker/AssetCooker.h)、[CookGraph.cpp](../../Engine/Editor/Source/Cooker/Graph/CookGraph.cpp)、[CookRoot.h](../../Engine/Source/Runtime/Source/Assets/AssetRegistry/CookRoot.h)。
- 实验：从当前编辑器已确认的 Cook/打包入口处理最小项目；列出入口关卡、直接/间接引用及插件内容根。
- 可观察结果：保存实际 Cook 日志与产物目录；对无资源插件明确记录“不需要新增 CookRoots”的理由。
- [ ] 验收： 至少一份资源能从根追到产物，或证明首版不依赖新增资源；不猜测 Cook CLI。
- 降级：工具链受阻时只完成可达图和实际入口定位，把 Cook 执行标为 blocked。

### Session D · 2h：离开 Editor 检查边界
- 任务/阅读：读 [LuminaLauncher.cpp](../../Engine/Applications/LuminaLauncher/Source/LuminaLauncher.cpp)、[Launch.cpp](../../Engine/Applications/Lumina/Source/Launch.cpp)、[Lumina target](../../Engine/Applications/Lumina/Lumina.Target.cs)。
- 实验：在已验证的 Game/打包运行路径检查参数与交互；比较 Editor 可见数据和交付产物中的必要数据。
- 可观察结果：没有 InteractionEditor 时核心逻辑仍可运行，缺失资源有可归因诊断。
- [ ] 验收： 保存运行配置与结果；Editor 构建成功不能替代 Game/交付路径验证。
- 降级：先做 Game 构建依赖审查与纯逻辑测试，将实际打包启动保留为第 10 周门槛。

## 第 7 周：TaskSystem——并发不是首版前提，生命周期才是

**周产物：** 串行基线、可选 snapshot 实验、退出与取消策略。

### Session A · 2h：画任务的拥有者与等待关系
- 任务/阅读：读 [TaskSystem.h](../../Engine/Source/Runtime/Source/TaskSystem/TaskSystem.h)、[JobScheduler.h](../../Engine/Source/Runtime/Source/TaskSystem/Scheduler/JobScheduler.h)、[TaskGraph.h](../../Engine/Source/Runtime/Source/TaskSystem/TaskGraph.h)。
- 实验：追一个现有任务提交与等待调用，标记输入寿命、结果位置、等待方与任务结束后的清理责任。
- 可观察结果：一张明确的依赖图，而非“扔到后台就更快”的结论。
- [ ] 验收： 能解释所读调用是否同步等待，以及捕获引用在等待期间为何有效。
- 降级：只研究一个简单批处理调用，不改 scheduler、不自建线程池。

### Session B · 3h：测量串行距离查询
- 任务/阅读：读 [SystemContext.h](../../Engine/Source/Runtime/Source/World/Entity/Systems/SystemContext.h)、[View.h](../../Engine/Source/Runtime/Source/World/ECS/View.h)、[JobProfiler.h](../../Engine/Source/Runtime/Source/TaskSystem/Scheduler/JobProfiler.h)。
- 实验：用固定种子或固定坐标生成小/中/大三组候选，记录串行查询耗时、规模、构建配置和重复次数。
- 可观察结果：数据能判断查询是否值得并行；候选选择结果在各轮保持一致。
- [ ] 验收： 分开测量查询与日志/场景创建成本；没有把 Debug 耗时直接当交付性能结论。
- 降级：没有可靠 profiler 时做外层计时并注明误差；不以单帧最小值判断收益。

### Session C · 3h：只并行纯数据计算
- 任务/阅读：读 [TaskSystem.cpp](../../Engine/Source/Runtime/Source/TaskSystem/TaskSystem.cpp)、[SystemContext.h](../../Engine/Source/Runtime/Source/World/Entity/Systems/SystemContext.h)、[World.h](../../Engine/Source/Runtime/Source/World/World.h) 的调度约束。
- 实验：可选地复制位置/标识到 snapshot，再按真实任务 API 分块计算局部候选；汇总后重新验证 Entity，再应用结果。
- 可观察结果：并行与串行结果一致；worker 不保存组件裸引用、不调用 Editor UI、不直接执行未确认安全的结构性修改。
- [ ] 验收： 写清 snapshot 拥有者、完成点与并列候选归并规则；收益不足时保留串行实现。
- 降级：完成 snapshot + 串行计算也可作为安全设计证据，但并行验收不勾选。

### Session D · 2h：退出时谁还在用数据
- 任务/阅读：读 [Engine.cpp](../../Engine/Source/Runtime/Source/Core/Engine/Engine.cpp) 的 Shutdown、[WorldManager.cpp](../../Engine/Source/Runtime/Source/World/WorldManager.cpp)、[ModuleManager.cpp](../../Engine/Source/Runtime/Source/Core/Module/ModuleManager.cpp)。
- 实验：查询期间删除目标、停止 World、退出程序；检查插件任务与订阅何时停止，是否在拥有者销毁前完成。
- 可观察结果：退出顺序图覆盖 World 数据、任务结果、委托与模块代码，不依赖程序结束时 OS 回收掩盖问题。
- [ ] 验收： 连续多轮开始/停止没有旧任务结果写回新 World；未测试情形有明确风险记录。
- 降级：取消异步路径并同步完成计算；不能证明寿命安全就不把后台任务并入首版。

## 第 8 周：Renderer / ECS 交界——让结果可见而不重写渲染器

**周产物：** 目标可视化与 Extract 边界图；不要求实现新 render pass。

### Session A · 2h：追一次场景提取
- 任务/阅读：读 [RenderingArchitecture](../RenderingArchitecture.md)、[WorldManager.cpp](../../Engine/Source/Runtime/Source/World/WorldManager.cpp)、[SceneRenderer_Extract.cpp](../../Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/SceneRenderer_Extract.cpp)。
- 实验：从一个已有可见实体追到 Extract 与 frame data；记录什么时候读取 ECS，什么时候消费提取结果。
- 可观察结果：图中明确 World 数据与渲染数据的所有权，不套用固定“独立 render thread”模型。
- [ ] 验收： 指出真实 Extract/Render 调用点；区分线程身份与 GPU 提交顺序。
- 降级：先只跟一个 transform，不追全部材质、shader 与 GPU culling 分支。

### Session B · 3h：使用现有 debug 绘制能力
- 任务/阅读：读 [DebugDrawLibrary.h](../../Engine/Source/Runtime/Source/World/DebugDrawLibrary.h)、[DebugDrawSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/DebugDrawSystem.cpp)、[SceneRenderer_Overlay.cpp](../../Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/SceneRenderer_Overlay.cpp)。
- 实验：从已存在且签名核实的绘制入口选择线或简单标记，显示玩家到候选目标的关系；成功/失败用不同表现区分。
- 可观察结果：可视化与第 3 周距离结果一致；删除或禁用目标后标记按预定寿命消失。
- [ ] 验收： Debug 绘制关闭时 Interaction 仍正确；没有为了高亮直接修改 RHI。
- 降级：使用文本日志或已有 Editor 诊断，不假设现成 outline API 可供插件调用。

### Session C · 3h：验证视口与暂停边界
- 任务/阅读：读 [DefaultSceneRenderer.h](../../Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/DefaultSceneRenderer.h)、[DefaultSceneRenderer.cpp](../../Engine/Source/Runtime/Source/World/Scene/RenderScene/Default/DefaultSceneRenderer.cpp)、[InputViewport.h](../../Engine/Source/Runtime/Source/Input/InputViewport.h)。
- 实验：切换视口尺寸与焦点，暂停/恢复，停止再开始；记录可见标记、候选数据与输入请求三者是否一致。
- 可观察结果：能识别“没有查询”“有查询但没画”“画了旧结果”三类问题。
- [ ] 验收： 没有跨 World/viewport 复用失效标记；日志包含足够的 World/帧标识供追踪。
- 降级：首版只支持一个游戏视口，明确限制；多视口只做架构审查，不顺手扩展需求。

### Session D · 2h：解释为什么不做新 pass
- 任务/阅读：读 [RenderScene.h](../../Engine/Source/Runtime/Source/World/Scene/RenderScene/RenderScene.h)、[RenderSceneFactory.cpp](../../Engine/Source/Runtime/Source/World/Scene/RenderScene/RenderSceneFactory.cpp)、[RenderingArchitecture](../RenderingArchitecture.md) 的资源与释放部分。
- 实验：比较现有 debug 绘制、Editor 工具、独立 pass 三条实现路径的依赖、资源所有权与测试成本。
- 可观察结果：一份短决策记录，首版选择最小侵入路径，并说明未来换方案的触发条件。
- [ ] 验收： 未新增 GPU 资源就不虚构资源管理成绩；若新增资源，须单独证明创建/释放路径。
- 降级：保留纯日志版可运行插件，把视觉增强列为 optional，不阻塞核心交付。

## 第 9 周：Physics / Animation / Audio / AI / UI / Networking 横向定位

**周产物：** 六模块边界矩阵；最多完成一个可移除的反馈适配。

### Session A · 2h：Physics 与 Animation 的输入输出
- 任务/阅读：读 [PhysicsScene.h](../../Engine/Source/Runtime/Source/Physics/PhysicsScene.h)、[AnimationSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/AnimationSystem.cpp)、[KinematicsSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/KinematicsSystem.cpp)。
- 实验：各选一条调用链，记录输入 Component、更新 stage、输出与寿命；判断距离交互是否需要碰撞查询或动画状态。
- 可观察结果：能够说明距离判定不等于可见性/遮挡判定；动画反馈失败不应改变 native 交互结果。
- [ ] 验收： 两个子系统各有一个真实读写边界；首版仍不引入复杂 raycast。
- 降级：只读接口与一个调用方，骨骼资源、物理后端和 ragdoll 不进入本周实现范围。

### Session B · 3h：Audio 与 UI 做结果消费者
- 任务/阅读：读 [AudioSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/AudioSystem.cpp)、[RmlUiBridge.h](../../Engine/Source/Runtime/Source/UI/RmlUiBridge.h)、[AudioSourceComponent.h](../../Engine/Source/Runtime/Source/World/Entity/Components/AudioSourceComponent.h)。
- 实验：先验证一个现有音频或 UI 示例；二选一把 Interaction 结果适配成反馈，另一项只写边界矩阵。
- 可观察结果：禁用反馈适配后请求与结果计数不变；缺失素材/文档时有明确降级输出。
- [ ] 验收： Runtime 判定不依赖 Editor 面板或某个 UI 页面存活；反馈解绑责任明确。
- 降级：继续使用日志；不要为了“闭环好看”新建 UI 框架、音频资源流水线或隐式强依赖。

### Session C · 3h：AI 与 Networking 只做契约分析
- 任务/阅读：读 [PerceptionSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/PerceptionSystem.cpp)、[NavMeshSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/NavMeshSystem.cpp)、[NetworkingRuntimeModule.cpp](../../Engine/Plugins/Networking/Source/NetworkingRuntime/NetworkingRuntimeModule.cpp)。
- 实验：分析“AI 发起请求”与“远端玩家发起请求”各缺什么：请求来源、目标身份、权限、校验、失败反馈；标为未来设计而非已有 API。
- 可观察结果：本地 Entity 标识与跨网络身份不混用；存在 Networking 插件不代表交互自动可复制。
- [ ] 验收： 写清首版只有本地权威判定；没有为了演示添加未验证的 RPC 或复制宏。
- 降级：只画假想时序并明确标注“未实现”，不运行需要未掌握配置的多进程网络实验。

### Session D · 2h：横向矩阵与去依赖检查
- 任务/阅读：回看本周六个入口及 [Lumina.BuildRules.cs](../../Engine/Build/Lumina.BuildRules.cs)，复核插件当前构建依赖。
- 实验：为六项填写“输入/输出/阶段/线程或任务约束/释放责任/Interaction 是否必需”；移除仅用于探索的依赖再构建。
- 可观察结果：核心距离交互可独立演示，可选消费者失败不会让判定失效。
- [ ] 验收： 矩阵没有把未知项填成默认安全；实际依赖数量与需求相符。
- 降级：不确定的依赖暂不删除，先追 include/链接来源；记录原因而非大范围改动引擎模块。

## 第 10 周：测试、发布与复用——把实验变成自己的 Module

**周产物：** 可复现包、测试矩阵、第二宿主复用证据与 [Progress](Progress.md) 记录。

### Session A · 2h：整理可重复测试
- 任务/阅读：读 [TestSuite.cs](../../Engine/Tools/LuminaBuildTool/Rules/TestSuite.cs)、[TestHarness.Build.cs](../../Engine/Source/TestHarness/TestHarness.Build.cs)、[EntityMapTests.cpp](../../Engine/Source/Runtime/Tests/EntityMapTests.cpp)。
- 实验：按现有测试发现规则组织纯距离判定用例，并核对新 suite 是否实际被扫描；再列出必须由 World 集成覆盖的用例。
- 可观察结果：测试涵盖阈值内/外/边界、并列、禁用、失效目标；不把纯函数通过当作 World 生命周期通过。
- [ ] 验收： 目标名由实际规则发现确认；保存测试输出、配置和失败时复现输入。
- 降级：suite 集成受阻时保留可重复独立样例与 expected/actual 表，测试自动化门槛记为未完成。

### Session B · 3h：干净构建与非 Editor 验证
- 任务/阅读：回看 [BuildTool README](../../Engine/Tools/LuminaBuildTool/README.md)、[Lumina target](../../Engine/Applications/Lumina/Lumina.Target.cs)、[AssetCooker.h](../../Engine/Editor/Source/Cooker/AssetCooker.h)。
- 实验：在独立工作副本或干净输出位置按已核实流程重建；验证 Editor 与 Game/交付路径，避免删除唯一资产或未提交修改。
- 可观察结果：可以从记录还原工具链、命令、产物、启动目录与资源前提；Game 不依赖 InteractionEditor。
- [ ] 验收： 没有依赖本机旧 generated 文件或遗留二进制；Cook/打包未执行则明确阻塞而非通过。
- 降级：先交付 Development 可复现基线，Shipping/打包单列待办，不把“能在 Editor 跑”写成发布完成。

### Session C · 3h：换宿主验证复用
- 任务/阅读：读 [PluginDescriptor.h](../../Engine/Source/Runtime/Source/Core/Plugin/PluginDescriptor.h)、[NetworkingEditor.Build.cs](../../Engine/Plugins/Networking/Source/NetworkingEditor/NetworkingEditor.Build.cs)，核对自己的公共头文件与依赖。
- 实验：将插件接入第二个最小项目/宿主，或在独立副本构造最小消费者；只接公开契约，不复制原项目私有实现。
- 可观察结果：清楚列出必须提供的 World 数据、输入适配、配置与可选反馈；无硬编码原项目路径。
- [ ] 验收： 第二消费者完成一次近距离成功和远距离失败；启用/禁用与退出均有证据。
- 降级：不能启动第二宿主时做最小消费者编译检查，复用运行验收保持未完成。

### Session D · 2h：演示、审阅与下一模块提案
- 任务/阅读：将证据汇总到 [Progress](Progress.md)，对照 [EngineOverview](../EngineOverview.md)、[ECSGuide](../ECSGuide.md)、[ReflectionSystem](../ReflectionSystem.md)、[RenderingArchitecture](../RenderingArchitecture.md)。
- 实验：做 10 分钟演示：加载插件 → 配置半径 → 近/远交互 → 删除目标 → 保存重载 → 退出；逐步给出实际证据。
- 可观察结果：提交包含范围、架构图、构建前提、测试结果、已知限制、失败排查与卸载责任的交接说明。
- [ ] 验收： Progress 区分完成/降级/blocked；能解释 Runtime/Editor 边界并提出一个有证据支撑的后续 module。
- 降级：没有通过的环节如实演示最小失败复现；冻结可运行版本，不在最后一晚补入网络或复杂渲染。

> 验证说明：本计划编写时只核对文档、源码入口与相对链接，未执行构建、启动、Cook 或插件实验；所有 checkbox 均待学习者凭实际证据确认。

## 最终自检

- [ ] 我能从进程入口追到 Interaction 的更新点，再追到退出时的数据与订阅清理。
- [ ] 我能解释构建依赖、运行期模块加载与插件启用是三个相关但不同的问题。
- [ ] 我能用测试证明距离契约，并说明 Entity、Object、反射元数据与资产引用各自的职责。
- [ ] 我能定位输入、managed binding、任务与渲染边界上的失败，而不是靠全局改动碰运气。
- [ ] 我交付的是一个范围有限、结果可复现的模块；所有未验证能力均已明确列出。
