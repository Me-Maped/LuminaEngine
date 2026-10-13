# Lumina Engine 架构概览

本文描述 Lumina Engine 的仓库布局、模块与目标、启动流程、模块系统、主循环与更新阶段、核心子系统、平台抽象以及扩展方式。所有路径均相对于仓库根目录，结论全部来自仓库中实际存在的源码。

## 概述

Lumina 是一款以 Vulkan 为基础、使用现代 C++（C++23 风格）构建的游戏引擎，包含反射系统、基于 ECS 的游戏逻辑层、模块化运行时，以及一个基于 ImGui 的编辑器（`README.md`）。它是一个正在积极开发的教育/实验项目，API 可能发生变化。

### 版本号

版本号来自 `Engine/Source/Runtime/Source/Lumina.h` 中的一组宏：

```cpp
#define LUMINA_VERSION_MAJOR 0
#define LUMINA_VERSION_MINOR 1
#define LUMINA_VERSION_PATCH 36

#define LUMINA_VERSION_TEXT_DETAIL(x) #x
#define LUMINA_VERSION_TEXT(x) LUMINA_VERSION_TEXT_DETAIL(x)
#define LUMINA_VERSION LUMINA_VERSION_TEXT(LUMINA_VERSION_MAJOR) "." LUMINA_VERSION_TEXT(LUMINA_VERSION_MINOR) "." LUMINA_VERSION_TEXT(LUMINA_VERSION_PATCH)
#define LUMINA_VERSION_NUM (LUMINA_VERSION_MAJOR * 10000 + LUMINA_VERSION_MINOR * 100 + LUMINA_VERSION_PATCH)
```

据此当前版本为 `0.1.36`，数值形式为 `136`。同一个头文件还定义了 `Lumina::Constants`（`kIndexNone`、`kCacheLineSize`、`kKiB` 等）和 `LUMINA_STATIC_HELPER`。仓库没有独立的 version 文件，`Lumina.h` 是版本号的唯一来源。

### 许可证

项目使用 Apache License 2.0，全文见 `LICENSE`。该文件是标准 Apache-2.0 文本，末尾的版权模板（`Copyright [yyyy] [name of copyright owner]`）未填写具体主体。

### 工具链约定

`Engine/Build/Lumina.BuildRules.cs` 里的引擎级默认规则设置 `CppStandard = "c++latest"`、`VectorExtensions = "AVX2"`、`bEnableRtti = false`、`bUseDynamicCrt = true`，并定义 `LE_DEBUG` / `LE_DEVELOPMENT` / `LE_SHIPPING` 与 `LUMINA_PLATFORM_NAME` 等全局宏。`README.md` 说明当前以 Windows + MSVC 为主目标，Linux 端依赖 GCC 16。

## 仓库布局

| 路径 | 职责 |
| --- | --- |
| `Engine/Source/Runtime` | 引擎运行时模块。`Runtime.Build.cs` 声明 module `Runtime`（`SharedLibrary`），对外公开 `Source` 作为 public include 根，其下包含 `Core/`、`World/`、`Renderer/`、`Memory/`、`TaskSystem/`、`Assets/`、`Physics/` 等（`Engine/Source/Runtime/Runtime.Build.cs`）。 |
| `Engine/Editor` | 编辑器模块。`Editor.Build.cs` 声明 module `Editor`，`HostType = ModuleHostType.Editor`，PCH 为 `EditorPCH.h`；源码在 `Engine/Editor/Source`（`Engine/Editor/Editor.Build.cs`）。 |
| `Engine/Resources/Shaders` | Slang shader 源码与 C++/C# 共享常量。`Shaders.Build.cs` 声明 `HeaderOnly` module `Shaders`，本身不编译任何东西，运行期由 Slang 编译器处理（`Engine/Resources/Shaders/Shaders.Build.cs`）。 |
| `Engine/Source/LuminaSharp` | 托管 C# 引擎 API（`net10.0`，`AssemblyName` 为 `LuminaSharp`），运行期由 .NET host 加载；`Engine/Source/LuminaSharp.Generators` 是对应的 Roslyn source generator/analyzer（`Engine/Source/LuminaSharp/LuminaSharp.csproj`）。 |
| `Engine/Source/ThirdParty` | 内置第三方库（`glfw`、`imgui`、`vulkan`、`volk`、`tracy`、`zstd`、`googletest` 等），每个子目录带自己的 `<Name>.Build.cs`。`Engine/Tools/LuminaBuildTool/README.md` 称其为受分层规则约束的第三方模块。 |
| `Engine/Applications` | 引擎自带的 target（应用/工具/示例）。每个子目录含 `<Name>.Build.cs` 与 `<Name>.Target.cs`，源码在 `Source/`。 |
| `Engine/Plugins` | 引擎自带插件。每个子目录含一个 `.lplugin` 描述符与 `Source/` 模块树。 |
| `Templates` | 项目与插件模板。`Templates/Blank` 是游戏项目模板，`Templates/Plugin` 是插件模板。 |
| `BuildScripts` | 构建与检查脚本，例如 `CheckPrerequisites.ps1`、`CheckIncludeCase.py`、`MemoryTrace.ps1`、`EnsureEngineBuilt.bat`。 |
| `Engine/Build` | 引擎级构建规则与特性开关：`Lumina.BuildRules.cs`、`LuminaFeatures.BuildRules.cs`、`LuminaGameProject.BuildRules.cs`、`BuildConfiguration.json`。 |
| `Engine/Tools/LuminaBuildTool` | 唯一的构建与项目生成系统（C#，取代了 Premake）。 |
| `Engine/Source/TestHarness` | 测试二进制共用的 main 与引擎启动逻辑（`Engine/Source/TestHarness/TestHarness.Build.cs`）。 |

