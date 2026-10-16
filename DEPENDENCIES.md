# 外部依赖

Setup 会从本仓库的 GitHub Releases（`external-deps` 标签）为每个平台拉取一个预编译依赖包，
并解压到 `External/`。它们是 release asset，而不是提交进仓库的文件，因此不会进入 git 历史，
也不走 LFS。`External/` 已被 git 忽略。

| 宿主平台 | 资源 | 状态 |
|---|---|---|
| Windows | `External-Win64.zip`（约 192 MB） | 已发布 |
| Linux | `External-Linux64.tar.gz`（约 186 MB） | 已发布 — 可用 [`BuildScripts/MakeLinuxBundle.sh`](BuildScripts/MakeLinuxBundle.sh) 重新构建 |

这些依赖包被设计为**可以共存于同一个 `External/` 目录树中**，因此在同时为两个平台构建的机器上
两者都会安装，且互不覆盖。之所以不需要任何平台子目录方案就能做到，是因为其中的内容不会在文件名上
冲突：.NET 运行时已经按 RID 划分（`runtime/win-x64`、`runtime/linux-x64`，与 `DotNetHost` 的
`RuntimeRid()` 一致），其余文件则靠扩展名区分。只有头文件是共享的，而它们完全相同。

Setup 判断某个依赖包是否已安装，靠的是探测宿主平台的单个哨兵文件 ——
`LLVM/bin/libclang.dll` 或 `LLVM/lib/libclang.so` —— 而不是仅仅检查 `External/` 是否存在，
因此一个为其他平台准备好的检出仍会去拉取当前宿主所需的东西。

Linux 依赖包是 **tarball 而不是 zip**，这是要求而非偏好。Slang 的 Linux 库使用带版本号的
文件名，并用普通名称的符号链接指向它们（`libslang.so` → `libslang-compiler.so.0.x.y`），
libclang 也以同样的方式打包；二进制中记录的 SONAME 是带版本号的那个，因此把这些链接展平为
副本会导致加载器报错，说找不到一个明明就在眼前的库。zip 还会丢失可执行位，而 `slangc`、
Tracy 工具和 .NET 宿主都需要它。`ZipFile.ExtractToDirectory` 两者都恢复不了，
因此 Linux 路径改用 `TarFile`。

