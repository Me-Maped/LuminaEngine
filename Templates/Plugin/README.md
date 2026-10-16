# $PLUGINNAME

$PLUGINDESCRIPTION

一个由编辑器脚手架生成的 Lumina 插件。它包含两个模块：

- **$RUNTIMEMODULE**（`Runtime`），在编辑器和打包后的
  游戏中都会加载的游戏逻辑代码。在这里添加被反射的组件/系统、脚本绑定等。
- **$EDITORMODULE**（`Editor`），仅编辑器使用的自定义内容，会从
  打包构建中剔除。在这里注册资源编辑器工具、属性自定义和
  菜单。

## 布局

```
$PLUGINNAME/
├── $PLUGINNAME.lplugin            描述符（模块、加载阶段）
└── Source/
    ├── $RUNTIMEMODULE/            源文件以及 $RUNTIMEMODULE.Build.cs
    └── $EDITORMODULE/             源文件以及 $EDITORMODULE.Build.cs
```

每个模块目录都在其源文件旁持有自己的 `.Build.cs`；该文件
即为该模块的完整构建配置。

## 构建

插件会在所属项目生成其 IDE 文件时被发现。
创建插件之后，运行该项目的 `GenerateProject.bat`（或使用编辑器的
New Plugin 流程，它会为你完成这一步），然后重新构建。新的模块 DLL 会在
下一次启动编辑器时加载。

使用模块的 `<MODULENAME>_API` 宏从模块中导出类型，正如
生成的模块头文件所做的那样。该宏由构建系统定义；没有任何头文件
需要编辑，也没有任何东西需要声明。

## 启用 / 禁用

在编辑器的 **Plugin Browser** 中按项目切换该插件，它会向
项目的 `.lproject` 写入一个条目。