## 模块与目标

### module 与 target 的区别

- **module** 是编译单元与依赖图节点，由 `<Name>.Build.cs` 描述，可声明 `BinaryType`（`SharedLibrary` / `StaticLibrary` / `WindowedApplication` / `ConsoleApplication` / `HeaderOnly`）、`HostType`、public/private 依赖、include 路径与定义。
- **target** 是一个可构建产物，由 `<Name>.Target.cs` 描述，声明 `Type`、`LaunchModuleName`、`ConfigurationOverride`、`bMonolithic`、`EnabledPlugins` 等，并通过 `PublicDependencyModuleNames` 拉起它需要的模块。

`Engine/Tools/LuminaBuildTool/README.md` 说明：规则文件会被编译并以 Roslyn 执行，工具据此解析模块依赖图、推导命令行、驱动 Reflector、做增量判断并生成 IDE 工程。规则发现扫描 `Engine/` 与项目树（`Engine/Tools/LuminaBuildTool/Rules/RulesScanner.cs`）。

public 设置会沿 `PublicDependencyModuleNames` 传递，private 设置只作用于声明它的模块。模块本身不可独立构建；IDE 中的 module 工程只用于浏览（`Engine/Tools/LuminaBuildTool/README.md`）。

### 引擎自带模块

| 模块 | 声明文件 | 说明 |
| --- | --- | --- |
| `Runtime` | `Engine/Source/Runtime/Runtime.Build.cs` | 运行时核心，`SharedLibrary`，PCH 为 `RuntimePCH.h`。 |
| `Editor` | `Engine/Editor/Editor.Build.cs` | 编辑器，`HostType = Editor`，因此不会出现在 Game target 中。 |
| `Shaders` | `Engine/Resources/Shaders/Shaders.Build.cs` | 仅把 shader 树放进 IDE 与 include 路径，不参与编译。 |
| `LuminaSharp` | `Engine/Source/LuminaSharp/LuminaSharp.csproj` | 托管程序集，不是 C++ module；由 `Lumina.Target.cs` 通过 `AddManagedProject(...)` 纳入构建（`Engine/Applications/Lumina/Lumina.Target.cs`）。 |

### 应用目标

`Engine/Applications/` 下的每个 `*.Target.cs` 对应一个 target。逐目录确认到的名字如下：

| target | `Type` | `LaunchModuleName` | 备注 |
| --- | --- | --- | --- |
| `Lumina` | `Target.Type`（Editor/Game/Server） | `Lumina` | 启动用目标；`PreBuildTargetNames` 含 `Reflector`，`ExtraModuleNames` 含 `Shaders`；`bIsStartupTarget = true`。Shipping 的 Game/Server 才 `bMonolithic`（`Engine/Applications/Lumina/Lumina.Target.cs`）。 |
| `Reflector` | `Program` | `Reflector` | 反射代码生成器；`Debug` 配置被 `ConfigurationOverride` 提升为 `Development`（`Engine/Applications/Reflector/Reflector.Target.cs`）。 |
| `LuminaLauncher` | `Program` | `LuminaLauncher` | 打包启动器；`bUseDynamicCrt = false`，`bBuildByDefault = false`（`Engine/Applications/LuminaLauncher/LuminaLauncher.Target.cs`）。 |
| `MinimalHost` | `Target.Type` | `MinimalHost` | 参考宿主；`bBuildByDefault = false`（`Engine/Applications/MinimalHost/MinimalHost.Target.cs`）。 |
| `RHITests` | `Target.Type` | `RHITests` | `bMonolithic = false`，`bBuildByDefault = false`（`Engine/Applications/RHITests/RHITests.Target.cs`）。 |
| `HelloTriangle` | `Target.Type` | `HelloTriangle` | `bMonolithic = false`，`bBuildByDefault = false`。 |
| `Breakout` | `Target.Type` | `Breakout` | `bMonolithic = false`，`bBuildByDefault = false`。 |
| `Grain` | `Target.Type` | `Grain` | `bMonolithic = false`，`bBuildByDefault = false`。 |
| `ECSBench` | `Target.Type` | `ECSBench` | `bMonolithic = false`，`bBuildByDefault = false`。 |
| `PhysicsBench` | `Target.Type` | `PhysicsBench` | `bMonolithic = false`，`bBuildByDefault = false`。 |
| `Umbral` | `Target.Type` | `Umbral` | `bMonolithic = false`，`bBuildByDefault = false`。 |

除 `Lumina` 应用外，各示例/测试目标在自己的源文件里提供进程入口：`Reflector` 与示例/测试程序定义 `int main(int, char**)`（例如 `Engine/Applications/MinimalHost/Source/MinimalHost.cpp`、`Engine/Applications/Reflector/Source/Reflector.cpp`），`LuminaLauncher` 在 Windows 上定义 `wWinMain`（`Engine/Applications/LuminaLauncher/Source/LuminaLauncher.cpp`）。只有 `Lumina` 应用走 `LuminaMain`（见下节）。