依赖包中的所有内容都是开源的，并列在下方。若要跳过下载，
可以自行从各上游获取每个库并放入 `External/`（见
[自行构建](#自行构建)）。

## 内容

| 库 | 路径（大小） | 用途 | 许可证 |
|---|---|---|---|
| .NET 10 runtime + hosting headers | `External/DotNet`（78 MB） | 用于 C# 脚本（LuminaSharp）的 CoreCLR 宿主 | MIT（头文件）；.NET 可再发行条款（运行时） |
| LLVM / Clang 19 (libclang) | `External/LLVM`（337 MB） | Reflector 解析 C++ 头文件以生成反射代码 | Apache-2.0 WITH LLVM-exception |
| Slang | `External/SLang`（155 MB） | 将 `.slang` shader 编译为 SPIR-V | Apache-2.0 WITH LLVM-exception |
| RenderDoc | `External/RenderDoc`（24 MB） | 应用内 GPU 帧捕获 | MIT |
| Tracy | `External/Tracy`（77 MB） | CPU/GPU 性能分析器 | BSD-3-Clause |

## 上游来源

- **.NET 10.0.2**（win-x64）：<https://github.com/dotnet/runtime>，构建见 <https://dotnet.microsoft.com/download/dotnet/10.0>。头文件为 MIT（`src/native/corehost`）；运行时为 Microsoft 的可再发行组件。
- **LLVM / Clang 19.x**（提交 `faef8b4`）：<https://github.com/llvm/llvm-project>。从源码构建，因为官方 Windows 安装包附带的 `libclang.dll` 缺少链接所需的 import lib 和头文件。
- **Slang**（std module 2026.3.1）：<https://github.com/shader-slang/slang>。`slang-llvm.dll` 内置了 LLVM，`slang-glslang.dll` 封装了 glslang，`gfx.dll` 是 Slang 已弃用的图形层。
- **RenderDoc**：<https://github.com/baldurk/renderdoc>，构建见 <https://renderdoc.org/builds>。以动态方式加载，因此本地安装的版本也可使用。
- **Tracy**：<https://github.com/wolfpld/tracy>。

## 校验

Setup 在解压前会用 `SetupMode.Bundles`
（`Engine/Tools/LuminaBuildTool/Modes/SetupMode.cs`）中固定的 SHA-256 对每个依赖包进行校验。
哈希保存在仓库中而非下载文件旁边，因此被篡改或掉包的文件会被拒绝。你也可以自行校验：

```bash
sha256sum External-Linux64.tar.gz
```

```bat
powershell -NoProfile -Command "(Get-FileHash External-Win64.zip -Algorithm SHA256).Hash"
```

固定的哈希为空表示该平台尚未发布依赖包，此时 Setup 会如实告知，而不是去下载 release 为缺失资源
所提供的任何东西。解压之后，Setup 会再次检查哨兵文件，因此平台错误或不完整的资源会在 setup
阶段失败，而不是在构建期间。

## 自行构建

### Linux

[`BuildScripts/MakeLinuxBundle.sh`](BuildScripts/MakeLinuxBundle.sh) 负责此事。请在 Linux
（或 WSL）上运行它 —— 包内带有符号链接和可执行位，而从 Windows 上打包这两者都无法保留：

```bash
BuildScripts/MakeLinuxBundle.sh
```

它会拉取各个上游、按布局放置它们、**在打包前校验布局**，并打印出可供固定的 SHA-256。
校验才是关键部分：它检查的每一条路径都是构建时按名称解析的路径，因此缺失或被改名的文件会在
拥有这些文件的机器上就被发现，而不会在之后表现为链接失败或无法启动的 shader 编译。

LLVM 来自发行版的 `libclang-$MAJOR-dev` 包，而不是 llvm.org 的 tarball。LLVM release 上的
资源命名一直在变动（较早的版本是 `clang+llvm-<v>-x86_64-linux-gnu-ubuntu-<rel>`，
较近的是 `LLVM-<v>-Linux-X64`），而且并非所有版本都在 `clang-c` 头文件旁附带 `libclang.so`；
`apt.llvm.org` 在每个发行版上都提供每个版本，这正是让这套配方可复现、而不依赖宿主默认 clang 的原因。

### 依赖包的运行时依赖

依赖包包含 `libLLVM` 和 `libclang` 链接所需的一切 —— libxml2、ICU、libedit、libffi、
libzstd、liblzma、libtinfo、libbsd、libmd、zlib —— 并将它们放置在它们旁边的 `External/LLVM/lib` 中。
这并非多余的双保险：LLVM 构建时所依赖的版本并不是当前发行版所附带的版本。Ubuntu 26.04 拥有
`libxml2.so.16`，而那里任何可安装的东西都无法满足在 22.04 上构建的 LLVM 19 所需的
`libxml2.so.2`，所以没有任何 `apt-get` 命令能解决这一点。

`Reflector.Build.cs` 在链接时通过 `-Wl,-rpath-link` 找到它们，在加载时通过相对于 `$ORIGIN` 的
`-Wl,-rpath` 找到它们。单纯的 `-L` 无法应对共享库的传递依赖需求。

### Clang 的内建头文件

`lib/clang/<major>/include` 存放 clang 自身提供的头文件 —— `stddef.h`、`float.h` 以及
`*intrin.h` 家族。在 Linux 上没有任何其他东西提供它们：glibc 不附带这些，而 libclang 是相对于
*宿主可执行文件*来解析其 resource 目录的，所以仅仅把一份副本放在 `libclang.so` 旁边也不会被找到。
因此 Reflector 会显式传入 `-resource-dir`，指向它从已加载的 `libclang.so` 定位到的目录。

Windows 不需要它们，这就是为什么漏掉它们只会导致 Linux 构建失败：clang 会从它自动探测到的
MSVC 和 Windows SDK 头文件中获取 `stddef.h`、`float.h` 和 intrinsics。缺少它们时，Linux 构建会在
反射生成阶段失败，报出 `ptrdiff_t`、`NULL` 和 `FLT_MAX` 未声明以及找不到 `immintrin.h` ——
这些错误指向的是引擎头文件，而不是缺失的依赖包目录。

核心运行时 —— glibc、libstdc++、libgcc —— 故意**不**打包。把它们与宿主工具链放在一起存在
ABI 风险，而且每个发行版都有它们。

要查看某个副本还需要什么：

```bash
ldd External/LLVM/lib/libLLVM.so.19.1 | grep "not found"
```

### 手动获取

从各上游获取每个库，并按构建的路径要求摆放它们。
必需项：

```
External/LLVM/include/clang-c/            Reflector 的解析头文件
External/LLVM/{bin,lib}/libclang.{dll,so} Reflector 会链接并归置它（不可省略）
External/LLVM/lib/clang/<major>/include/  clang 的内建头文件（仅 Linux，见下）
External/SLang/lib/                       libslang.so / import libs
External/SLang/bin/                       slangc，以及 Windows 上的一整套 .dll
External/DotNet/include/                  hostfxr.h, nethost.h, coreclr_delegates.h
External/DotNet/runtime/<rid>/            按 RID 划分：win-x64, linux-x64
```

可选项 —— 缺少它们只会降级而不会中断：

```
External/RenderDoc/renderdoc.dll | librenderdoc.so    应用内 GPU 捕获
External/Tracy/                                       性能分析器 UI 二进制
```

然后运行 `GenerateProjectFiles.bat`。只要当前宿主的哨兵文件已存在，Setup 就会跳过下载。

## 许可证合规

再分发该依赖包时，请在每个库的二进制旁边附上其 `LICENSE`/`NOTICE`：
LLVM 和 Slang 为 Apache-2.0，Tracy 为 BSD，RenderDoc 和 .NET 头文件为 MIT，
外加 .NET 的 `THIRD-PARTY-NOTICES.txt`。.NET 运行时二进制受 Microsoft 再分发条款约束，
而非 MIT。

## setup 还做了哪些事

- 在 `HKCU\Software\Classes` 下注册 `.lproject` 和 `.lplugin` 的图标，以及 `.lproject` 的右键菜单动词。
- 将 `core.hooksPath` 指向 [`BuildScripts/Hooks`](BuildScripts/Hooks)。唯一的 hook 是 `post_merge`，它会在合并后清空 `Binaries/`、`Intermediates/` 以及过期的 IDE 文件。
- 通过调用 `LuminaBuild.bat GenerateProjectFiles` 生成 IDE 项目文件。

## 更新依赖包

```bat
:: replace the asset (the URL stays the same)
gh release upload external-deps External-Win64.zip --clobber
gh release upload external-deps External-Linux64.tar.gz --clobber
```

然后重新固定哈希：把哈希粘贴到对应条目在 `SetupMode.Bundles` 中的 `Sha256` 字段，
并与上传该资源的提交放在同一个 commit 中。仅在首次时需要创建 release：
`gh release create external-deps <asset> --title "External dependencies"`。

两个资源都命名为 `External-<Platform>.<format>`。格式不同是因为内容不同：Linux 的那个
必须是 tarball，才能保留符号链接和可执行位，而 zip 会丢失它们。

该 release 目前仍保留着旧的、无后缀的 `External.zip`。请留着它。它的 URL 被固化在重命名之前
创建的每个检出中，删除它会让所有这些检出的 Setup 失效；把它保留为旧别名没有任何代价。
