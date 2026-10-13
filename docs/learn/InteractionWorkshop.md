# Interaction：从项目级 plugin 到可复用 module

本手册用一个距离交互功能贯穿 module、reflection、ECS、Input、C#、Editor 和打包边界。
完成后，你应能解释每个扩展点由谁发现、由谁创建、何时更新、由谁销毁，而不只是复制代码。
导航：[学习入口](StartHere.md) · [总览](README.md) · [周计划](WeeklyPlan.md) · [进度记录](Progress.md)。

> 范围：只在你的练习项目中实现 `Plugins/Interaction`，不修改引擎。
> 本文依据当前仓库静态核实；示例未执行编译或运行，不宣称已经通过测试。
> P0–P6 是你逐步执行的实验，不是一次粘贴即可完成的完整功能包。

## 0. 实验约定与文件布局

- 建议宿主项目命名为 `EngineLearning`，plugin 命名为 `Interaction`；不要把二者混为一个 module。
- 每阶段单独 commit；先只做一个可观察变化，再继续添加功能。
- 每次记录：改动文件、调用链、日志或截图、负例、尚未验证的假设。
- native C++ 改动采用关闭 Editor → 重新生成必要文件 → build → 重启的迭代方式。
- C# reload 不等于 C++ hot unload；不要默认 DLL 中的反射类型、回调或存活对象可安全热卸载。
- 下方树中的新文件是实验规划；模板最初只生成 descriptor、Build.cs 和 module h/cpp。

```text
EngineLearning/
├── EngineLearning.lproject
├── Config/GameSettings.json
├── Source/
│   ├── EngineLearning.Build.cs
│   ├── EngineLearning.Target.cs
│   └── EngineLearningModule.h/.cpp
├── Game/
│   ├── Content/                 # 用 Editor 创建测试场景，不手写资源格式
│   └── Scripts/InteractionProbe.cs
└── Plugins/Interaction/
    ├── Interaction.lplugin
    ├── README.md
    ├── Scripts/                 # P4 需要时添加；Generated 是生成产物
    └── Source/
        ├── InteractionRuntime/
        │   ├── InteractionRuntime.Build.cs
        │   ├── InteractionRuntimeModule.h/.cpp
        │   ├── InteractionAPI.h/.cpp
        │   ├── Components/InteractableComponent.h
        │   ├── Systems/InteractionScanSystem.h/.cpp
        │   └── Events/InteractionEvents.h
        └── InteractionEditor/
            ├── InteractionEditor.Build.cs
            ├── InteractionEditorModule.h/.cpp
            └── Tools/InteractionValidationTool.h/.cpp
```

依赖方向：宿主 gameplay → `InteractionRuntime` → `Runtime`；`InteractionEditor` → `InteractionRuntime` + `Editor`。
Runtime 不得包含 Editor 头文件或引用 Editor 类型；Editor 能读取 Runtime 数据，反方向禁止。
构造函数只存配置、设默认值；不得在构造或 CDO 构造时查 World、订阅事件、打开工具或分配场景实体。
module 的 `StartupModule/ShutdownModule` 成对注册/解绑；World system 用 `OnStartup/OnTeardown` 管理 World 级资源。

## P0. 生成脚手架，证明两个 module 的生命周期

**目标：** 明白 plugin 是发现/启用单元，module 才是编译、依赖和加载单元。

1. 先用 Blank template 创建并启动 `EngineLearning`，保留默认 module 日志和 `ExampleScript.cs`。
2. 在项目打开后使用 **Tools > Plugin Browser > New Plugin**，输入 `Interaction`。
3. 生成后检查 descriptor、两个 Build.cs、两个 module h/cpp；退出 Editor，再由你执行项目构建和启动。
4. 不先加入任何 gameplay 代码；收集启动与正常退出日志。

### 模板核对清单

仓库的 `Templates/Plugin` 文件名与内容均需替换；实现依据是
[ReplacePluginTokens / CopyTemplateTree](../../Engine/Editor/Source/LuminaEditor.cpp)。

