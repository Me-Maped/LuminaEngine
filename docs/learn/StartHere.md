# 第一天：从零到可调试项目

[返回学习路线](README.md) · [逐周计划](WeeklyPlan.md) · [记录模板](Progress.md)

目标不是一天学会架构，而是建立一个以后可以反复验证想法的环境。首次依赖下载、编译耗时不计入阅读时间；遇到硬件或工具链阻塞，先解决，不要盲目修改引擎源码。

## 0. 开始前：15 分钟

- [ ] 将 [Progress.md](Progress.md) 复制到自己的笔记位置，填写环境。
- [ ] 保存当前工作，检查 `git status --short`；不要覆盖已有修改。
- [ ] 记录 `git rev-parse HEAD`，以后所有笔记关联到这个版本。
- [ ] 为实验创建自己的 branch，例如 `learn/interaction`，不要执行 reset/clean 来“清理环境”。
- [ ] 准备引擎目录外的项目目录，例如 `~/LuminaProjects/EngineLearning`。

以下命令分两种 shell：Linux 使用 Bash；Windows 使用 Developer Command Prompt（cmd）。不要把 cmd 的 `%VAR%` 语法粘进 PowerShell。

## 1. 检查环境：20～40 分钟

以 [主 README 的环境要求](../../README.md#环境要求) 为准。当前仓库主 README 要求 Linux GCC 16+、.NET 10 SDK；Windows 为 Visual Studio 2026/18.0+ 与指定 MSVC 工具集、.NET 10 SDK。不要仅依赖 Blank 模板中可能滞后的 GCC 版本说明。

### Linux / Bash

在引擎根目录执行：

```bash
pwd
git rev-parse HEAD
g++ --version
dotnet --info
vulkaninfo --summary
```

- `vulkaninfo` 命令不存在：先按主 README 安装工具；这与 GPU 不支持是两回事。
- 默认 `g++` 不是目标版本：查看实际 build 日志采用的 compiler，不要凭系统默认输出认定工具链正确。
- Editor 需要支持 `VK_EXT_mesh_shader` 的 GPU 与足够 VRAM。`--summary` 用于确认设备和 driver，**不单独证明** extension 满足要求；让 Setup/设备选择检查进一步确认。
- 不把 `-ignoreminspec` 当作正式解决方案。

### Windows / cmd

```bat
where dotnet
dotnet --info
```

在 Visual Studio Installer 检查版本和主 README 所列 workload/toolset。仅有独立 .NET SDK 并不等于 IDE 使用的 MSBuild 可以构建对应 framework。

### 本阶段成功信号

你知道缺的是哪一层：系统包、compiler、SDK、driver、GPU feature 或 VRAM。将错误全文记录下来，不只保存最后一行“Build failed”。

## 2. Setup、构建和启动

以下命令均从**引擎根目录**执行；按顺序运行，前一条失败则先停下处理。

### Linux / Bash

```bash
./Setup.sh
./GenerateProjectFiles.sh
./LuminaBuild.sh Build Lumina -TargetType=Editor -Configuration=Development
./LuminaBuild.sh Run Lumina -TargetType=Editor -Configuration=Development
```

为后续项目脚本在当前 shell 设置引擎位置：

```bash
export LUMINA_DIR="$(pwd)"
test -f "$LUMINA_DIR/LuminaBuild.sh"
```

该 export 只作用于当前 shell；新终端需要重新设置或采用 Setup 提供的持久化方式。

### Windows / cmd

```bat
Setup.bat
GenerateProjectFiles.bat
LuminaBuild.bat Build Lumina -TargetType=Editor -Configuration=Development
LuminaBuild.bat Run Lumina -TargetType=Editor -Configuration=Development
```

若当前终端还没有获取到 Setup 持久化的环境变量，可以在引擎根目录设置当前终端：

```bat
set "LUMINA_DIR=%CD%"
```

命令依据：[RunMode.cs](../../Engine/Tools/LuminaBuildTool/Modes/RunMode.cs)。`Run` 解析所选 target 的启动配置，但**不会替你先完成 Build**。

### 成功信号

- [ ] Build 正常结束，记录产物路径。
- [ ] Editor 能打开，并能正常退出。
- [ ] 找到本次实际日志文件，记录路径。
- [ ] 再执行同配置 Build，观察哪些 action 仍运行，理解首次生成与增量行为；暂不要求绝对零 action。

不要一上来切 Shipping。先用 Development；需要更完整的调试信息时再切 Debug，并保持 Build/Run 配置一致。

## 3. 创建独立学习项目

推荐通过 Editor 的项目创建界面创建 **EngineLearning**，目标位置放在引擎目录外，使用 Blank 模板。UI 具体入口以当前版本为准；若只有打开项目功能，先阅读项目创建实现，不要假设复制目录就完成了生成。

> 原始 `Templates/Blank/` 含 `$PROJECTNAME`、`$PROJECTGUID` 等占位符，文件名也含 token。不要把原始模板目录直接当成可构建项目。手工创建时必须同时处理文件名、文本与 GUID；优先使用现有 Editor 流程。

检查项目至少包含：

```text
EngineLearning/
├── EngineLearning.lproject
├── GenerateProject.bat
├── GenerateProject.sh
├── Source/
│   ├── EngineLearning.Build.cs
│   └── EngineLearning.Target.cs
└── Game/
    └── Scripts/
```

参考：[Blank 项目说明](../../Templates/Blank/README.md)、[Target 模板](../../Templates/Blank/Source/$PROJECTNAME.Target.cs)。只用预编译引擎创建的项目可能没有 C++ module；本课程应使用源码版项目。

### Linux / Bash：从项目根目录运行

```bash
./GenerateProject.sh
"$LUMINA_DIR/LuminaBuild.sh" Build EngineLearning -Project="$PWD" -TargetType=Editor -Configuration=Development
"$LUMINA_DIR/LuminaBuild.sh" Run EngineLearning -Project="$PWD" -TargetType=Editor -Configuration=Development
```

### Windows / cmd：从项目根目录运行

```bat
GenerateProject.bat
"%LUMINA_DIR%\LuminaBuild.bat" Build EngineLearning -Project="%CD%" -TargetType=Editor -Configuration=Development
"%LUMINA_DIR%\LuminaBuild.bat" Run EngineLearning -Project="%CD%" -TargetType=Editor -Configuration=Development
```

若采用其他项目名，替换命令里的 `EngineLearning`。这里显式传 `-Project`，避免误解析为引擎自己的 target。项目 target 的 Run 通常指向加载项目的 Editor，不是直接执行游戏 module library。

### 成功信号

- [ ] `.lproject` 的 Name、文件名与 target 命名匹配。
- [ ] 生成后的项目中不再有未替换的模板 token。
- [ ] Editor 加载的是学习项目，而不是空的引擎窗口。
- [ ] 可以保存场景并在重开后找回。

## 4. 建立 debugger：30～60 分钟

Windows 可用生成的 solution；Linux 的 `compile_commands.json` 提供代码索引，**不自动等于完整 Run/Debug 配置**。使用 CLion 或其他 debugger 时，分别设置可执行文件、参数、工作目录和 symbols，依据 Build/Run 日志确认，而不是猜测最后一次生成了哪个 binary。

设置断点：

| 断点 | 源码 | 观察内容 |
| --- | --- | --- |
| `LuminaMain` | [Launch.cpp](../../Engine/Applications/Lumina/Source/Launch.cpp) | Editor/Game 分支与全局入口 |
| `FApplication::Run` | [Application.cpp](../../Engine/Source/Runtime/Source/Core/Application/Application.cpp) | window、输入与主循环 |
| `FEngine::Init` | [Engine.cpp](../../Engine/Source/Runtime/Source/Core/Engine/Engine.cpp) | subsystem 创建顺序 |
| `FEngine::Update` | 同上 | 当前 frame 与 update stage |
| `FEngine::Shutdown` | 同上 | 退出与资源释放顺序 |

不要让每帧断点持续卡住渲染；命中一次、记录调用栈后禁用。超时/watchdog 或 GPU 错误不一定意味着业务逻辑错误，要记录停顿背景。

最小实验：在你自己的项目 module 启动回调里增加一条 `LOG_INFO`，重新构建并启动，验证日志和断点都命中。不要为了输出日志改引擎核心。

## 5. 第一天交付物

- [ ] 环境和 commit 记录。
- [ ] 引擎与项目各一份成功 Build/Run 记录。
- [ ] 自己项目 module 的日志或调用栈。
- [ ] 一张不超过 10 个节点的启动流程图。
- [ ] 写下两个未理解的问题，安排到 [第 1 周任务](WeeklyPlan.md)。

完成后进入 [Interaction 实践手册](InteractionWorkshop.md) 的 P0。没有 GPU 条件时可以先读源码和构建工具，但标记 Editor/渲染验收为“阻塞”，不能写成通过。

## 常见阻塞处理

| 现象 | 先检查 | 不要做 |
| --- | --- | --- |
| 找不到引擎 | `LUMINA_DIR` 是否指向根目录，是否在当前 shell 生效 | 修改脚本中的固定路径 |
| 找不到 target | 项目名、`.Target.cs`、`-Project` | 改引擎 target 来迁就项目 |
| 找不到 `.generated.h` | Reflector 是否执行、前序诊断、规则是否正确 | 手写 generated 文件 |
| C# bootstrap 缺失 | managed build/restore 的首个错误 | 把脚本失效当作正常情况跳过 |
| module 未加载 | descriptor 名、HostType、LoadingPhase、插件启用状态 | 仅反复重编译 |
| 断点不命中 | 配置、实际 binary、symbols、module 是否已加载 | 无证据地认定优化器有问题 |
| GPU 不满足要求 | driver、设备枚举、extension、VRAM | 默认使用绕过参数 |

保存失败日志，再参考 [主 README 故障排查](../../README.md#故障排查)。避免未经备份就删除全部输出或修改依赖版本。
