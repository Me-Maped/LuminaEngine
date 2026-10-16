# GameplayExtras

可选的游戏玩法类组件，不属于引擎核心功能。
这里的所有内容都可以按项目通过 `.lproject` 的
`Plugins` 数组禁用，Runtime 本身从不依赖它。

## 当前内置

- `SHealthComponent`（`Components/HealthComponent.h`），生命值 + 最大生命值
  + 可选回复。已反射的脚本 API：`ApplyDamage(float)`、`GiveHealth(float)`。

## 如何为某个项目禁用

在你的 `.lproject` 中：

```json
"Plugins": [
    { "Name": "GameplayExtras", "Enabled": false }
]
```

或使用 **Tools → Plugin Browser** 编辑器工具。

## 插件 API 头文件的模式

`GameplayExtrasRuntimeAPI.h` 采用三模式写法，每个插件都应照此
编写，从而同时适用于模块化（Debug/Development）和单体
（Shipping）构建：

```cpp
#pragma once
#ifdef LUMINA_MONOLITHIC
    #define MYMODULE_API
#elif defined(MYMODULE_EXPORTS)
    #define MYMODULE_API DLL_EXPORT
#else
    #define MYMODULE_API DLL_IMPORT
#endif
```

`LUMINA_MONOLITHIC` 由 LuminaBuildTool 为单体目标定义；
在该配置中每个模块都被静态链接进最终的可执行文件，
dllexport/dllimport 只会产生 LNK4286 警告。