| token | Interaction 替换值 | 说明 |
| --- | --- | --- |
| `$PLUGINNAME` | `Interaction` | descriptor、README、日志 |
| `$PLUGINDESCRIPTION` | 自定义描述 | JSON 中注意字符串转义 |
| `$RUNTIMEMODULE` | `InteractionRuntime` | 目录、文件、class、module 名 |
| `$EDITORMODULE` | `InteractionEditor` | 同上 |
| `$RUNTIMEMODULEUPPER` | `INTERACTIONRUNTIME` | 头文件导出宏的前缀 |
| `$EDITORMODULEUPPER` | `INTERACTIONEDITOR` | 头文件导出宏的前缀 |
| `$PLUGINNAMEUPPER` | `INTERACTION` | 替换器支持；当前模板文件未使用 |

手动方案必须复制到新目录，在副本中替换**全部路径和文本**，长 token 先替换。
尤其不要遗漏 `MODULEUPPER`：否则会留下 `$..._API` 或错误大小写的 export macro。
检查不存在残余 `$PLUGIN...`、`$RUNTIME...`、`$EDITOR...`；然后生成项目文件。
不要把未替换的原模板再次覆盖到工作目录，也不要修改 `Templates/Plugin`。
Blank 的 `Plugins/README.md` 只列部分 token，不能替代上表。

当前 descriptor 默认 `EnabledByDefault=true`、`EditorOnly=false`、`ContainsContent=false`：
`InteractionRuntime` 是 `Runtime/PreEngineInit`，`InteractionEditor` 是 `Editor/EditorInit`。
两个 module 均为 `SharedLibrary`，`PublicIncludePaths.Add(".")`。
Runtime Build.cs 的 public dependency 为 `Runtime`；Editor 的为 `Runtime`、`Editor`、`InteractionRuntime`。
两者 private dependency 均含 `ImGui`、`RPMalloc`、`Tracy`；先保留模板，不把 ImGui 误认成 Editor module。

两个 cpp 都 include 自己的 module header、`Core/Module/ModuleManager.h`、`Log/Log.h`，
通过 `IMPLEMENT_MODULE` 声明入口，`StartupModule/ShutdownModule` 输出日志。
预期启动包含 `[Interaction] Runtime module ready`、`[Interaction] Editor module ready`；
正常退出包含相应 `module shutdown`，不是靠 static constructor 打印日志。
模板中仍有 Lua 注释，不代表本实验应走 Lua；P4 以实际 C# 实现为依据。

Blank 核实点：`Source/*.Build.cs` 继承 `LuminaGameModuleRules`，
设置 `CSharpBindingsDirectory = ModulePath("../Game/Scripts/Generated")`；
Target 继承 `LuminaGameTargetRules`，设置 `LaunchModuleName` 和 `SetProjectFileToOpen`。
它的 cpp 同样使用 `IMPLEMENT_MODULE`；`.lproject` 包含名称、GUID、描述、版本与 Plugins 列表。
手动生成 Blank 时检查 `$PROJECTNAMEUPPER`、`$PROJECTNAME`、`$PROJECTGUID`、`$PROJECTDESCRIPTION`；
替换器还支持 `$LUMINADIRBACKSLASH`、`$LUMINADIR`，是否出现取决于模板版本。
`GenerateProject.bat/.sh` 使用 `LUMINA_DIR`；生成 IDE 文件不等于完成编译。

**验收：** 截图 Plugin Browser、保存 descriptor 与四条生命周期日志；解释为何 Game 不应加载 Editor module。
**常见错误：** plugin 同名冲突；路径 token 未替换；module/class/Build.cs 名不一致；只生成未重建。
源码：[Plugin Browser](../../Engine/Editor/Source/UI/Tools/PluginBrowserEditorTool.cpp)、
[PluginManager](../../Engine/Source/Runtime/Source/Core/Plugin/PluginManager.cpp)、
[ModuleManager](../../Engine/Source/Runtime/Source/Core/Module/ModuleManager.cpp)。

## P1. 纯 native export API 与跨 module 调用