测试目标由构建工具从模块旁的 `Tests/` / `Benchmarks/` 目录合成，而不是手写 `*.Target.cs`：`TestSuite.SuiteName => ModuleName + Kind`（`Engine/Tools/LuminaBuildTool/Rules/TestSuite.cs`），例如 `Engine/Source/Runtime/Tests` 生成 `RuntimeTests`、`Engine/Editor/Tests` 生成 `EditorTests`、`Engine/Source/Runtime/Benchmarks` 生成 `RuntimeBenchmarks`。构建工具 README 的目标清单里写作 `Tests`，对应代码中实际合成的这些 `<Module>Tests` 目标（`Engine/Tools/LuminaBuildTool/README.md`）。

### 插件与 `.lplugin`

插件是一个包含 `.lplugin` JSON 描述符的目录，描述符是插件身份与模块列表的唯一来源（`Engine/Tools/LuminaBuildTool/README.md`）。运行期解析入口是 `FPluginDescriptor::LoadFromFile` / `LoadFromString`（`Engine/Source/Runtime/Source/Core/Plugin/PluginDescriptor.h`）。

描述符中的模块带 `Type`（`Runtime` / `Editor` / `Developer`）与 `LoadingPhase`，插件整体可带 `EnabledByDefault`、`EditorOnly`、`SupportedPlatforms`、`Dependencies`、`CookRoots`。以 `Engine/Plugins/Networking/Networking.lplugin` 为例，它声明 `NetworkingRuntime`（`Runtime`，`Earliest`）与 `NetworkingEditor`（`Editor`，`EditorInit`）两个模块；`Engine/Plugins/MCP/MCP.lplugin` 则是 `EnabledByDefault: false`、`EditorOnly: true`。

发现与加载由 `FPluginManager` 负责：`DiscoverEnginePlugins` 扫描 `Engine/Plugins/`，`DiscoverProjectPlugins` 扫描 `<Project>/Plugins/`，`LoadModulesForPhase` 按阶段加载（`Engine/Source/Runtime/Source/Core/Plugin/PluginManager.h`）。插件二进制写到插件自己的 `Binaries/<Platform>`（`Engine/Tools/LuminaBuildTool/README.md`）。

### `.Build.cs` / `.Target.cs`

模块规则文件是 `<Name>.Build.cs`，target 规则文件是 `<Name>.Target.cs`，共享辅助代码放在 `*.BuildRules.cs`（不会被实例化为模块）。类名必须与文件基名一致，可带可选的 `Target`/`Module` 后缀（`Engine/Tools/LuminaBuildTool/README.md`）。引擎范围默认值集中在 `Engine/Build/Lumina.BuildRules.cs`，其中还包含 `ForbidDependency("Runtime", "Editor", ...)` 之类的分层规则。

## 启动流程

下面是 `Lumina` 应用从进程入口到退出的真实调用链，括号内为入口文件。

1. **进程入口。** Windows 上是 `WinMain`（`Engine/Applications/Lumina/Source/Platform/LaunchWindows.cpp`），Linux 上是 `main`（`Engine/Applications/Lumina/Source/Platform/LaunchLinux.cpp`）。两者都先调用 `LuminaHasRequiredCpu()` 做 CPU 检查，失败则打印原因并返回 1，然后转发到 `LuminaMain(__argc, __argv)` / `LuminaMain(ArgC, ArgV)`。Linux 入口还会忽略 `SIGPIPE`。
2. **`LuminaMain`**（`Engine/Applications/Lumina/Source/Launch.cpp`）。初始化崩溃上报与 `HangWatchdog`；构造 `FApplicationGlobalState`；解析 `FCommandLine` 并设置 `GCommandLine`；构造 `FApplication` 并设置 `GApp`；构造 `FConfig` 并设置 `GConfig`；在 `WITH_EDITOR` 下构造 `FEditorEngine` 并设置 `GEditorEngine`/`GEngine`，否则构造 `FEngine` 并设置 `GEngine`。非 `-project` 的打包运行时通过 `FCoreDelegates::OnPreEngineInit` / `OnPostEngineInit` 挂载并启动 pak（`MountCookedRuntime` / `StartCookedGame`）。最后调用 `Application.Run`，返回后清空全局指针并关闭上报组件。
3. **`FApplication::Run`**（`Engine/Source/Runtime/Source/Core/Application/Application.cpp`）。先 `PreInitStartup()`，其中 `InitializeCObjectSystem()` 初始化反射/对象系统，`Paths::InitializePaths()` 解析引擎目录。非 headless 时 `CreateApplicationWindow()` 创建 `FWindow` 并把键盘、鼠标、文件拖放、关闭请求等事件转发到 `FEventProcessor`。
4. **引擎初始化。** 编辑器构建中 `GEngine` 是 `FEditorEngine`：它先挂载 `/Editor` VFS、加载 `/Editor/Config`，再调用 `FEngine::Init()`（`Engine/Editor/Source/LuminaEditor.cpp`）。`FEngine::Init` 见下一节。
5. **主循环。** `Run` 进入 `while(!bEngineWantsExit)`：`GEngine->MarkLoopStart()` → `WaitForFrameSlot()` → `MainWindow->ProcessMessages()` → `OnInputPumped.Broadcast()` → `ShouldExit()` → `FInputViewportRegistry::BeginFrame` → `GEngine->Update(...)` → `Benchmark::Tick` → `FInputViewportRegistry::EndFrame`（`Application.cpp`）。headless（`GIsHeadless`）跳过窗口与渲染相关分支。
6. **关闭。** 循环退出后调用 `GEngine->Shutdown()`，注销并释放 `PrimaryViewport`，再调用 `FApplication::Shutdown()`。回到 `LuminaMain` 后停止 `HangWatchdog`、关闭 `CrashHandler` 与 `CrashReporting`。

### `FEngine::Init` 的模块加载阶段

