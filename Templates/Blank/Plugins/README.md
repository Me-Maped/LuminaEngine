# Plugins

项目插件放在这里，每个一个文件夹。插件是一个自包含的模块、内容与设置集合，
可以按项目启用和停用，因此它是你想在另一个项目中复用、或与游戏模块分开分发的
任何内容的理想归处。

该文件夹初始为空。它不是必需的，如果你永远不想要插件，删掉它即可。

## 添加插件

在编辑器中：**Tools > Plugin Browser > New Plugin**。给它起个名字，编辑器就会在此处
生成插件脚手架并重新生成你的项目文件。重新构建解决方案，新的模块会在
下一次启动编辑器时加载。

手动方式：把引擎安装目录中的 `Templates/Plugin` 复制到这个文件夹，重命名它，并把
文件名和内容中的 `$PLUGINNAME`、`$RUNTIMEMODULE` 和 `$EDITORMODULE` 占位符替换掉。然后
在项目根目录运行 `GenerateProject.bat`。

## 你会得到什么

一个名为 `Combat` 的插件会被脚手架生成为：

```
Plugins/Combat/
├── Combat.lplugin                  描述符：模块、加载阶段、是否默认启用
└── Source/
    ├── CombatRuntime/              在编辑器和打包后的游戏中都会加载
    │   ├── CombatRuntime.Build.cs
    │   └── CombatRuntimeModule.h/.cpp
    └── CombatEditor/               仅编辑器使用，会从打包构建中剔除
        ├── CombatEditor.Build.cs
        └── CombatEditorModule.h/.cpp
```

把游戏逻辑代码、被反射的组件与系统放进 Runtime 模块。把资源编辑器、
属性自定义和菜单项放进 Editor 模块。

每个模块的 `.Build.cs` 就是它的完整构建配置：在那里写明它的依赖，方式如同
`Source/$PROJECTNAME.Build.cs` 为你的游戏模块所做的那样。使用模块的
`<MODULENAME>_API` 宏从模块中导出类型；该宏由构建系统定义，因此没有头文件需要编辑。

## 命名

插件名在你的项目*和*引擎自带插件之间必须唯一。发现机制以名称为键，
因此名称冲突意味着其中一个会被静默丢弃。编辑器会提前拒绝重复的名称。

## 启用与禁用

插件的 `.lplugin` 决定它是否默认启用。在编辑器的
Plugin Browser 中按项目切换它，选择会被记录到 `$PROJECTNAME.lproject` 中。

被关闭的插件既不会被构建，也不会被加载。

## 在 IDE 中

项目插件会出现在生成解决方案中的 `Games/$PROJECTNAME/Plugins/<PluginName>`
下，与 `Games/$PROJECTNAME/Source` 并列。引擎自带的插件则保留在单独的一级
`Plugins` 文件夹下，因此始终能分清哪些是你自己的。

增删插件会改变解决方案，因此如果你是手动操作的，之后请重新运行 `GenerateProject.bat`。
编辑器的 New Plugin 流程会为你完成这一步。
