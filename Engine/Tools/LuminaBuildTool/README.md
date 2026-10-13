# LuminaBuildTool

Lumina Engine 的构建与项目生成系统。它是引擎如何配置、生成和编译的唯一权威；Premake 已被移除。

模块和目标由 C# 规则文件（`*.Build.cs`、`*.Target.cs`）描述，该工具会编译并执行这些文件。
工具据此解析模块依赖图、推导编译与链接命令行、运行反射生成器、判断哪些内容已过期、
运行工具链，并写出 IDE 项目文件。

## 运行方式

引擎根目录下的 `LuminaBuild.bat` 是前端入口。它会在需要时重建该工具，并把其余内容转发过去：

```bash
LuminaBuild.bat Build Lumina -TargetType=Editor -Configuration=Development
```

| 脚本 | 用途 |
| --- | --- |
| `Setup.bat` | 首次安装：先决条件、依赖包、项目文件 |
| `GenerateProjectFiles.bat` | 刷新 IDE 解决方案 |
| `LuminaBuild.bat` | 其他所有操作，转发给该工具 |

模式：

| 模式 | 用途 |
| --- | --- |
| `Setup` | 获取并校验外部依赖包、持久化 LUMINA_DIR、设置 git hooks |
| `Build <Target>` | 编译并链接一个目标 |
| `Clean [Target]` | 删除某个目标的产物，或删除全部中间产物 |
| `Query [Target]` | 列出目标、模块和插件，或描述某个已解析的目标 |
| `Includes <Target>` | 按包含该头文件的翻译单元数量对头文件排名 |
| `Deps <Target>` | 将模块声明的依赖与它实际触及的依赖进行对比 |
| `GenerateProjectFiles` | 写出 `.vcxproj`、`.filters`、解决方案以及 `compile_commands.json` |

目标：`Lumina`（编辑器与游戏启动器）、`Reflector`（反射生成器）
以及 `Tests`。游戏项目位于引擎目录树之外，通过 `-Project=<path>` 传入。

常用选项：`-Configuration=Debug|Development|Shipping`、`-TargetType=Editor|Game|Program`、
`-Platform=Win64`、`-EngineRoot=<path>`、`-Project=<path>`、`-MaxParallel=<n>`、`-DryRun`、
`-Clean`、`-KeepGoing`、`-RecompileRules`、`-Verbose`、`-Trace`、`-Timeline`、
`-NoProjectFileUpdate`。

## 度量构建

`-Timeline` 会把 Chrome Trace Event 文件写到
`Intermediates/BuildTool/Timeline-<Target>-<Type>-<Configuration>.json`，该文件可在 Perfetto 或
`chrome://tracing` 中打开，同时还会记录耗时最长的十个操作。span 只覆盖执行时间，不包含等待并行
槽位的时间，因此图上出现的空隙是真正的空闲而非排队。查看器显示的 lane 数量就是该构建实际达到的
并行度，这个数字值得与它被允许的并行度进行对比。

`Includes` 和 `Deps` 会读取编译器在最近一次构建中报告的头部闭包，LuminaBuildTool 本来就保存了
这份数据，以便编辑某个头文件时只重建真正读取了它的那些对象。
两者都要求该目标已经被构建过一次；它们自身都不编译任何东西。

```bash
LuminaBuild.bat Includes Lumina -Top=30
LuminaBuild.bat Includes Lumina -Module=Runtime
LuminaBuild.bat Deps Lumina
```

`Includes` 会先把排名按拥有该头文件的模块汇总，然后再列出单个文件，因为你需要做判断的单位是库。
排名靠前且位于预编译头之外的头文件，是可获得的最廉价的干净构建收益；排名靠后且位于某个 PCH 之内
的头文件，则是每个翻译单元都要付出、却几乎无人使用的负担。`-All` 会纳入工具链和 SDK 头文件，
它们默认被排除，因为它们会主导排名且无法更改。

`Deps` 会报告两件事。模块从未打开其头文件的已声明依赖是可供删除的候选，但并非已确认要删：
某个依赖可能纯粹是为了链接而存在。从某个未声明模块触及的头文件之所以今天还能编译，只是因为
其他东西重新导出了它们，报告会指出它们经由哪个模块到达，因为正是这条边在被移除时会破坏构建。
注意，像 `EASTLImpl.cpp` 这样按 image 编译的源文件会被编译进每个模块，其闭包会被计入编译它的
那个模块，这解释了那些没有指出重新导出模块的条目。