`FEngine::Init`（`Engine/Source/Runtime/Source/Core/Engine/Engine.cpp`）按固定顺序推进，插件的模块在各阶段被载入：

1. `FPluginManager::Get().DiscoverEnginePlugins()`，随后（若有启动项目）`DiscoverProjectPlugins` 与 `PreloadProjectPluginOverrides`。
2. `LoadModulesForPhase(EPluginLoadingPhase::Earliest)`。
3. 挂载 `/Engine`、`/Intermediates`，并按需 `MountProject(StartupProject)`；广播 `OnPreEngineInit`。
4. 加载 console variables、`Task::Initialize()`、网络与物理子系统初始化。
5. `LoadModulesForPhase(EPluginLoadingPhase::Core)`。
6. 并行初始化 basisu 编码器、`DotNet::Initialize()`、音频与渲染器（`Render().Initialize()`、`FTextureStreamingManager::Initialize()`），然后 join 这些线程并 `ProcessNewlyLoadedCObjects()`。
7. `LoadModulesForPhase(EPluginLoadingPhase::PreEngineInit)`，再次 `ProcessNewlyLoadedCObjects()`。
8. 创建 `CPrimitiveManager`、`CFontManager`、`FWorldManager`。
9. `LoadModulesForPhase(EPluginLoadingPhase::EngineInit)`，`ProcessNewlyLoadedCObjects()`；若已挂载项目则 `StartProject()`。
10. 编辑器构建：`DiscoverAndLoadSettings()`、创建并初始化 `DeveloperToolUI`、`LoadModulesForPhase(EPluginLoadingPhase::EditorInit)`。
11. `RmlUi::Initialize()`（非 headless）。
12. `LoadModulesForPhase(EPluginLoadingPhase::PostEngineInit)`，广播 `OnPostEngineInit`。

### `FEngine::Shutdown` 的顺序

`FEngine::Shutdown`（`Engine/Source/Runtime/Source/Core/Engine/Engine.cpp`）大致按相反顺序收尾：广播 `OnEngineShutdownStarted` / `OnPreEngineShutdown` → `Jobs::WaitForAll()` → `RHI::WaitDeviceIdle()`、`RmlUi::Shutdown()` → 反初始化并释放 `DeveloperToolUI` → `DestroyGameInstance()` → 释放 `FWorldManager` → `Jobs::WaitForAll()` → `FTextureStreamingManager::Shutdown()` → `ShutdownCObjectSystem()` → `DotNet::Shutdown()` → 释放 `FRenderManager` → `Physics::Shutdown()`、`Audio::Shutdown()`、网络与 `Task::Shutdown()` → `FPluginManager::ShutdownAllPlugins()`、`FModuleManager::UnloadAllModules()`。

## 模块系统

### 模块基类与回调

`IModuleInterface` 是所有模块的基类，只有两个默认空实现的回调（`Engine/Source/Runtime/Source/Core/Module/ModuleInterface.h`）：

```cpp
class RUNTIME_API IModuleInterface
{
public:
    IModuleInterface() = default;
    virtual ~IModuleInterface() = default;
    LE_NO_COPYMOVE(IModuleInterface);

    /** Called after the DLL has been loaded and module object has been created. */
    virtual void StartupModule() { }

    /** Called before the module is unloaded, right before the module object is destroyed */
    virtual void ShutdownModule() { }
};
```

引擎运行时的模块实现是 `FLuminaModule`（`Engine/Source/Runtime/Source/Module/LuminaModule.h`、`Engine/Source/Runtime/Source/Module/LuminaModule.cpp`）；编辑器模块是 `FEditorModule`（`Engine/Editor/Source/Module/EditorModule.h`）。

### `IMPLEMENT_MODULE`

`IMPLEMENT_MODULE(ModuleClass, ModuleName)` 有两种形态，取决于是否定义了 `LUMINA_MONOLITHIC`（`Engine/Source/Runtime/Source/Core/Module/ModuleManager.h`）。非单体构建导出两个 C 符号：

```cpp
#define IMPLEMENT_MODULE(ModuleClass, ModuleName)                                           \
    extern "C" DLL_EXPORT const char* LuminaModuleABISignature()                            \
    {                                                                                       \
        return LUMINA_MODULE_ABI_SIGNATURE;                                                 \
    }                                                                                       \
    extern "C" DLL_EXPORT Lumina::IModuleInterface* InitializeModule()                      \
    {                                                                                       \
        Lumina::Memory::InitializeThreadHeap();                                             \
        return Lumina::Memory::New<ModuleClass>();                                          \
    }                                                                                       \
    extern "C" DLL_EXPORT void ShutdownModule()                                             \
    {                                                                                       \
        Lumina::Memory::ShutdownThreadHeap();                                               \
    }                                                                                       \
    static_assert(true, "IMPLEMENT_MODULE consumes the semicolon at the call site")
```

单体构建则改为在匿名命名空间里注册一个 `FStaticModuleRegistration`，由 `FModuleManager` 在处理查找时一次性排空（`ModuleManager.h`）。`FModuleManager::LoadModule` 会先校验模块导出的 ABI 签名，再做 `InitializeModule()`、`StartupModule()`，最后广播 `FCoreDelegates::OnModuleLoaded`（`Engine/Source/Runtime/Source/Core/Module/ModuleManager.cpp`）。ABI 版本当前为 `LUMINA_MODULE_ABI_VERSION 29`，签名里还编码了二进制后缀、Editor/Game/Server 平台、编译器与 STL。

### 加载阶段

