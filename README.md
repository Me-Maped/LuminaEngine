<div align="center">

<img src="https://github.com/user-attachments/assets/552b8ca0-ebca-4876-9c6a-df38c468d41e" width="120"/>

# Lumina Game Engine

**一款基于 Vulkan 构建的现代高性能游戏引擎**

[![License](https://img.shields.io/github/license/mrdrelliot/lumina)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-blue)](https://github.com/mrdrelliot/lumina)
[![C++](https://img.shields.io/badge/C++-23-blue)](https://github.com/mrdrelliot/lumina)
[![Vulkan](https://img.shields.io/badge/Vulkan-renderer-red)](https://www.vulkan.org/)
[![Discord](https://img.shields.io/discord/1193738186892005387?label=Discord&logo=discord)](https://discord.gg/xQSB7CRzQE)

[官网](https://luminagameengine.com) &bull; [Discord](https://discord.gg/xQSB7CRzQE) &bull; [文档](https://luminagameengine.com/getting-started/introduction/) &bull; [支持](#支持)

</div>

---

## 目录

- [Lumina Game Engine](#lumina-game-engine)
  - [目录](#目录)
  - [概述](#概述)
  - [特性](#特性)
    - [渲染](#渲染)
    - [架构](#架构)
    - [编辑器](#编辑器)
    - [性能](#性能)
    - [C# 脚本](#c-脚本)
  - [文档](#文档)
  - [截图](#截图)
  - [快速开始](#快速开始)
    - [环境要求](#环境要求)
      - [Windows](#windows)
      - [Linux](#linux)
    - [安装（Windows）](#安装windows)
    - [安装（Linux）](#安装linux)
    - [构建配置](#构建配置)
    - [故障排查](#故障排查)
  - [贡献](#贡献)
    - [工作流程](#工作流程)
    - [要求](#要求)
  - [支持](#支持)
  - [第三方依赖](#第三方依赖)
  - [致谢](#致谢)
  - [许可证](#许可证)
  - [联系方式](#联系方式)

---

## 概述

Lumina 是一款从零开始、以 Vulkan 为基础打造的现代 C++ 游戏引擎。它的设计目标是结合真实的引擎架构进行学习与实验，展示了多种专业设计模式，包括反射系统、基于 ECS 的游戏逻辑层，以及现代渲染技术。

> [!NOTE]
> 获取更详细的信息，请访问：https://luminagameengine.com

它非常适合用于：

- 学习现代游戏引擎架构
- 试验 Vulkan 渲染技术
- 在干净、模块化的代码库上构建原型
- 理解 Unreal、Godot 等引擎的内部工作原理

贡献者会通过多种方式获得认可，包括热门游戏的 Steam 激活码，以及在 Discord 社区中的公开致谢。Lumina 正是依靠有动力的贡献者的工作不断改进、推动引擎向前发展。

> [!CAUTION]
> Lumina 是一个正在积极开发中的教育项目。API 可能发生变化，部分功能仍处于实验阶段。如果你遇到构建问题，请在 [Discord](https://discord.gg/xQSB7CRzQE) 上寻求帮助。


> [!NOTE]
>
> ### AI 使用说明
>
> AI 仅被有选择地用于静态分析、调研、构建工具和脚本辅助等任务。
>
> 引擎本身是历经三年手工打造的业余爱好项目。AI 被当作一种在能创造价值时使用的工具，仅此而已。


---

## 特性

### 渲染

- 由 Vulkan 驱动的渲染器，具备自动资源追踪与 barrier 插入
- 采用 Forward+ 管线与 clustered lighting，可高效处理多光源场景
- 通过材质图（material graph）创作 PBR 材质，并编译为 shader 代码

### 架构

- 自研的稀疏集（sparse-set）Entity Component System，支持按类型划分的存储布局、类型化视图和生命周期信号
- 反射系统驱动自动序列化与编辑器集成
- 模块化设计，关注点清晰分离

### 编辑器

- 基于 ImGui 的编辑器，支持实时场景操作
- 用于实体管理的场景层级（Scene Hierarchy）
- 组件检视器，其 UI 由反射自动生成

### 性能

- 基于 fiber 构建的多线程任务系统
- 基于 RPMalloc 构建的自定义内存分配器
- 通过 Tracy 集成提供内置性能分析

### C# 脚本

- 通过 LuminaSharp 实现 C# 脚本，LuminaSharp 是一个内置 .NET 10 运行时的 CoreCLR 宿主
- 基于 EntityScript 的游戏逻辑，可完整访问 ECS：`Registry.View` 查询、C# 实体系统，以及组件读写
- 由 Reflector 自动生成、反射驱动的 C++ 到 C# 绑定
- 支持脚本热重载，无需重新编译即可迭代游戏逻辑
- 通过 `World` 门面（facade）提供的高层游戏 API：Physics、Navigation、Input、Messages 和 GameplayTags

---

## 文档

[快速开始](https://luminagameengine.com/getting-started/introduction/)

仓库内还包含一组架构文档，说明各子系统真实的实现方式（每条结论都附源码路径）：

- [引擎概览](docs/EngineOverview.md) — 仓库布局、模块与 target、启动流程、主循环与更新阶段
- [渲染架构](docs/RenderingArchitecture.md) — RHI 分层、帧编排、shader 与管线缓存、场景渲染的 pass 顺序
- [ECS 指南](docs/ECSGuide.md) — `FEntity`、`FRegistry`、组件存储、typed view、如何定义新组件
- [反射系统](docs/ReflectionSystem.md) — 标记宏、Reflector、运行时元数据、序列化与 C# 绑定

总入口见 [docs/README.md](docs/README.md)。

---

## 截图

<div align="center">
  <img width="800" alt="Lumina editor" src="https://github.com/user-attachments/assets/ba4fc531-362c-4b4f-bbec-261bb1e79652" />
  <img width="800" alt="Lumina scene" src="https://github.com/user-attachments/assets/5df3dee0-71fe-4439-b851-5e22ff2b23cc" />
</div>

<details>
  <summary><b>显示更多截图</b></summary>

  <div align="center">
    <img width="800" alt="Lumina screenshot" src="https://github.com/user-attachments/assets/086a1e5e-4d8d-4c98-be81-bc77287fba39" />
    <img width="800" alt="Lumina screenshot" src="https://github.com/user-attachments/assets/66d739ca-f257-4350-b2f0-2e10d66f5591" />
    <img width="800" alt="Lumina screenshot" src="https://github.com/user-attachments/assets/68e86207-20f2-4200-8649-257738b2855f" />
    <img width="800" alt="Lumina screenshot" src="https://github.com/user-attachments/assets/a4c7c497-d975-47d2-a695-5342e11e8f44" />
    <img width="800" alt="Lumina screenshot" src="https://github.com/user-attachments/assets/5343eff8-545a-47ca-a858-b2aa4f1fef71" />
  </div>
</details>

https://github.com/user-attachments/assets/3d797479-fc47-4b8f-baf4-87315709d0c2

---

## 快速开始

### 环境要求

**所有平台**都需要一块支持 Vulkan mesh shader（`VK_EXT_mesh_shader`）的 GPU：NVIDIA Turing（GTX 16 系列 / RTX 20 系列）或更新，AMD RDNA2（RX 6000）或更新，或 Intel Arc。渲染器的几何管线建立在这些特性之上，因此编辑器在更旧的硬件上会拒绝启动，并给出说明。

该 GPU 还需要 **4 GB 显存，建议 6 GB 或更多**。渲染器在任何内容加载之前就会预留远超 1 GB 的显存（render target、两张 4K shadow atlas，以及默认 1 GB 的流式纹理池），低于最低要求的显卡不会优雅降级，而是渲染出错帧并最终丢失设备。这项检查与 mesh shader 检查一并在设备选择时执行，并会指出具体差在哪里。有少数支持 mesh shader 的型号低于这条线，多数是 2 GB 的笔记本芯片，例如 GeForce MX550 和 MX570。

`-ignoreminspec` 会无视限制继续运行（不受支持）。`-minvram=N` 可设置不同的阈值，这是在能通过检查的硬件上查看拒绝对话框的方法。

#### Windows

- Windows 10（1803 或更新）或 Windows 11，64 位
- **Visual Studio 2026（18.0 或更新）**，并安装 MSVC v143 工具集和“使用 .NET 的桌面开发”工作负载
- **.NET 10 SDK**（x64）

> [!IMPORTANT]
> 引擎的 C# 层（LuminaSharp）目标框架为 **`net10.0`**。只有 Visual Studio
> **18.0+（2026）** 才能构建该目标框架，VS 2022（17.x）即使安装了独立的 .NET 10 SDK 也会
> 以 `error NETSDK1209` 失败，因为 VS 使用的是它自带的 MSBuild。`Setup.bat` 会为你校验
> 这一点，一旦缺东西就会停下并给出清晰的提示。

#### Linux

- 64 位发行版
- **GCC 16 或更新。** `Memory/Construct.h` 使用了 `__is_trivially_destructible`，
  这是 GCC 直到 16 才新增的内建特性，因此 g++-13 和 g++-15 会直接拒绝这套代码树。
  Clang 目前也不可用，无论其链接的 libstdc++ 是什么版本：它会拒绝
  `Memory/Memory.h`、`PlatformString.h` 和 `MeshData.h` 中 `RUNTIME_API NODISCARD`
  这样的属性顺序，而 GCC 是接受的。
- **.NET 10 SDK**
- X11 开发包，GLFW 会直接链接它们
- 运行时需要 Vulkan **loader 和驱动**。编译时并不需要这些：Vulkan 头文件已随源码内置，
  volk 通过 `dlopen` 解析入口点，因此一台没有任何驱动的机器也能构建出编辑器，只是无法启动。

  在 Debian 或 Ubuntu 上：

  ```bash
  # build
  sudo apt-get install -y g++-16 pkg-config libx11-dev libxrandr-dev \
      libxinerama-dev libxcursor-dev libxi-dev libxkbcommon-dev
  # run
  sudo apt-get install -y libvulkan1 mesa-vulkan-drivers vulkan-tools
  # .NET 10 SDK: https://dotnet.microsoft.com/download/dotnet/10.0
  ```

  `Setup.sh` 会检查上述全部内容，并告诉你缺了什么，包括已安装的 GPU 是否真的报告支持
  `VK_EXT_mesh_shader`。

> [!NOTE]
> 不需要 IDE。在 Windows 上，[JetBrains Rider](https://www.jetbrains.com/rider/) 和
> Visual Studio 都能打开生成的解决方案。在 Linux 上没有解决方案文件：
> [CLion](https://www.jetbrains.com/clion/) 会读取为 C++ 生成的
> `compile_commands.json`，Rider 则打开引擎的 C# 项目。

### 安装（Windows）

1. **克隆仓库**

   ```bash
   git clone https://github.com/mrdrelliot/luminaengine
   cd LuminaEngine
   ```

2. **运行 setup**

   ```bash
   Setup.bat
   ```
   它会下载一个预编译的依赖包
   （`External-Win64.zip`，约 192 MB），用本仓库中固定的 SHA-256 哈希进行校验
   （需要维护者先录入，见 [`DEPENDENCIES.md`](DEPENDENCIES.md)），配置 git hooks，
   注册 `.lproject` 文件类型，并生成 `Lumina.sln`。

   该依赖包包含 .NET 10 运行时（C# 脚本）、LLVM/Clang 19
   （反射代码生成）、Slang shader 编译器、RenderDoc 和 Tracy。每一项都是开源的；
   [`DEPENDENCIES.md`](DEPENDENCIES.md) 列出了每个组件的版本、用途、上游来源和许可证，
   并说明了如何自行获取这些组件而不使用依赖包。

   若要在无人值守/CI 运行中跳过提示，请传入 `--yes`（或设置
   `LUMINA_SETUP_YES=1`）。如果下载失败，请手动下载
   [External-Win64.zip](https://github.com/MrDrElliot/LuminaEngine/releases/download/external-deps/External-Win64.zip)，
   解压到 `LuminaEngine/` 文件夹，然后运行
   `GenerateProjectFiles.bat`。

1. **打开解决方案**

   在 Visual Studio 中打开 `Lumina.sln`。

2. **构建并运行**

   - 将 `Lumina` 设为启动项目。
   - 选择配置：`Development Editor`（默认）、`Debug Editor`（完整调试器功能，牺牲速度），
     或 `Development Game`（不含编辑器的运行时）。
   - 按 F5，或使用 **Build -> Run**。

3. **开始开发**

   - 复制 `Templates/Blank/` 来创建新项目，然后运行其中的
     `GenerateProject.bat` 生成解决方案。

### 安装（Linux）

下面每个脚本在 Windows 上都有同名的 `.bat` 版本，并接受相同的参数。

1. **克隆仓库**

   ```bash
   git clone https://github.com/mrdrelliot/luminaengine
   cd LuminaEngine
   ```

2. **运行 setup**

   ```bash
   ./Setup.sh
   ```

   它会检查上述先决条件、下载一个预编译的依赖包
   （`External-Linux64.tar.gz`，约 186 MB）、用本仓库中固定的 SHA-256 哈希校验，
   并配置 git hooks。

   该依赖包包含 .NET 10 运行时（C# 脚本）、LLVM/Clang 19
   （反射代码生成）、Slang shader 编译器、RenderDoc 和 Tracy。每一项都是开源的；
   [`DEPENDENCIES.md`](DEPENDENCIES.md) 列出了每个组件的版本、用途、上游来源和许可证，
   并说明了如何自行获取这些组件而不使用依赖包。

   若要在无人值守/CI 运行中跳过提示，请传入 `-Yes`（或设置
   `LUMINA_SETUP_YES=1`）；无人值守运行绝不会修改你的 shell 配置文件。如果
   下载失败，请手动获取
   [External-Linux64.tar.gz](https://github.com/MrDrElliot/LuminaEngine/releases/download/external-deps/External-Linux64.tar.gz)
   并解压到 `LuminaEngine/` 文件夹。

3. **构建**

   ```bash
   ./LuminaBuild.sh Build Lumina -TargetType=Editor
   ```

   Reflector 是该目标的前置依赖，会先自行构建，所以这是你唯一需要的命令。加上
   `-Configuration=Debug` 可获得完整的调试器保真度，代价是速度；用 `-TargetType=Game`
   可获得不含编辑器的运行时。

4. **运行**

   ```bash
   ./LuminaBuild.sh Run Lumina -TargetType=Editor
   ```

   `Run` 会按构建时所用的同一套规则定位二进制文件，因此它总是启动你指定的配置，
   而不是最近一次构建出来的东西。它接受与 `Build` 相同的 `-TargetType` 和
   `-Configuration` 参数，而裸 `--` 之后的任何内容都会原样转发给编辑器。如果你更愿意
   直接启动可执行文件，它位于 `Binaries/Linux64/Lumina-Editor-Development`；它会相对
   自身定位引擎，因此不关心你从哪个目录运行。

5. **配置代码补全**

   ```bash
   ./GenerateProjectFiles.sh
   ```

   这会在仓库根目录写出 `compile_commands.json`。Linux 上不生成解决方案文件，
   因为这里没有任何东西能打开它。把你的编辑器指向仓库根目录，它就会拾取该数据库：

   - **clangd**（以及 VS Code 的 clangd 扩展）无需任何配置即可找到它。
   - **CLion**：打开该文件夹，在提示时选择 compile database。

   Rider 是用于引擎 C# 项目（`LuminaBuildTool`、`LuminaSharp`）的工具，
   而不是用于 compile database 的。

   在增删模块、插件或源文件之后请重新运行它。普通源文件在构建时被发现，
   而修改 `Build.cs` 的构建会自行刷新该数据库。

6. **开始开发**

   在编辑器中创建项目，或手工复制 `Templates/Blank/` 并运行其中的
   `GenerateProject.sh`。每个生成的项目都同时附带 `GenerateProject.bat` 和
   `GenerateProject.sh`，因此在一个平台上创建的项目在另一个平台上依然可用。

### 构建配置

可选的引擎特性由单个文件控制，
[`Engine/Build/BuildConfiguration.json`](Engine/Build/BuildConfiguration.json)。每个特性可设为
`"auto"`、`"on"`（强制纳入所有配置）或 `"off"`（从所有配置中剔除），这些选择会在你重新生成解决方案时固化。

| 特性 | 控制内容 | `"auto"` 默认值 |
| -------------------- | ------------------------------------------ | ---------------------------------------- |
| `Tracy` | Tracy CPU/GPU 性能分析器（`LUMINA_PROFILE_*`） | Debug + Development；Shipping 中**关闭** |
| `GpuProfiling` | GPU 计时查询池及其回读 | Debug + Development；Shipping 中**关闭** |
| `RmlUiProfiling` | RmlUi 自身的 Tracy zone，每个 element、property 和 text run 一个 | **关闭**；需要 `Tracy` |
| `Validation` | Vulkan 验证层 + 同步层 | 仅 Debug |
| `GpuValidation` | 在每个 shader 内进行的 GPU 辅助验证 | 各处均**关闭**；`--gpuvalidation` 在运行时切换 |
| `Aftermath` | NVIDIA Nsight Aftermath GPU 崩溃转储 | NVIDIA 主机，Debug + Development |
| `RadeonGpuDetective` | 针对 `.rgd` 转储的 AMD 对象命名与故障报告 | AMD 主机，Debug + Development |
| `VerboseLogging` | `LOG_TRACE` / `LOG_DEBUG` / `LOG_INFO` 宏 | Debug + Development；Shipping 中**关闭** |
| `BugSplat` | 向引擎数据库上传崩溃和 minidump | 仅 Windows 上的 Editor 目标 |
| `Box3DDebugChecks` | Box3D 自身的断言与内部校验 | 仅 Debug |

`"off"` 是真正的剔除：例如 `Tracy = "off"` 会把 Tracy 库完全从构建中移除，
并把每个性能分析宏变成空操作，而 `VerboseLogging = "off"` 会移除 `LOG_TRACE/DEBUG/INFO`
（warning 和 error 始终保留）。`Aftermath` 仅在生成解决方案的机器上检测到 NVIDIA GPU 时才会自动启用。

对于一次性构建（例如产出不含性能分析的包），你可以在命令行上覆盖任何特性而无需编辑文件，命令行标志优先：

```bash
GenerateProjectFiles.bat --tracy=off --validation=on --verbose-logging=off
```

在 Linux 上没有可供固化这些选择的解决方案文件，因此请把同样的标志传给构建
（或传给 `./GenerateProjectFiles.sh`，它只影响 compile database）：

```bash
./LuminaBuild.sh Build Lumina -TargetType=Editor -Tracy=off -Validation=on
```

重新生成时会打印解析后的特性集合，例如
`[Lumina] Build features: Tracy=auto (Debug, Development)  Validation=auto (Debug)  Aftermath=auto (Debug, Development)  VerboseLogging=auto (Debug, Development)  [NVIDIA GPU]`。

### 故障排查

#### Linux

> [!TIP]
> - **“Vulkan Device Unsuitable: No GPU meeting the renderer's requirements was
>   found”？** 日志会列出它拒绝的每一块 GPU 以及原因。如果原因对每个设备都是
>   `no VK_EXT_mesh_shader` 或 `below the ... MiB minimum`，说明硬件低于
>   [环境要求](#环境要求)中的下限。如果你预期的某块 GPU 完全不在列表中，
>   说明 Vulkan loader 没有看到它的驱动；用 `vulkaninfo --summary` 检查并对比。
> - **混合显卡笔记本启动到了错误的 GPU？** 设备在启动时选择，只要独显能被枚举出来就
>   优先使用它。如果它没有出现在 `vulkaninfo --summary` 中，说明专有驱动没有加载，
>   任何引擎设置都无法绕过这一点。
> - **`error: the .NET SDK is required`，但你确实装了？**
>   `dotnet-install.sh` 会把它装到 `$HOME/.dotnet` 且不做任何持久化。这里的脚本
>   无论如何都会去那里查找，但除非你把它加进 `PATH`，你自己的 shell 不会。
> - **构建使用的核心数比机器实际拥有的少？** 这是有意为之：并行度被限制以适配可用内存，
>   原因会记录在日志中。可用 `-MaxParallel=<n>` 覆盖。
> - **在 `stdc++_libbacktrace` 或 `stdc++exp` 上链接失败？** `std::stacktrace` 背后的
>   归档文件在 GCC 14 中改名了，工具链会探测你的编译器附带的是哪一个。如果仍然失败，
>   你的编译器很可能不是你以为的那个；检查 `CXX`。
> - **在 `External/` 或 `ThirdParty/` 内部构建失败？** 通常是用了比这套代码树构建时更新的
>   编译器。目前没有更旧的版本可以固定回去：GCC 16 既是下限，也是测试过的最新版本。

#### Windows

> [!TIP]
> - **`error NETSDK1209` / “does not support targeting .NET 10.0”？** 你的 Visual
>   Studio 比 18.0（2026）旧。请安装 VS 2026，仅安装独立的 .NET 10
>   SDK **不会**解决这个问题，因为 VS 构建时使用的是它自带的
>   MSBuild。运行 `Setup.bat`（或 `BuildScripts\CheckPrerequisites.ps1`）来验证。
> - **缺少 v143 工具集？** 通过 Visual Studio Installer ->
>   Individual Components -> MSVC v143 Build Tools 安装。
> - **出现 “Cannot find .generated.h” 错误？** 再构建一次；Visual Studio 有时
>   需要第二轮才能拾取生成的文件。
> - **达到 C1076 编译器上限？** 重试构建；这是一个与某个字体文件相关的已知
>   偶发问题。
> - **“Application control policy blocked this file”？** 关闭 Windows 11
>   的 Smart App Control。
> - **`error NETSDK1004` / “Assets file ... project.assets.json not found”？** 托管项目的
>   NuGet 还原位于 `Intermediates/` 下，因此删除该文件夹会一并删除它。运行
>   `GenerateProjectFiles.bat`（它会在最后执行还原），或
>   给 MSBuild 传入 `-restore`。
> - **“C# scripting disabled: managed bootstrap missing”？** `LuminaSharp`
>   托管项目没有构建成功，通常是上面的还原问题。运行
>   `GenerateProjectFiles.bat` 并重新构建（`Lumina` 应用会把它作为
>   依赖项来构建）。在命令行上，给 MSBuild 传入 `-restore`。

**在任一平台上仍然失败？**
[提交 issue](https://github.com/mrdrelliot/LuminaEngine/issues) 或在 Discord 上
联系我们。

> [!NOTE]
> 不再需要环境变量。每个引擎检出目录会在其中首次运行任何 `LuminaBuild` 命令时记录自身，
> 项目可以用 Switch Engine 指定它的引擎（右键点击 `.lproject`，或
> `LuminaBuild SwitchEngine <file>`）。没有指定引擎的项目会使用打开它的那个引擎。

> [!CAUTION]
> 在拉取或合并之后，删除 `Binaries/` 和 `Intermediates/`，然后在 Windows 上运行
> `GenerateProjectFiles.bat` 重新生成解决方案，或在 Linux 上运行
> `./GenerateProjectFiles.sh` 刷新 compile database。

---

## 贡献

完整准则见 [CONTRIBUTING.md](CONTRIBUTING.md)。

欢迎任何形式的贡献，无论是 bug 修复、新功能还是文档改进。

### 工作流程

1. Fork 仓库。
2. 创建功能分支：`git checkout -b feature/amazing-feature`。
3. 按照[编码规范](CONTRIBUTING.md)进行修改。
4. 在适用的地方添加测试。
5. 用清晰的提交信息提交：`git commit -m "Add amazing feature"`。
6. 推送到你的分支：`git push origin feature/amazing-feature`。
7. 发起 pull request。

### 要求

- 干净、文档完善的代码
- 遵循现有架构模式
- 在合适的地方编写测试
- 按需更新文档

---

## 支持

Lumina 是一个公开构建和维护的业余爱好项目。如果你觉得它有用，
[在 Patreon 上支持 Lumina](https://www.patreon.com/c/DrElliot)。
你的支持有助于维持引擎、文档以及未来版本的开发工作。

你也可以通过[贡献代码或文档](CONTRIBUTING.md)、
[报告 bug 或分享想法](https://github.com/MrDrElliot/LuminaEngine/issues)，以及向其他开发者
介绍 Lumina 来提供帮助。每一种支持都能帮助项目成长。感谢你成为其中的一分子。

---

## 第三方依赖

按字母顺序排列。

| 库 | 用途 |
| --------- | --------- |
| basis_universal | GPU 纹理压缩，可在运行时转码为 BC7/ETC/ASTC |
| Box3D | 多线程刚体物理，带无状态的胶囊体角色移动器 |
| BugSplat | 向引擎崩溃数据库上传崩溃与 minidump |
| cgltf | 网格与材质导入器背后的 glTF 2.0 解析器 |
| DPDK | `Engine/Source/Runtime/Source/Memory/Memcpy.h` 中向量化 memcpy 的来源；仅为许可证而内置 |
| ENet | 网络插件背后的可靠 UDP 传输 |
| FreeType | 为 RmlUi、MSDFGen 和编辑器文本提供字体光栅化 |
| GLFW | 面向 OpenGL 和 Vulkan 的多平台窗口与输入库 |
| GoogleTest | 带断言、fixture 和测试发现功能的 C++ 测试框架 |
| ImGui | 用于快速工具开发的即时模式 GUI |
| MeshOptimizer | 针对 vertex cache、overdraw 和 buffer 压缩的网格优化 |
| MikkTSpace | 为导入的网格提供参考切线空间生成 |
| Miniz | 用于 package 和 pak 归档序列化的 Deflate 压缩 |
| MSDFGen | 用于清晰世界空间文本的多通道有符号距离场 |
| .NET Runtime | 作为 C#（LuminaSharp）脚本宿主的 CoreCLR（.NET 10） |
| Nlohmann JSON | 兼容 STL 的现代 JSON 库 |
| NVIDIA Aftermath | GPU 崩溃调试与事后转储分析 |
| NVIDIA Nsight Perf | 硬件性能分析器插件背后的 GPU 计数器采集 |
| Recast & Detour | 导航网格生成与寻路 |
| RenderDoc | 用于帧捕获与分析的图形调试器集成 |
| RmlUi | 用于游戏和编辑器界面的保留式 HTML/CSS 风格 UI |
| RPMalloc | 无锁、线程缓存的内存分配器 |
| Slang | 现代 shader 语言与编译器，可输出 SPIR-V / HLSL |
| stb_image | 单头文件的图像加载库 |
| TinyObjLoader | 轻量的 OBJ 解析器，支持 MTL 材质 |
| Tracy | 实时帧性能分析器，支持采样、GPU zone 和锁竞争追踪 |
| ufbx | 用于几何体、骨骼和动画的 FBX 加载器 |
| Volk | 用于运行时函数加载的 Vulkan 元加载器 |
| Vulkan | 提供显式 GPU 控制的底层图形 API |
| VulkanMemoryAllocator | Vulkan 内存管理库，支持碎片整理 |
| xxHash | 极快的非加密哈希算法 |

---

## 致谢

Lumina 受到以下开源引擎的启发并从中学习：

- [Spartan Engine](https://github.com/PanosK92/SpartanEngine) - 功能丰富的 Vulkan 引擎
- [Kohi Game Engine](https://kohiengine.com/) - 教学性质的引擎系列
- [Lumix Game Engine](https://github.com/nem0/LumixEngine) - 完整可用的独立引擎
- [ezEngine](https://ezengine.net/) - 专业的开源引擎
- [Godot](https://godotengine.org/) - 高质量的开源引擎
- [Unreal Engine](https://www.unrealengine.com/) - 业界标准的引擎

感谢游戏引擎开发社区分享知识与资源。

---

## 许可证

Lumina 采用 [Apache 2.0 License](LICENSE) 授权。

```
Copyright 2024 Dr. Elliot

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

---

## 联系方式

- 博客：[dr-elliot.com](https://www.dr-elliot.com)
- Discord：[加入社区](https://discord.gg/xQSB7CRzQE)
- GitHub：[mrdrelliot/lumina](https://github.com/mrdrelliot/lumina)
