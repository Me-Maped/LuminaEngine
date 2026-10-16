
imgui_stdlib.h + imgui_stdlib.cpp
  InputText() 对 C++ 标准库 (STL) 类型 std::string 的封装。
  这也是一个示例，展示你可以如何为自己的类似类型做封装。

imgui_scoped.h
  [实验性，目前不在主仓库中]
  额外的头文件，包含一些针对常见 Dear ImGui 函数的 RAII 风格封装。
  试用方式：合并 https://github.com/ocornut/imgui/pull/2197
  讨论地址：https://github.com/ocornut/imgui/issues/2096

imgui-module:
  C++20 模块绑定
  https://github.com/stripe2933/imgui-module

在 Wiki 上查看更多 C++ 相关扩展（fmt、RAII、语法糖）：
  https://github.com/ocornut/imgui/wiki/Useful-Extensions#cness
