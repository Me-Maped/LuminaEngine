# $PROJECTNAME

一个 Lumina Engine 项目。

## 打开它

从引擎文件夹运行 `Lumina Editor.exe`，然后在项目列表中选择本项目，或使用
**Browse for project file** 并选择 `$PROJECTNAME.lproject`。

## 脚本（C#）

游戏逻辑使用 **C#** 配合 `LuminaSharp` 编写。脚本位于 `Game/Scripts/`，并且**在编辑器内部编译**：编辑 `.cs`、保存，改动即刻生效。无需构建步骤，也无需重启任何东西。

- 脚本是一个派生自 `EntityScript` 的类（见 `Game/Scripts/ExampleScript.cs`）。它可以获得 `Entity`、`World`、`Registry`、缓存的 `Transform`，以及生命周期钩子（`OnAttach` / `OnReady` / `OnUpdate` / `OnDetach`）外加输入和碰撞回调。
- 通过添加 **C# Script** 组件并选择脚本类，即可把脚本挂到某个实体上。标记了 `[Property]` 的字段会出现在检视器中。
- `Game/Scripts/<...>.Scripts.csproj` 是**生成**的，仅用于 IDE 的 IntelliSense。它会在项目每次加载时重新创建，因此绝不要提交它。

## 迭代

- **C# 脚本**（`Game/Scripts/*.cs`）：在你的编辑器中保存；运行中的引擎会重新编译并重新加载它们。
- **内容**（`Game/Content/` 中的资源）：在编辑器内热重载。

## 项目布局

```
$PROJECTNAME.lproject          项目描述符（名称、GUID、插件）
Config/GameSettings.json       按项目划分的引擎设置（启动地图、cook 根目录等）
Plugins/                       项目插件，每个一个文件夹（见 Plugins/README.md）
Game/Content/                  资源，以 /Game/Content 暴露给引擎
Game/Scripts/                  C# 脚本，在编辑器内编译（以 /Game/Scripts 暴露）
Logs/                          加载本项目运行时的引擎日志（Lumina.log，保留 5 份）
CrashDumps/                    加载本项目运行时的 minidump 和 GPU 崩溃转储
```

`Binaries/`、`Intermediates/`、`Logs/` 和 `CrashDumps/` 是生成产物，会被 git 忽略。

## C++

本项目是基于预编译的引擎创建的，因此它没有 C++ 模块。添加原生代码需要
从源码构建的引擎；工程师可以从这样的引擎向项目中添加一个 `Source/` 模块。