插件模块的加载阶段枚举是 `EPluginLoadingPhase`（`Engine/Source/Runtime/Source/Core/Plugin/PluginLoadingPhase.h`），按声明顺序执行，同一阶段内按依赖顺序：

| 值 | 语义（源码注释） |
| --- | --- |
| `Earliest` | 在任何引擎子系统之前；留给包装引擎自身依赖的第三方库的模块；引擎专用。 |
| `Core` | 在 core（memory/log/task/physics）之后、render/Lua/world 之前。 |
| `PreEngineInit` | 在 Lua + 反射（`ProcessNewlyLoadedCObjects`）之后、任何 world 或编辑器 UI 之前；大多数 gameplay 扩展放这里。 |
| `EngineInit` | 在 world manager 存在之后、项目 DLL 加载之前。 |
| `PostEngineInit` | 所有引擎初始化完成之后、主循环之前。 |
| `PostProjectLoad` | 在项目 DLL 加载之后，供依赖项目类型的模块使用。 |
| `EditorInit` | 仅编辑器：编辑器 UI 初始化之后；非编辑器构建整体跳过。 |
| `Count` | 哨兵。 |

> [!NOTE]
> 上表中 `Core` 与 `PreEngineInit` 的注释提到了 Lua，但**本仓库没有任何 Lua 运行时**：`Engine/Source/ThirdParty` 下没有 Lua/sol2，全仓库对 `lua_State`/`sol::` 的代码级引用只有 `Reflector/Types/Properties/ReflectedStringProperty.cpp` 里残留的一行生成文本。这些注释是历史遗留。当前唯一的脚本后端是 C#（`LuminaSharp`，见 [ReflectionSystem.md](ReflectionSystem.md) 第 9 节）。

字符串解析由 `LexToString` / `ParsePluginLoadingPhase` 提供。

### 导出宏 `<NAME>_API` 的来源

`Engine/Source/Runtime/Source/ModuleAPI.h` 明确说明：每个模块的导出宏**不**在该头文件中声明，而是由 LuminaBuildTool 在命令行上定义：

```cpp
// The per-module export macros are NOT declared here. LuminaBuildTool defines <NAME>_API on the
// command line for the module being compiled and for every shared library it can see, working out
// dllexport vs dllimport from the dependency graph. That is what lets a game or plugin module have
// one at all: a list in this header could only ever name the modules that ship with the engine, and
// nothing out of tree can add itself to it.
```

对应实现位于 `Engine/Tools/LuminaBuildTool/Graph/TargetAssembler.cs`：遍历模块的依赖闭包，为每个 `SharedLibrary` 模块生成 `<NAME>_API=DLL_EXPORT`（自身）或 `=DLL_IMPORT`（依赖方），单体链接下置空。`DLL_EXPORT` / `DLL_IMPORT` 由平台工具链定义（`Engine/Tools/LuminaBuildTool/Platform/Windows/WindowsPlatform.cs` 为 `__declspec(dllexport/dllimport)`，Linux 为 visibility 属性）。`ModuleAPI.h` 里唯一保留的是 `LUMINA_SCRIPT_API`——C#↔native 互操作 thunk 按名字在运行期解析，因此任何构建模式下都必须是 `DLL_EXPORT`。

## 主循环与更新阶段

### `FUpdateContext`

每帧上下文是 `FUpdateContext`（`Engine/Source/Runtime/Source/Core/UpdateContext.h`），关键字段与访问器：

```cpp
double       Time = 0;                 // 帧结束时的引擎时间
double       FrameStart = 0;
double       DeltaTime = 1.0 / 60.0;
float        FrameRateLimit = 144.0f;
uint64       Frame = 0;
EUpdateStage UpdateStage = EUpdateStage::Max;
```

`MarkFrameStart(Now)` 由 `FEngine::MarkLoopStart` 调用，用相邻帧起点之差更新 `DeltaTime`；`MarkFrameEnd(Time)` 递增 `Frame` 并记录时间。`GetDeltaTime()`、`GetFrame()`、`GetUpdateStage()`、`GetFPS()` 供系统读取。

### 阶段枚举

`EUpdateStage`（`Engine/Source/Runtime/Source/Core/UpdateStage.h`）：

```cpp
REFLECT()
enum class EUpdateStage : uint8
{
    FrameStart,
    PrePhysics,
    DuringPhysics,
    PostPhysics,
    FrameEnd,
    Paused,
    Max,
};
```

注意：枚举的声明顺序（`Paused` 在 `FrameEnd` 之后）与每帧实际执行顺序不同。配套的 `EUpdatePriority` 为 `Highest = 0`、`High = 64`、`Medium = 128`、`Low = 192`、`Disabled = 255`、`Default = Medium`；数值越小越先执行，`FUpdatePriorityList` 为每个系统保存逐 stage 的优先级，`Disabled` 表示该系统不参与该 stage（`UpdateStage.h`）。

### 每帧执行顺序

`FEngine::Update`（`Engine/Source/Runtime/Source/Core/Engine/Engine.cpp`）在窗口未最小化时依次执行：