**目标：** 先证明链接边界，不引入 reflection、World 或 C#。

在 `InteractionRuntime/InteractionAPI.h` 写入以下**实验新增 API**：

```cpp
#pragma once
namespace Lumina
{
    INTERACTIONRUNTIME_API bool IsInteractionDistanceValid(float Distance, float Radius);
}
```

对应 `InteractionAPI.cpp`：

```cpp
#include "InteractionAPI.h"
namespace Lumina
{
    bool IsInteractionDistanceValid(float Distance, float Radius)
    {
        return Radius > 0.0f && Distance >= 0.0f && Distance <= Radius;
    }
}
```

这是简单规则函数，不是引擎已有 API；输入约定为有限值，后续数据校验负责排除 NaN/Infinity。
export macro 沿用模板 module header 的真实机制，由 build system 定义，不手工定义为 `dllexport`。
在宿主 `EngineLearning.Build.cs` 添加 `PrivateDependencyModuleNames.Add("InteractionRuntime");`。
宿主 module cpp include `InteractionAPI.h`，在 `StartupModule` 用已有 `LOG_INFO` 打印几组返回值。
先测 `(1,2)`、`(3,2)`、`(0,0)`，再测边界 `(2,2)`；不用反射注册此函数。
若宿主 public header 暴露该 module 的类型，才相应评估 public dependency，而非一律 public。

**验收：** 调用点位于宿主 module，定义位于 plugin DLL；正例、负例、边界结果符合定义。
**常见错误：** 忘记 dependency 导致 include/link 失败；导出宏写为 `INTERACTION_API`；只在同一 cpp 自测。
P1 的硬依赖意味着禁用 plugin 前必须移除宿主调用及该依赖，不能期待 linker 自动降级。

## P2. reflection component 与 Editor 可见性

**目标：** 用一个 float 从 Reflector 走到组件列表、属性面板，再进入序列化实验。

最小示例由 [SLifetimeComponent](../../Engine/Source/Runtime/Source/World/Entity/Components/LifetimeComponent.h)
改写；去掉其 `HideInComponentList`，属性使用实际存在的 `PROPERTY(Editable)`，参见
[SInputComponent](../../Engine/Source/Runtime/Source/World/Entity/Components/InputComponent.h)。
文件必须为 `Components/InteractableComponent.h`，generated include 与文件 stem 一致：

```cpp
#pragma once
#include "Core/Object/ObjectMacros.h"
#include "InteractableComponent.generated.h"

namespace Lumina
{
    REFLECT(Component)
    struct INTERACTIONRUNTIME_API SInteractableComponent
    {
        GENERATED_BODY()

        PROPERTY(Editable)
        float Radius = 2.0f;
    };
}
```

1. 只添加这个 header；重新生成必要文件、构建、重启 Editor，检查 Reflector 输出与错误。
2. 在测试场景实体上添加该组件；检查属性面板是否能编辑 Radius。
3. 放置两个带 `STransformComponent` 的目标，分别配置不同 Radius；暂时不写 system。
4. Radius 单位跟随 World 坐标单位，先在场景测量，不无依据地叫它“米”。

**验收：** 可添加组件；Radius 默认值为 2，可编辑且切换选择后仍保留。
**常见错误：** 保留 `HideInComponentList`；generated include 名错误；手写 generated 文件；把 C# `[Property]` 用在 C++。
`REFLECT` 不等于已完成 C# event bridge；组件可见也不等于已经验证保存加载，后者放在 P6。

## P3. system 距离扫描：遵循实际注册与更新机制

**目标：** 从“模块已加载”推进到“每个 World 中的 system 已创建并更新”。

先逐段阅读以下真实实现，不写假想的 `RegisterSystem` 或 `Update(float)`：

