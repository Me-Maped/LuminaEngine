
misc/cpp/
  InputText() 对 C++ 标准库 (STL) 类型 std::string 的封装。
  这也是一个示例，展示你可以如何为自己的类似类型做封装。

misc/debuggers/
  面向常用调试器（Visual Studio、GDB、LLDB）的辅助文件。
  例如，借助 .natvis 文件，ImVector<> 等类型可在 Visual Studio 调试器中良好显示。

misc/fonts/
  字体加载/合并说明（例如如何处理字形范围、如何合并图标字体）。
  命令行工具 "binary_to_compressed_c"，用于创建压缩数组以便将数据嵌入源代码。
  推荐的字体与链接。

misc/freetype/
  使用 FreeType 而非 stb_truetype 的字体图集构建/光栅化程序。
  可获得更好的 FreeType 光栅化效果，尤其是对小字号字体。

misc/single_file/
  单文件头文件存根（stub）。
  我们用它来验证在同一编译单元中编译所有 *.cpp 文件。
  使用该技术（也称为 "Unity builds"）的用户通常可以自己提供这一文件，
  因此我们并不太建议你在自己的项目中使用它。

misc/themes/
  本仓库新增的应用层主题模块：五套可编辑主题与 00 原生只读基准。
  只依赖 imgui.h 与 C++ 标准库，提供外观应用、INI 配置和语义按钮封装。
  使用说明见 examples/example_theme_studio/README.md，验证截图见 docs/themes/README.md。