| 顺序 | `EUpdateStage` | 该块内的关键调用 |
| --- | --- | --- |
| 1 | `FrameStart` | `ProcessPendingOpenLevel`、`ProcessPendingTravel`、缓存裁剪倒计时、`DotNet::ProcessPendingScriptReload`、`Render().FrameStart`、`FTextureStreamingManager::Update`、编辑器 `DeveloperToolUI::StartFrame/Update`、`GWorldManager->ReclaimIdleRenderers`、`GWorldManager->BeginFrame`、`GWorldManager->UpdateWorlds`、`OnUpdateStage` |
| 2 | `Paused` | 编辑器 UI `Update`、`GWorldManager->UpdateWorlds`、`OnUpdateStage` |
| 3 | `PrePhysics` | 同上 |
| 4 | `DuringPhysics` | 同上 |
| 5 | （无 stage） | `GWorldManager->TickPhysics()`，同步步进物理并分发接触事件 |
| 6 | `PostPhysics` | 编辑器 UI `Update`、`GWorldManager->UpdateWorlds`、`OnUpdateStage` |
| 7 | `FrameEnd` | 编辑器 UI `Update`、`GWorldManager->UpdateWorlds`、`DeveloperToolUI::EndFrame`、`RmlUi::TickEditorContexts`、`GWorldManager->ExtractWorlds`、`BeginImmediateLines`、`Render().FrameEnd`、`DotNet::Tick`、`OnUpdateStage` |

`OnUpdateStage` 是 `FEngine` 上的虚函数，基类实现为空（`Engine/Source/Runtime/Source/Core/Engine/Engine.h` / `Engine.cpp`），作为子类钩子每阶段调用一次。`GWorldManager::UpdateWorlds` 遍历所有 world，只更新 `IsTickingThisFrame()` 为真的 world（`Engine/Source/Runtime/Source/World/WorldManager.cpp`）；`CWorld::Update` 根据 `Context.GetUpdateStage()` 派发系统（`Engine/Source/Runtime/Source/World/World.h`）。每帧结束时 `UpdateContext.MarkFrameEnd`，随后按 `GetMaxFrameRate()` 做帧率限制（先 `SleepPrecise` 再自旋，`Engine/Source/Runtime/Source/Core/Engine/Engine.cpp`）。

### 固定步长

不存在全局固定步长的世界更新。相关逻辑有两处：

- `MarkLoopStart` 读取 console variable `CVarFixedDeltaTime`（`Core.FixedDeltaTime`），当其大于 0 时直接把 `UpdateContext.DeltaTime` 覆盖为该值，用来让每帧游戏时间固定（`Engine/Source/Runtime/Source/Core/Engine/Engine.cpp`）。
- `SEntityScriptSystem` 用 accumulator 驱动脚本 `OnFixedUpdate`：固定步长为 `1 / SDefaultWorldSettings.PhysicsHz`（默认 `60.0f`），步数上限为 `MaxPhysicsSteps`（默认 8），并对 accumulator 做“死亡螺旋”钳制；当 world 有 physics scene 时改由物理步进驱动，脚本固定更新不重复跑（`Engine/Source/Runtime/Source/World/Entity/Systems/EntityScriptSystem.cpp`、`Engine/Source/Runtime/Source/World/Subsystems/WorldSettings.h`）。

## 核心子系统一览