- [CEntitySystem](../../Engine/Source/Runtime/Source/World/Entity/Systems/EntitySystem.h)：`Configure/OnStartup/OnUpdate/OnTeardown`。
- [EntitySystems::CreateMissing](../../Engine/Source/Runtime/Source/World/Entity/Systems/EntitySystem.cpp)：扫描反射 class、排除禁用项、检查 `ShouldCreate`。
- [CWorld::RegisterSystems](../../Engine/Source/Runtime/Source/World/World.cpp)：调用 `CreateMissing`、重建 schedule、启动 pending systems。
- [SLifetimeSystem](../../Engine/Source/Runtime/Source/World/Entity/Systems/LifetimeSystem.h)：最小 `RequireUpdate` 与 view 遍历。
- [FSystemContext](../../Engine/Source/Runtime/Source/World/Entity/Systems/SystemContext.h)：`GetDeltaTime/CreateView/DispatchEvent`。
- [STransformComponent](../../Engine/Source/Runtime/Source/World/Entity/Components/TransformComponent.h)：`GetWorldLocation` 与 cached 访问差异。

按顺序实现，不同时加入 Input：

1. 参照 LifetimeSystem 新建反射 `SInteractionScanSystem : public CEntitySystem`，使用匹配的 generated header。
2. `Configure()` 中用真实 `RequireUpdate(EUpdateStage::FrameEnd)`；先只验证 startup、有限频率 update、teardown 日志。
3. 第一版扫描点固定为 World 原点，这是实验输入，不宣称已有 player 查询 API。
4. 通过 `GetContext().CreateView<...>().ForEach(...)` 扫描 Interactable 与 Transform，借鉴 LifetimeSystem 回调签名。
5. 计算目标到扫描点的距离，复用 P1 的规则；先只报告进入/离开范围，不每帧刷日志。
6. 原始 `GetWorldLocation()` 可能触发 lazy cache 更新；起步先不声明访问，让 system 按当前实现 exclusive 执行。
7. 理解 Transform cache 更新时机后，再决定 `Reads<Ts...>()/Writes<Ts...>()` 与 const view 查询形式；访问声明必须覆盖真实读写。
8. 把候选状态保持在 World 所属 system 内，不放进 process-wide static；实体消失时清除候选。

先检查 Registry 的 const view 语义，再声明只读访问；不能只把调度声明改成 Reads 而继续修改组件。
未声明 component access 的 system 在 `ConfigureSystem()` 中被设为 exclusive，不代表可以永远忽略并发设计。
`OnUpdate()` 无 delta 参数，时间从 context 获取；反射 class 必须在 World 建立系统列表前可发现。
不在 module Startup 中手动 new 一个全局 system，也不修改引擎 World.cpp 来塞入自己的类。

**验收：** 移动目标穿越边界仅产生一次进入/离开；删除候选不崩溃；两次 Play/Stop 不重复订阅。
**常见错误：** 只写 class 未反射；漏 `RequireUpdate`；在构造读 World；读写声明与 view 不一致；跨 World 保存裸指针。

## P4. Input → event → C#：先读真实链，再设计 bridge

**目标：** 一次按下触发一次有效交互，C# 能观察成功结果，重复 reload 不叠加监听。

先完成阅读清单，再写接口；这里刻意不提供未经运行验证的全套跨语言代码。

| 真实入口 | 要确认的问题 |
| --- | --- |
| [InputSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/InputSystem.cpp) | `FrameStart/Highest`；Input context 归属；为何 dispatch managed code 时 exclusive |
| [native EntityScript](../../Engine/Source/Runtime/Source/Scripting/EntityScript.h) | Input/Action 回调的 native 端签名 |
| [managed EntityScript](../../Engine/Source/LuminaSharp/Scripting/EntityScript.cs) | `EnableInput()`、`OnAction`；override 时保留 `base.OnAction` |
| [InputAction](../../Engine/Source/LuminaSharp/Input/InputAction.cs) | action binding 的真实声明方式 |
| [PerceptionSystem.cpp](../../Engine/Source/Runtime/Source/World/Entity/Systems/PerceptionSystem.cpp) | 已有 `Context.DispatchEvent<FPerceptionUpdatedEvent>` 示例 |
| [EventDispatcher](../../Engine/Source/Runtime/Source/World/ECS/EventDispatcher.h) | typed sink 的 `Connect/Disconnect` 与 payload 生命周期 |
| [ScriptDelegate](../../Engine/Source/LuminaSharp/Core/ScriptDelegate.cs) | managed subscription 的 `DelegateBinding.Unbind/Dispose` |
| [CoreEvents](../../Engine/Source/LuminaSharp/Core/CoreEvents.cs) | native delegate 暴露给 managed 的真实用法 |
| [TargetAssembler](../../Engine/Tools/LuminaBuildTool/Graph/TargetAssembler.cs) | plugin 默认 bindings 目录与项目脚本程序集的关系 |