## 模块分层

每个解析出的依赖图都会在任何编译或链接环境建立之前，按规则声明的分层进行检查。
所有违规会一次性全部报告，因为架构漂移通常是一批同时到来的边。

有两项检查无需声明。第三方模块不得依赖第一方模块：内置的代码必须能独立存在，这样它才能被自身的
下一个版本替换，而一条指回引擎的边会把这次更新变成一次合并。此外，某个目标自身的
`ForbidDependency` 规则会在整个闭包上检查，因此把一条被禁止的边经由中间模块绕过去也无法规避它；
错误信息会指出它找到的最短路径。

```csharp
ForbidDependency(
    "Runtime",
    "Editor",
    "The runtime is what ships. An editor dependency here cannot link in a Game target.");
```

原因是必填的，并且会被原样引用回错误信息中。一旦添加规则的人不再回答相关问题，一条没有原因的
分层规则就会变成口口相传的传说。

宿主类型被有意**不**做检查。“依赖必须存在于其被依赖者存在的所有地方”听起来像是一条显而易见
的规则，但它并不成立：`Build.cs` 是按目标类型分别求值的，因此模块可以在
`Target.bWithEditor` 判断内部写明一个编辑器依赖，而这条边在 Game 解析中根本不存在。
`Lumina` 正是这么做的。排除条件边之后剩下的，只是当前正在解析的目标缺少某个依赖，
而模块解析已经会用更好的信息拒绝这种情况。

如果某个目标确实需要按照它自己声明的分层来构建，可在其规则中设置
`bEnforceModuleLayering = false`。故意没有对应的命令行开关：任何构建都能挥手绕过的守卫，
在第一次“挥手绕过比修复依赖更快”之后就不再是守卫了。

## 编译数据库

项目生成还会在工作区根目录写出 `compile_commands.json`，clangd、clang-tidy 以及 Visual Studio
之外的编辑器都会在这里查找它。这些命令来自构建所使用的同一次工具链调用，因此工具看到的内容与
编译器看到的一致。

与真实命令行有两处不同，都与预编译头有关。Clang 无法读取 MSVC 的 `.pch`，因此 `/Yc` 和 `/Yu`
会变成对同一个头文件的强制包含，而 `/Fp` 被丢弃：相同的声明会经由不需要该工具无法解析的二进制
文件的途径到达。`/sourceDependencies` 被丢弃，因为它要求的是一个构建产物，而读取代码并不是构建。
条目按文件而非按 unity blob 划分，因为 blob 的命令行并未指出它吸收了哪些源文件。

该文件是生成产物、与机器相关，并且被 git 忽略。

## 编写规则

模块是一个包含 `<Name>.Build.cs` 的目录：

```csharp
using LuminaBuildTool.Configuration;

public class Renderer : LuminaModuleRules
{
    public Renderer(TargetInfo Target)
        : base(Target)
    {
        BinaryType = ModuleBinaryType.SharedLibrary;

        // Exposed through this module's own public headers, so dependents see them too.
        PublicDependencyModuleNames.Add("Runtime");

        // Implementation only; stops here.
        PrivateDependencyModuleNames.AddRange(new[] { "Volk", "VMA" });

        PublicIncludePaths.Add("Public");
        PrivateDefinitions.Add("LUMINA_RENDERER_VULKAN");

        if (Target.bWithEditor)
        {
            PrivateDependencyModuleNames.Add("Editor");
        }
    }
}
```

目标是 `<Name>.Target.cs`：

```csharp
public class LuminaTarget : LuminaTargetRules
{
    public LuminaTarget(TargetInfo Target)
        : base(Target)
    {
        Type = Target.Type;
        LaunchModuleName = "Lumina";
        EnabledPlugins.Add("GameplayExtras");
    }
}
```

类名必须与文件的基名一致，可以带可选的 `Target` 或 `Module` 后缀。
构造函数接收单个 `TargetInfo`，并可调用 `ModulePath()` / `TargetPath()` 来解析相对于规则文件的路径。

共享的辅助代码放在 `*.BuildRules.cs` 文件中。它会被编译进规则程序集，但绝不会被实例化为模块。
`Engine/Build/Lumina.BuildRules.cs` 存放引擎范围的默认目标设置，以及 `LuminaModuleRules` /
`LuminaThirdPartyModuleRules` 基类。

### public 与 private