| 子系统 | 入口头文件 | 职责 |
| --- | --- | --- |
| Object 与反射 | `Engine/Source/Runtime/Source/Core/Object/Object.h`、`Engine/Source/Runtime/Source/Core/Object/ObjectMacros.h`、`Engine/Source/Runtime/Source/Core/Object/Class.h`、`Engine/Source/Runtime/Source/Core/Reflection/ReflectionMacros.h`、`Engine/Source/Runtime/Source/Core/Reflection/Type/LuminaTypes.h` | `CObject` 是所有可反射类型的基类，路径格式为 `<package-path>.<object-name>`；`DECLARE_CLASS` / `DEFINE_CLASS_FACTORY` / `IMPLEMENT_CLASS` 生成类型信息，`REFLECT`/`PROPERTY`/`FUNCTION`/`GENERATED_BODY` 标注可反射成员；`CClass`（`Class.h`）、`FProperty`（`LuminaTypes.h`）及其 `ArrayProperty`/`ClassProperty`/`EnumProperty` 等派生（`Core/Reflection/Type/Properties/`）描述成员。 |
| 内存 | `Engine/Source/Runtime/Source/Memory/Memory.h` | 基于 rpmalloc 的 `Memory::FMalloc`，提供 `Malloc`/`Realloc`/`Free`、`Memory::New`/`Delete`、`Memzero`/`Memset`/`MemEqual`/`Prefetch` 等；另有 `MemoryTracking.h` 与 leak detection。 |
| TaskSystem（fiber） | `Engine/Source/Runtime/Source/TaskSystem/TaskSystem.h`、`Engine/Source/Runtime/Source/TaskSystem/Scheduler/JobScheduler.h`、`Engine/Source/Runtime/Source/Core/Threading/Fiber.h` | fiber 调度器，带基于 counter 的依赖；等待中的 job 不会阻塞 worker，而是 park 自己的 fiber 稍后恢复；提供 `Task::Initialize`/`Shutdown`、`FTaskSystem`、`ParallelFor`、`FTaskGraph`。 |
| 文件系统与路径 | `Engine/Source/Runtime/Source/FileSystem/FileSystem.h`、`Engine/Source/Runtime/Source/Paths/Paths.h` | `VFS` 别名下可挂载多种 `IFileSystem`（`FNativeFileSystem`、`FMemoryFileSystem`、`FPakFileSystem`），提供 `ReadFile`/`WriteFile`/`AtomicWriteFile`、目录遍历与路径工具；`Paths` 解析引擎目录、资源/字体/配置/shaders 目录与模块文件名。 |
| 日志 | `Engine/Source/Runtime/Source/Log/Log.h` | `Logging::Init`/`Shutdown`/`Flush`，`ELogLevel` 阈值、`LOG_CRITICAL`/`ERROR`/`WARN`/`DISPLAY`/`INFO` 等宏，console 环形缓冲队列与 sink（`Log/Sinks`）。 |
| 配置 | `Engine/Source/Runtime/Source/Config/Config.h` | `FConfig` 基于 nlohmann/json 加载 VFS 下的 JSON；发现并加载 `CDeveloperSettings` 子类的 CDO，支持 `LoadPath`、`SaveSettings`、`DiscoverAndLoadSettings`。 |
| 事件与委托 | `Engine/Source/Runtime/Source/Events/EventProcessor.h`、`Engine/Source/Runtime/Source/Core/Delegates/Delegate.h`、`Engine/Source/Runtime/Source/Core/Delegates/CoreDelegates.h` | `FEventProcessor` + `IEventHandler` 按 `EInputLayer` 优先级分发，首个返回 true 的 handler 停止传播；`TBaseDelegate`/multicast delegate 提供静态、成员与 lambda 绑定；`FCoreDelegates` 暴露引擎生命周期事件。 |
| 序列化 | `Engine/Source/Runtime/Source/Core/Serialization/Archiver.h` | `FArchive` 支持 reading/writing、compress、encrypt、cooking 等标志，`FBulkDataRef` 描述包内 bulk-data 区域；`ObjectArchiver` 负责对象序列化，`Structured` 提供 structured archive，`Package` 下是包/资产存储。 |
| 平台抽象 | `Engine/Source/Runtime/Source/Platform/GenericPlatform.h`、`Engine/Source/Runtime/Source/Platform/Platform.h` | 统一 `uint8/uint32/int64`、`TCHAR`、`TEXT()` 等类型与 `FORCEINLINE`、`NODISCARD`、`LIKELY`/`UNLIKELY`、`PLATFORM_WIDECHAR_SIZE` 等宏，屏蔽 MSVC / GCC / Clang 差异。 |
| 调试 | `Engine/Source/Runtime/Source/Core/Assertions/Assert.h`、`Engine/Source/Runtime/Source/Core/Diagnostics/HangWatchdog.h`、`Engine/Source/Runtime/Source/Platform/CrashHandler.h` | 断言类型 `EAssertionType` 与可替换的 `AssertionHandler`、`LUMINA_DEBUG_BREAK`；`HangWatchdog` 检测主线程停滞并转储各线程栈；`CrashHandler`/`CrashReporter` 负责崩溃捕获与上传。 |
| 命令与进度 | `Engine/Source/Runtime/Source/Core/CommandLine/CommandLine.h`、`Engine/Source/Runtime/Source/Core/Progress/SlowTask.h` | `FCommandLine` 解析 `-key[=value]` 与位置参数；`FScopedSlowTask` 是 RAII 进度上报器，编辑器为每个存活实例渲染模态进度。 |
| 单例与线程 | `Engine/Source/Runtime/Source/Core/Singleton/Singleton.h`、`Engine/Source/Runtime/Source/Core/Threading/Thread.h` | `TSingleton<T>` 提供 `Get()`；线程层包含 `FThread`、原子与平台同步原语。 |
| 应用与引擎 | `Engine/Source/Runtime/Source/Core/Application/Application.h`、`Engine/Source/Runtime/Source/Core/Engine/Engine.h` | `FApplication` 负责窗口、事件与主循环；`FEngine` 负责初始化、每帧更新与关闭，编辑器由 `FEditorEngine` 派生实现。 |

## 平台抽象

- 基础类型与宏：`Engine/Source/Runtime/Source/Platform/GenericPlatform.h` 定义引擎统一的整型别名（`uint8`/`uint16`/`uint32`/`uint64`、`int8`…）、`ANSICHAR`/`WIDECHAR`/`UTF8CHAR`/`UTF16CHAR`/`UTF32CHAR`，并按平台把 `TCHAR` 定义为 `WIDECHAR`（Windows）或 `ANSICHAR`（其他），同时提供 `LUMINA_VISIBLE_TYPE`、`TEXT()`、`PLATFORM_WIDECHAR_SIZE`。
- 通用属性宏：`Engine/Source/Runtime/Source/Platform/Platform.h` 提供 `LIKELY`/`UNLIKELY`、`FORCEINLINE`、`FORCENOINLINE`、`NODISCARD`、`ALIGN`、`RESTRICT`、`LE_CONSTEVAL`、`LUMINA_DISABLE_OPTIMIZATION` 等，并针对 `REFLECTION_PARSER` 分支提供空实现，避免 libclang 解析期被属性干扰。
- 平台选择：`LE_PLATFORM_WINDOWS` / `LE_PLATFORM_LINUX` 由构建工具链写入编译定义，而不是头文件自行推断（`Engine/Tools/LuminaBuildTool/Platform/Windows/WindowsPlatform.cs`、`Engine/Tools/LuminaBuildTool/Platform/Linux/LinuxPlatform.cs`）。
- 平台实现的位置：通用接口放在 `Engine/Source/Runtime/Source/Platform/` 下的 `Time/PlatformTime.h`、`Process/PlatformProcess.h`、`Filesystem/`、`Socket/` 等；具体实现放在同目录的 `Windows/`（`WindowsPlatformProcess.cpp`、`WindowsPlatformTime.cpp`、`WindowsPlatformSocket.cpp`、`WindowsPlatformString.cpp`、`WindowsCrashHandler.cpp`、`WindowsPlatformPty.cpp`）和 `Linux/`（`LinuxPlatformProcess.cpp`、`LinuxPlatformTime.cpp`、`LinuxPlatformSocket.cpp`、`LinuxCrashHandler.cpp`、`LinuxPlatformPty.cpp`）。`PlatformProcess.h` 的 `GetDLLHandle`/`FreeDLLHandle`/`GetDLLExport` 正是模块加载器跨平台加载 DLL 的入口（`Engine/Source/Runtime/Source/Core/Module/ModuleManager.cpp`）。
- 运行时镜像的线程堆初始化按平台分开：Windows 在 `Engine/Source/Runtime/Source/DLLMain.cpp` 的 `DllMain` 里处理 `DLL_PROCESS_ATTACH`/`DLL_THREAD_ATTACH`，Linux 用 `__attribute__((constructor/destructor))` 注册 `LuminaRuntimeImageAttach`/`Detach`。