分四次提交：

1. 基于 Blank 的 `ExampleScript.cs` 创建 `InteractionProbe.cs`；先只在 `OnReady` 输出日志。
2. 配置一个项目 Input action，按真实示例接收 pressed 边沿；验证未聚焦 viewport、持续按住和 released 行为。
3. native 侧加入请求与成功通知；检查 entity 仍有效、目标仍在范围、Radius 合法，成功才发 event。
4. 最后接通 C# 成功回调；说明 bridge 的 owner、线程、数据拷贝与解绑时刻。

建议设计接口：`RequestInteraction(Interactor, Target)`、`FInteractionSucceededEvent`、`OnInteractionSucceeded`。
**这些都是本实验拟新增的接口/类型，不是仓库现有 API，也不是可直接调用的代码。**
native `DispatchEvent<T>` 不会自动变成 C# event；必须沿真实 delegate/bindings 机制实现 adapter。
先证明 native event，再实现 managed bridge；不要在 native worker callback 中未经核实地调用 managed。
Input 在 FrameStart 而 P3 扫描在 FrameEnd，必须决定消费上一帧候选还是请求时重新校验，记录选择。
绑定的 entity/World 不同则不能混用；不要把 `FEntity` 当作跨 World 或跨保存加载的永久身份。
C# 在 `OnDetach`、system 在 `OnTeardown`、module 在 `ShutdownModule` 释放各自订阅；owner 销毁前解绑。
plugin 默认生成目录逻辑见 TargetAssembler；不得让 Interaction 的 bindings 意外写回引擎 LuminaSharp 源码。

**验收：** 范围内按下仅回调一次；范围外零次；按住不重复；删除目标后请求安全失败；C# reload 后仍只有一次。
**常见错误：** 把任意 native struct 当作可自动跨语言 event；忘记 base 回调；订阅句柄丢失；闭包保留已销毁实体。

## P5. Editor 校验与可视化：单向依赖，不侵入 Runtime

**目标：** 在不按 Play 时发现错误，并用图形解释交互范围。

先实现一个独立 validation tool，再考虑 property customization；不要第一步修改 EditorUI。
真实扩展样例是 [NetworkingEditorModule](../../Engine/Plugins/Networking/Source/NetworkingEditor/NetworkingEditorModule.cpp)，
配合 [FToolsMenuRegistry](../../Engine/Editor/Source/UI/Tools/ToolsMenuRegistry.h) 阅读。
`Register(FToolsMenuEntry)` 返回 handle；`ShutdownModule` 必须调用 `Unregister(handle)`。
菜单 callback 捕获 plugin DLL 内代码，忘记解绑会在退出时留下失效函数对象。

1. 在 `InteractionEditor` 的 Startup 注册菜单；先只打开空工具，正常退出验证配对清理。
2. 校验当前编辑 World 的 Interactable：Radius 必须有限且大于零、必须具备 Transform。
3. 列出实体、问题和修复建议；第一版只报告，不自动修改场景。
4. 再添加选中目标的半径与朝向显示；读取真实 viewport/tool 绘制入口后实现。
5. 参考 [PrimitiveDrawInterface](../../Engine/Source/Runtime/Source/Renderer/PrimitiveDrawInterface.h) 的 `DrawSphere/DrawLine`；
   这只是绘制接口，不能假设有一个任意线程可用的全局 drawer。退出工具时清理持有的 World 引用。

