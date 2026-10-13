# $PROJECTNAME

一个 Lumina Engine 项目。

## 环境要求

- `LUMINA_DIR` 环境变量必须指向你的引擎安装目录（Windows 上由引擎的 `Setup.bat` 设置，Linux 上由 `Setup.sh` 设置）。
- **Windows：** Visual Studio 2026 (18.0+)，并安装 C++ 工作负载。（引擎的 C# 层目标框架为 `net10.0`，需要 VS 18.0+；仅安装独立的 .NET 10 SDK 是不够的。）
- **Linux：** GCC 13+ 以及 .NET 10 SDK。完整的依赖包列表见引擎的 README。

## 首次配置（Windows）

1. 在本文件夹中运行 `GenerateProject.bat`。它会为本项目调用 `%LUMINA_DIR%\LuminaBuild.bat GenerateProjectFiles` 并写出 `$PROJECTNAME.sln`。
2. 在 **Visual Studio** 或 **JetBrains Rider** 中打开 `$PROJECTNAME.sln`。
3. 按 **F5**。

F5 会构建游戏 DLL（`Binaries\Windows64\$PROJECTNAME-Development.dll`），并启动已预加载本项目的
Lumina 编辑器。`IMPLEMENT_MODULE` 一运行，你游戏模块中的断点就会命中。

## 首次配置（Linux）

```bash
./GenerateProject.sh                                              # writes compile_commands.json
"$LUMINA_DIR/LuminaBuild.sh" Build $PROJECTNAME -TargetType=Editor
"$LUMINA_DIR/LuminaBuild.sh" Run   $PROJECTNAME -TargetType=Editor
```

这会构建游戏共享库（`Binaries/Linux64/lib$PROJECTNAME-Development.so`）并启动已预加载本项目的
编辑器。Linux 上不存在解决方案：`GenerateProject.sh` 会写出一个 `compile_commands.json`，
clangd、CLion 以及 VS Code 的 clangd 扩展会读取它来实现补全。

该解决方案还包含引擎自身的各个目标，因为引擎是与你的项目一起从源码构建的。
它的输出留在引擎目录树中，并被每个项目共享，因此只会构建一次，之后不再重复构建。

**Visual Studio** 和 **Rider** 都会读取生成的 `.vcxproj.user`，它会将 Run 指向编辑器，
并把 `--Project=` 设为本项目。

## 脚本（C#）

游戏逻辑使用 **C#** 配合 `LuminaSharp` 编写。脚本位于 `Game/Scripts/`，并且**在编辑器内部编译**：
编辑 `.cs`、保存，改动即刻生效；无需重建 DLL，无需重启编辑器。

- 脚本是一个派生自 `EntityScript` 的类（见 `Game/Scripts/ExampleScript.cs`）。它可以获得 `Entity`、`World`、`Registry`、缓存的 `Transform`，以及生命周期钩子（`OnAttach` / `OnReady` / `OnUpdate` / `OnDetach`）外加输入和碰撞回调。
- 通过添加 **C# Script** 组件并选择脚本类，即可把脚本挂到某个实体上。标记了 `[Property]` 的字段会出现在检视器中。
- `Game/Scripts/<...>.Scripts.csproj` 是**生成**的，仅用于 IDE 的 IntelliSense（它引用引擎的 `LuminaSharp.dll`）。它会在项目加载时以及通过 `dotnet.genprojects` 控制台命令重新创建，绝不要提交它，也绝不要依赖它来构建（引擎会在运行时自行编译脚本）。

`Source/` 中的 C++ 模块是可选的：在需要原生类型、自定义组件和引擎集成时使用它。
一个项目完全可以只在其上编写纯 C# 脚本。

## 迭代

- **C# 脚本**（`Game/Scripts/*.cs`）：在你的编辑器中保存；运行中的引擎会重新编译并重新加载它们。
- **内容**（`Game/Content/` 中的资源）：在编辑器内热重载；无需重建。
- **C++**（`Source/*.cpp` / `*.h`）：再次按 F5（或重新运行 `LuminaBuild.sh Build`）来重建游戏模块并重启编辑器。新增的 `.h` / `.cpp` 文件会被构建自动发现；重新运行 `GenerateProject.bat` / `GenerateProject.sh` 可让它们出现在 IDE 的文件列表中。

## 项目布局

```
$PROJECTNAME.lproject          项目描述符（名称、GUID、插件）
GenerateProject.bat            在增删源文件后重新生成 .sln（Windows）
GenerateProject.sh             同上，但改为写出 compile_commands.json（Linux）
Config/GameSettings.json       按项目划分的引擎设置（启动地图、cook 根目录等）
Source/                        你的 C++ 模块（可选）
  $PROJECTNAME.Target.cs       构建什么：指定启动模块，并把 Run 指向编辑器
  $PROJECTNAME.Build.cs        如何构建：依赖、定义、include 路径
Plugins/                       项目插件，每个一个文件夹（见 Plugins/README.md）
Game/Content/                  资源，以 /Game/Content 暴露给引擎
Game/Scripts/                  C# 脚本，在编辑器内编译（以 /Game/Scripts 暴露）
Logs/                          加载本项目运行时的引擎日志（Lumina.log，保留 5 份）
CrashDumps/                    加载本项目运行时的 minidump 和 GPU 崩溃转储
```

`Binaries/`、`Intermediates/`、`Logs/` 和 `CrashDumps/` 是生成产物，会被 git 忽略。

## 为 C++ 模块添加内容

`Source/$PROJECTNAME.Build.cs` 是该模块的完整构建配置。若要使用其他
引擎模块或第三方模块，在那里写明它：

```csharp
PrivateDependencyModuleNames.Add("Box3D");
```

用 `$PROJECTNAMEUPPER_API` 宏从模块中导出类型，正如
`Source/$PROJECTNAMEModule.h` 所做的那样。该宏由构建系统为你定义；没有任何头文件
需要编辑，也没有任何东西需要声明。