public 设置会通过 `PublicDependencyModuleNames` 传递性地传播。private 设置仅作用于声明它的模块。
凡是模块自身头文件对外暴露的内容都用 public，其余一律用 private；正是这种划分防止了 include 路径
在整个依赖图中泄漏。

### 插件

插件是一个包含 `.lplugin` 描述符的目录。该描述符是插件身份及其模块列表的唯一真实来源，
列出的每个模块都需要在插件的 `Source` 目录树下有匹配的 `Build.cs`。插件二进制会被写到插件自己的
`Binaries/<Platform>` 目录，以便运行时插件加载器能在它预期的位置找到它们。

当描述符中设置了 `EnabledByDefault`，或者某个目标在 `EnabledPlugins` 中列出了该插件时，
插件就会被构建。`DisabledPlugins` 会覆盖这两者。`EditorOnly` 插件和 `Editor` 宿主类型的模块
会从 Game 目标中剔除。独立的 `Program` 目标完全不承载任何插件。

## 增量构建如何做判断

每个 action 都会声明其输入和输出。action 会在以下情况下重新运行：

1. 某个输出缺失，
2. 生成该输出的命令与记录的命令不同，
3. 某个已声明的输入比输出更新，或
4. 上一次编译实际包含过的某个头文件比输出更新。

随后过期状态会传播到依赖方。

规则 2 正是让编辑 `Build.cs` 或 `Target.cs` 只使正确的工作失效、且仅使正确的工作失效的原因。
规则变化以不同的编译器和链接器命令行形式传到构建中，因此给叶子模块添加一个 public 定义会重建该
模块及其下游的一切，而一个不影响任何命令行的编辑则什么都不重建。这比按规则文件时间戳来失效更精确，
后者会在任何编辑时重建整个世界。

规则 4 使用编译器自身的 `/sourceDependencies` 输出，因此头文件追踪反映的是实际被包含的内容，
而不是规则所声称的内容。

缓存位于 `Intermediates/Obj/<Platform>/<Type>-<Config>/.buildtool/` 下。删除它们只需付出一次重建的
代价，别无其他。

## Unity 构建

模块可以把它的源文件编译成若干个生成的、`#include` 真实源文件的文件，这样共享头文件按组解析一次，
而不是按源文件各解析一次。

它默认开启。`TargetRules.bUseUnityBuild` 设置某个目标的默认值；
`ModuleRules.bUseUnityBuild` 是一个可空覆盖项，因此模块只在不同意时才需要表态。

内置的第三方库也会被合并，但逐个处理而非作为一整类：33 个第三方模块中有 27 个合并，
6 个在各自的 `Build.cs` 中选择退出，并附上导致其退出的错误。有五个在合并后编译失败，
每一个的失败方式都是这门技术所特有的：

| 模块 | 原因 |
| --- | --- |
| `BasicUniversal` | 合并源文件后 `astc_6x6_hdr` 变得有歧义 |
| `ImGui` | Vulkan 后端期望自己就是包含 Vulkan 头文件的那个 TU |
| `MSDFGen` | `_USE_MATH_DEFINES` 只影响第一次 `<cmath>` 包含，导致 `M_PI` 消失 |
| `Recast` | 文件作用域名称冲突，把一个初始化器绑定到了函数上 |
| `RmlUi` | `CommonSource.h` 在命名空间作用域定义了非 inline 变量 |

第六个 `EA` 情况不同，值得了解：它能编译，但随后*链接*失败。静态库让链接器只取它需要的对象，
而 EASTL 依赖这一点，因为引擎在 `EASTLImpl.cpp` 中提供了自己的 `eastl::AssertionFailure`。
合并会把 EASTL 的那份副本与引擎确实引用的符号放进同一个对象，于是它总是被拉入并产生冲突。
合并改变的是链接粒度，而不仅仅是编译，因此一次只做编译的排查不会发现它。

它的代价是隔离性。同一个 unity 文件中的源文件共享一个翻译单元，因此文件作用域的 static、
匿名命名空间、一个 `using namespace` 或一个遗留为定义状态的宏都会波及相邻文件，
而两个各自定义了相同内部辅助函数的文件现在会冲突。为引擎开启它时，在 `Runtime` 中暴露出四处
这类冲突，在 `Editor` 中也有四处；每一处都是重复代码或某个名称遮蔽了引擎符号，
并且全都在源文件中修复，而不是在这里绕过去。确实无法接受它的模块会设置
`bUseUnityBuild = false` 并说明原因。

有些源文件永远不能合并，这些会被自动排除，而不是靠谁记得把它们列出来：