若以后为自定义资产添加 asset editor，必须读取
[EditorToolRegistry](../../Engine/Editor/Source/UI/Tools/EditorToolRegistry.h)。
真实 `RegisterAssetEditor` 需要 Owner，按 owner 用 `UnregisterAll` 清理；模板 cpp 的无参注释示例不可直接照抄。
本实验组件不需要为了可见性而注册一个 asset editor；P2 reflection 与 P5 工具是不同层次。

**验收：** Radius 为负值、零、非有限值时有可定位报告；修复后消失；图形半径与 P3 边界一致。
**常见错误：** Runtime include Editor header；工具缓存旧 World；关闭窗口却未清理 callback；为校验偷偷改引擎。

## P6. 保存加载、禁用、Game/打包与跨项目复用

**目标：** 证明功能不只在当前 Editor 会话中“碰巧能跑”。

按以下矩阵逐项记录结果，不把 Editor Play 当成 Game target 或 packaged build 的替代品。

| 场景 | 操作 | 通过条件 |
| --- | --- | --- |
| 保存加载 | 编辑 Radius，保存场景，退出，再启动并打开 | authored 值保留；候选缓存重算，没有旧 entity 引用 |
| Play 循环 | 连续三次 Play/Stop，再切换场景 | 每次初始化/销毁配对；一次输入一次事件 |
| system 禁用 | 阅读 World 的禁用系统路径后在可用 UI 中验证 | teardown 生效；重新启用不重复监听 |
| plugin 禁用 | 先备份内容，移除 P1 硬依赖和测试调用，再在 Browser 禁用，重生成并重启 | 无 Interaction 启动日志；不依赖旧 DLL 误加载 |
| 缺失类型 | 在备份项目中打开依赖该组件的内容 | 记录真实诊断；不把未知类型丢失宣称为无损支持 |
| Game target | 按项目实际 Target/BuildTool 配置运行 Game | Runtime ready；无 Editor ready；不需 Editor DLL |
| packaged build | 配置 GameStartupMap、CookRoots，按实际 packager 流程打包 | 脱离 Editor 启动，Input→C# 有效，资源与脚本齐全 |
| 第二项目 | 复制 plugin 源码与必需内容，不复制 Binaries/Intermediates | 在新 Blank 项目生成、编译、启用，重复 P0/P2/P4 核心验收 |

打包路径先阅读 [ProjectPackagerEditorTool](../../Engine/Editor/Source/UI/Tools/Debug/ProjectPackagerEditorTool.cpp)
和 [BuildTool README](../../Engine/Tools/LuminaBuildTool/README.md)，不要猜测 CLI 参数。
模板 `ContainsContent=false`；若新增 plugin 内容，需核实 mount/cook 配置，不只复制文件夹。
禁用只改变项目选择，不等于当前进程可以卸载所有 C++ 类型；以关闭 Editor、重新构建启动为实验边界。
Runtime authored property 与瞬态状态分开；增加持久引用前先阅读现有 entity/asset 引用序列化，不保存裸地址。
复制到第二项目时清除 `EngineLearning` 硬编码、绝对路径和宿主脚本假设；README 写明版本、依赖与配置步骤。

**验收：** 矩阵每行附证据；未实际执行的一律标为“未验证”，不得用设计说明替代运行结果。
**常见错误：** 没设置启动地图；依赖 Editor DLL；禁用 plugin 却保留链接依赖；复制生成 bindings 当作长期源文件。

## 完成标准：能独立解释并扩展

- 能画出 descriptor → Build.cs → module → reflection → World system → Input/event → C# 的调用与依赖链。
- 能区分 `StartupModule`、system `OnStartup`、script `OnReady`，并指出每个订阅的对应解绑点。
- 能在不修改引擎的前提下给 Runtime 加入交互冷却配置，并让 Editor 校验、保存加载与 Game 验收覆盖它。
- 在 [Progress.md](Progress.md) 登记证据和遗留问题，再用 [WeeklyPlan.md](WeeklyPlan.md) 安排下一轮小步迭代。
