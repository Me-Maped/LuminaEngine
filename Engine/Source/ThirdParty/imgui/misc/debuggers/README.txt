
-----------------------------------------------------------------------------
 面向常用调试器的辅助文件
-----------------------------------------------------------------------------

imgui.gdb
    GDB：禁止步入（stepping into）琐碎函数 (*1)。
    （详情请阅读文件内部的注释）

imgui.natstepfilter
    Visual Studio 调试器：禁止步入琐碎函数 (*1)。
    （详情请阅读文件内部的注释）

imgui.natvis
    Visual Studio 调试器：描述 Dear ImGui 类型以获得更好的显示效果。
    借助它，ImVector<> 等类型将在调试器中良好显示。
    （详情请阅读文件内部的注释）

imgui_lldb.py
    基于 LLDB 的调试器 (*2)：为 Dear ImGui 类型提供 synthetic children provider 和摘要。
    借助它，ImVector<> 等类型将在调试器中良好显示。
    （详情请阅读文件内部的注释）
    (*2) Xcode、Android Studio，也可用于 VS Code、C++Builder、CLion、Eclipse 等。

-----------------------------------------------------------------------------
 新功能！禁止步入琐碎函数
-----------------------------------------------------------------------------

自 1.93.0 WIP（2026 年 9 月）起，
Dear ImGui 使用 IM_NODEBUGSTEP 宏为函数标记 __declspec(non_user_code)
或 [[gnu::artificial]]，以自动告知调试器跳过它们。

(*1) 假设新的 IM_NODEBUGSTEP 宏有效，那么使用 imgui.natstepfilter 等
可能就不再必要。对用户体验的确切副作用尚待明确。

更多参考：
    https://maskray.me/blog/skipping-boring-functions-in-debuggers
    以及 https://news.ycombinator.com/item?id=42547576