## 扩展点

### 加一个模块

1. 新建一个包含 `<Name>.Build.cs` 的目录，类名与文件基名一致并继承 `LuminaModuleRules`（引擎内）或 `LuminaGameModuleRules`（游戏项目），按需设置 `BinaryType`、`PublicIncludePaths`、`PublicDependencyModuleNames` / `PrivateDependencyModuleNames`、`PrivateDefinitions`。可见 `Engine/Source/Runtime/Runtime.Build.cs`、`Engine/Editor/Editor.Build.cs` 作为范例。
2. 模块的源文件由构建工具遍历模块目录自动发现；子目录里若另有 `Build.cs` 则属于另一个模块，不会被抢占。
3. 若模块需要在启动时执行代码，在源文件中实现一个 `IModuleInterface` 派生类并用 `IMPLEMENT_MODULE(Class, "Name")` 注册，参照 `Engine/Source/Runtime/Source/Module/LuminaModule.cpp` 与 `Templates/Blank/Source/$PROJECTNAMEModule.cpp`。
4. 把该模块加入某个 target 的依赖（`PublicDependencyModuleNames` / `PrivateDependencyModuleNames`）后才会被构建。新增/删除模块后需要重新运行 `GenerateProjectFiles` 刷新工程与 `compile_commands.json`（`Engine/Tools/LuminaBuildTool/README.md`）。

### 加一个插件

1. 在 `Engine/Plugins/` 或 `<Project>/Plugins/` 下建目录，写入 `<Name>.lplugin` JSON 描述符。字段含义见 `Engine/Source/Runtime/Source/Core/Plugin/PluginDescriptor.h`；模板见 `Templates/Plugin/$PLUGINNAME.lplugin`。
2. 在插件的 `Source/` 下放一个或多个模块目录，每个模块都要有匹配的 `<Module>.Build.cs`。描述符的 `Modules[].Type`（`Runtime`/`Editor`/`Developer`）与 `Modules[].LoadingPhase` 决定它在哪种 target、哪个阶段被加载；`EnabledByDefault`、`EditorOnly`、`SupportedPlatforms`、`Dependencies` 控制适用性。
3. 插件二进制写到插件自己的 `Binaries/<Platform>`，运行时由 `FPluginManager::DiscoverDirectory` 找到后用 `FModuleManager::LoadModule` 加载（`PluginManager.cpp`）。可参考 `Engine/Plugins/Networking/`（Runtime + Editor 两模块）与 `Engine/Plugins/GameplayExtras/`。
4. 编辑器内还提供 `FEditorEngine::CreatePlugin(...)`，从 Plugin 模板脚手架生成项目级插件（`Engine/Editor/Source/LuminaEditor.h`）。

### 加一个独立应用

1. 在 `Engine/Applications/<Name>/` 下建 `<Name>.Build.cs`（继承 `LuminaModuleRules`，通常 `BinaryType = ConsoleApplication` 或 `WindowedApplication`）与 `<Name>.Target.cs`（继承 `LuminaTargetRules`，设置 `Type` 与 `LaunchModuleName`）。范例：`Engine/Applications/MinimalHost/MinimalHost.Build.cs` / `MinimalHost.Target.cs`、`Engine/Applications/Reflector/Reflector.Target.cs`。
2. 在 `Source/` 下提供 `int main(int, char**)`。若要在编辑器/游戏宿主结构内启动完整引擎，可参照 `Lumina` 应用的 `LuminaMain`（`Engine/Applications/Lumina/Source/Launch.cpp`）；示例应用通常直接使用 `Runtime` 的公共 API，见 `Engine/Applications/MinimalHost/Source/MinimalHost.cpp`。
3. 规则扫描器会自动发现 `Engine/` 下的 `*.Target.cs`（`Engine/Tools/LuminaBuildTool/Rules/RulesScanner.cs`），target 名称即文件基名去掉 `.Target`（`RulesFile.DeclaredName`）。
4. 若要加测试套件，只需在某个模块的 `Build.cs` 旁放置 `Tests/`（或 `Benchmarks/`）目录并放入 `.cpp`，构建工具会合成 `<Module>Tests` / `<Module>Benchmarks` target，用 `TestHarness` 提供 main（`Engine/Tools/LuminaBuildTool/Rules/TestSuite.cs`、`Engine/Source/TestHarness/TestHarness.Build.cs`）。

## 相关文档

- [渲染架构](RenderingArchitecture.md)
- [ECS 指南](ECSGuide.md)
- [反射系统](ReflectionSystem.md)
- [贡献指南](../CONTRIBUTING.md)
- [LuminaBuildTool](../Engine/Tools/LuminaBuildTool/README.md)