- 任何带有 `PerFileCompilerOptions` 的文件，否则它会静默丢掉自己要求的标志
  （`JobScheduler.cpp` 需要 `/GT`，丢掉它意味着崩溃而非报错），
- 预编译头的源文件，它必须在自己的翻译单元中运行 `/Yc`，
- `PerImageSourceFiles`，它们是按已加载 image 而非按模块划分的，
- 反射分片（reflection shard），它们本身就已经是作为 unity 文件生成的。

`ModuleRules.ExcludeFromUnity` 覆盖那些只有代码本身才知道的情况：一个承载着单头文件库实现宏的
源文件必须成为唯一这样做的翻译单元，因此 `StbImageImpl.cpp` 和插件的 `RymlImpl.cpp` 会在这里
自我声明。

分组依据是源文件字节数（`UnityBuildBytesPerFile`，384 KB）而非文件数量，因为一个模块内部的
文件大小可能相差几个数量级。文件在分组前会先排序，因此同一棵目录树在任何机器上都会产生相同的分组，
未改动的模块会保留它已有的对象。可合并源文件少于 `MinFilesForUnityBuild` 的模块会逐文件编译。

每个生成的文件都会把其成员声明为构建输入，因此编辑某个被合并的源文件会在第一次构建时就重建包含
它的那个分组，早于任何头文件扫描运行。生成出的 IDE 项目列出真实源文件，而绝不会列出这些文件。

`-NoUnity` 会在一次构建中逐文件编译所有内容，当某个编译错误只在 unity 配置下出现时，
这是首先要尝试的手段。

在一次干净的 Development Editor 构建上测得：开启 unity 时为 590 个 action / 77.3 秒，
使用 `-NoUnity` 时为 1035 个 action / 103.2 秒。

## 架构

```
Core/           日志、命令行、带缓存时间戳的内化文件句柄、哈希
Configuration/  规则 API：TargetRules、ModuleRules、PluginDescriptor、目录布局
Rules/          规则文件的发现，及其通过 Roslyn 编译为缓存程序集
Graph/          源文件发现、模块与目标解析、依赖传播、action
Toolchain/      IToolchain 以及 MSVC 实现及其编译器和 SDK 定位器
Platform/       IBuildPlatform：各平台的命名约定与工具链选择
Execution/      action 图、时间戳与命令缓存、并行执行器
ProjectFiles/   IProjectFileGenerator 以及 Visual Studio 实现
Modes/          Build、Clean、Query 和 GenerateProjectFiles 入口点
```

真正重要的边界：

- **`Toolchain/` 之外没有任何代码知道编译器标志。** 依赖图处理的是 include 路径、
  定义和库；由工具链把它们转成命令行。
- **`Platform/` 之外没有任何代码知道文件扩展名。** 添加一个平台意味着实现
  `IBuildPlatform` 和 `IToolchain` 并注册该平台，核心无需改动。
- **`Execution/` 不知道一个 action 意味着什么。** 它只知道输入、输出和一条命令，
  这正是同一个执行器能处理编译、归档、链接和代码生成的原因。
- **`Rules/` 是唯一对用户类型做反射的地方。** 下游的一切都使用已解析的数据。

## 生成的 IDE 项目

项目是 NMake 项目：从 IDE 构建某个项目会回调 LuminaBuildTool，而不是在 MSBuild 中重新实现构建。

共有三种，这种划分很重要：

- **Target 项目**（`Lumina`、`Reflector`、`Tests`）负责构建。一个项目驱动一个
  目标，这能避免并行解决方案构建针对同一份输出启动多个工具实例。
- **Module 项目**用于浏览和编辑。每个都带有自身模块的 include 路径、
  定义和强制包含，因此 IntelliSense 完全准确，但不构建任何东西：模块本身不可独立构建，
  而给它套上目标的构建命令，正是以前会让一次解决方案构建为每个模块启动一次完整构建的原因。
- **`LuminaRules`** 是一个 C# 项目，包含所有的 `Target.cs`、`Build.cs` 和 `BuildRules.cs`。
  规则文件是针对该工具编译的普通 C#，但只有当某个项目认领它们时，IDE 才会把它们当作 C# 对待；
  没有这样一个项目，它们就会以纯文本打开，没有语法高亮、没有对 `ModuleRules` 的补全，
  也没有任何错误提示，直到构建失败。它通过 `ActiveCfg` 映射到解决方案中，但没有 `Build.0`，
  因此它从不构建：构建系统会自行编译这些相同的文件，而编译两次可能因为真实构建并不关心的原因
  而失败。它的设置刻意与 `RulesCompiler` 保持一致，包括 `ImplicitUsings=disable`，
  这样编辑器报告的内容与构建报告的内容是同一回事。

目标可以设置 `bBuildByDefault = false`，从而不参与整个解决方案的构建但仍可单独构建；
`Tests` 就是这么做的，因此 Build Solution 构建的是引擎和反射生成器，而不是所有东西。

### 从 IDE 运行

解决方案文件没有启动项目字段，因此在用户另行选择之前，IDE 运行的是第一个列出的项目。
设置了 `bIsStartupTarget` 的目标会被写在最前面，其余可构建目标紧随其后，
因此运行和调试落在的是一个真正的目标上，而不是按字母顺序排在最前的某个模块。

运行一个目标意味着什么，由该目标自己决定，因为它并不总是“启动这个目标所构建的东西”：

| | `DebuggerCommand` |
| --- | --- |
| 默认 | 启动模块的可执行文件 |
| 游戏项目 | 编辑器，并带 `--Project=<name>.lproject` |

游戏目标构建的是供编辑器加载的库，因此启动那个库毫无意义。
`LuminaGameTargetRules` 把它指向引擎，并由 `SetProjectFileToOpen` 提供项目。
设置会同时写入 `.vcxproj` 和一个生成的 `.vcxproj.user`，Visual Studio 和 Rider
正是从后者读取它们。

与项目形态无关的是，会写入同一份输出的并发构建会在一个锁文件上串行化，而不是相互竞争。
该锁以输出根、平台、目标类型和配置为键，因此不同配置仍可并行构建。它使用文件句柄而非命名
mutex，因为构建会在多个 await 之间持有它，而操作系统必须在构建进程死亡时释放它。

## 反射

启用了 `bEnableReflection` 的模块会按目标汇总到一次生成器运行中。该工具写出 Reflector 的输入文档、
把每个被反射的头文件声明为输入、把每个生成的 unity 分片声明为输出，并让普通的 action 图来编排
顺序：这些分片是编译输入，因此无需任何东西知道某个生成器产生了它们。

由于 Reflector 只在内容变化时才重写生成的文件，一次不改变任何被反射类型的头文件编辑不会重新生成
任何东西，而那些本会随之进行的编译会在执行时被丢弃，而不是白白运行。编辑一个引擎头文件只会重建
包含它的那少数几个翻译单元，而不是整个模块。

生成的 C# 绑定默认写到 `Intermediates/CSharpBindings`，并会把它们编译进
`LuminaSharp`。插件或游戏模块可以设置 `CSharpBindingsDirectory`，把它们改为写入自己的脚本程序集。

生成的 C++ 按目标划分，位于
`Intermediates/Reflection/<Platform>/<Target>/<Type>-<Config>/<Module>`，原因与目标文件相同：
两个共享同一模块的目标不一定会为它生成相同的代码，而共用一个目录会让彼此的构建看起来是过期的。
构建会显式把该路径传给生成器，因此布局是构建系统的决定，而不是固化在工具里的约定。

## 可选特性

`Engine/Build/BuildConfiguration.json` 把每个特性设为 `auto`、`on` 或 `off`，同名的命令行开关会
覆盖它。`auto` 对每个特性的含义属于引擎策略，位于 `Engine/Build/LuminaFeatures.BuildRules.cs`，
而不在该工具中。

```bash
LuminaBuild.bat Build Lumina -TargetType=Editor -Tracy=off
```

由于特性状态是以预处理器定义的形式传到构建中的，翻转某个特性只会使命令行发生变化的那部分工作失效。

## 游戏项目

项目是针对引擎目录树而不是它的预编译副本进行构建的：

```bash
LuminaBuild.bat Build MyGame -Project=C:\Path\To\MyGame
```

引擎模块把中间产物和二进制保留在引擎目录树中，因此引擎只构建一次并被每个项目共享，
而项目模块则落在项目自己的 `Binaries` 和 `Intermediates` 中。因此构建一个游戏项目同时也会让引擎
保持最新，而不是静默地链接到过期的引擎。

项目模块派生自 `LuminaGameModuleRules`，项目目标派生自
`LuminaGameTargetRules`，两者都位于 `Engine/Build/LuminaGameProject.BuildRules.cs`。

`GenerateProjectFiles -Project=<path>` 会写出项目自己的解决方案，并且它生成的 NMake 命令会
把该 `-Project` 一路带下去，因此从项目的 IDE 构建时解析的是项目的目标，而不只是引擎的目标。

### 模块导出宏

`<MODULENAME>_API` 由构建系统定义，而不是写在任何地方。每个模块都会为自己、以及其依赖闭包中的
每个共享库获得一个，从依赖图中解析得出：在编译拥有它的模块时为 `DLL_EXPORT`，
对每个使用它的东西为 `DLL_IMPORT`，在单体链接下则为空。

这正是游戏或插件模块得以导出任何东西的原因。引擎头文件中的一份宏列表永远只能列出随引擎一同发布
的模块，而树外的东西无法把自己加进去。

## 布局约定

源文件通过遍历模块的源目录来发现。一个包含自身 `Build.cs` 的子目录属于另一个模块并被跳过，
因此模块之间从不会争抢源文件。模块的源根目录是其公开接口的一部分：依赖方以头文件所在的路径来
包含它们。第三方模块则显式声明其 include 根目录。

二进制落在 `Binaries/<Platform>`，使用平台的目录名，即 `Windows64` 而非 `Win64`。该字符串以
`LUMINA_PLATFORM_NAME` 的形式固化在引擎中，运行时正是用它来解析插件和项目模块 DLL，
因此不能只改动其中一侧。

### 预编译头

模块的 PCH 以该模块命名：`RuntimePCH.h`、`EditorPCH.h`。由于模块的源根目录位于所有下游内容的
include 路径上，那里一个通用的名称就会变成每个依赖方都必须避开的名称。当两者都叫 `pch.h` 时，
`EditorPCH.h` 中的 `#include "pch.h"` 会解析到包含者自己的目录，Runtime 的那个永远不会到达，
这就是为什么必须重命名 Editor 的那个，而不是反过来。

没有翻译单元需要包含自己的 PCH：工具链会在其他一切之前强制包含它，这也让 `/Yu` 能在一个并非
手写的生成文件中找到它的标记。每个源文件顶部的那条显式 include 只是约定，而且它必须写出模块
自己的 PCH 名称。

同样的情况也适用于生成的代码。反射生成器通过反射输入文档获知每个模块的 PCH 并据此生成，
或在模块没有 PCH 时什么都不生成。它过去会生成字面量 `pch.h`，那只在 Runtime 恰好位于所有人
include 路径上时才行得通。

### 按文件名引用的规则

`PerFileCompilerOptions` 和 `ExcludeFromUnity` 以裸文件名为键，因此一次重命名会静默丢掉挂在旧
文件名上的任何东西。当其中任一规则引用了一个模块并不拥有的文件时，扫描器会让构建失败。
在这里保持沉默是错误的结果：`JobScheduler.cpp` 丢掉 `/GT` 会导致 fiber 调度器崩溃，
而不是给出一条诊断信息。

## 当前状态

引擎所需的一切都已就绪：规则发现与编译、带 public 与 private 传播的模块与插件图解析、
MSVC 工具链探测、带预编译头的 C 和 C++ 编译、反射代码生成、静态库归档、共享库与可执行文件链接、
带 whole-archive 模块注册的单体 Shipping、.NET 项目构建、运行时依赖归置、头文件精确的增量构建、
并行执行，以及 Visual Studio 项目与解决方案生成。

引擎在每一种配置和每一种目标类型下都能通过它构建，`Reflector` 和 `Tests` 也一样。

已验证：每个目标都能构建，并在下一次运行时报告零个过期 action，覆盖了
Editor 和 Game 下三种配置的 `Lumina`，以及 `Reflector` 和 `Tests`。

已知范围限制：

- 仅支持 Windows 和 MSVC。添加一个平台意味着实现 `IBuildPlatform` 和 `IToolchain`
  并注册它；核心无需任何改动。
- Editor 目标永远不是单体的。`Runtime` 和 `Editor` 各自带有一份
  stb_image_write 实现的副本，这在分离的 image 中是正确做法，而在同一个 image 中则是重复符号。
  排除了编辑器模块的 Shipping 游戏会按预期以单体方式链接。
- 构建前打印的过期 action 计数是一个上界。那些输入最终并未发生变化的 action 会在构建运行时被丢弃，
  摘要会报告丢弃了多少个。
- 生成代码创建之后的第一次构建会记录使其保持最新的输入高水位标记，因此一个新生成的目标会在第二次
  运行而非第一次时稳定下来。
