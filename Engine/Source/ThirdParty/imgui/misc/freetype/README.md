# imgui_freetype

使用 FreeType 而非 stb_truetype（默认的字体光栅化器）来构建字体图集。
<br>作者：@vuhdo、@mikesart、@ocornut。

### 用法

1. 获取最新的 FreeType 二进制文件，或自行构建（在 Windows 下可以使用 vcpkg：`vcpkg install freetype --triplet=x64-windows`、`vcpkg integrate install`）。
2. 将 imgui_freetype.h/cpp 与你的项目文件放在一起。
3. 在你的 [imconfig.h](https://github.com/ocornut/imgui/blob/master/imconfig.h) 文件中添加 `#define IMGUI_ENABLE_FREETYPE`，让 Dear ImGui 自动使用 imgui_freetype 加载器。如果你使用的 Dear ImGui 是预编译的，那么始终可以通过调用 ImFontAtlas::SetFontLoader() 来启用 imgui_freetype。

### 关于 Gamma 校正混合

FreeType 假定混合在线性空间而非 gamma 空间中进行。
请参阅 FreeType 关于 [FT_Render_Glyph](https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_render_glyph) 的说明。
要获得正确的结果，你需要使用 sRGB，并在像素着色器输出中转换到线性空间。
这一改动会影响 Dear ImGui 的默认样式（需要调整 alpha 值）。

### 用于试验各项设置的测试平台（供开发者使用）

请参阅 https://gist.github.com/ocornut/b3a9ecf13502fd818799a452969649ad

### 已知问题

- 超采样（oversampling）设置会被忽略，但在更高质量的渲染下也不太必要。

### 对比

细小、纤细的抗锯齿字体通常能从 FreeType 的 hinting 中获益很多：
![comparing_font_rasterizers](https://user-images.githubusercontent.com/8225057/107550178-fef87f00-6bd0-11eb-8d09-e2edb2f0ccfc.gif)

### 彩色字形/表情符号

你可以使用 `ImGuiFreeTypeBuilderFlags_LoadColor` 标志来加载某些彩色字形。请参阅 FONTS.md 的
["Using Colorful Glyphs/Emojis"](https://github.com/ocornut/imgui/blob/master/docs/FONTS.md#using-colorful-glyphsemojis) 章节。

![colored glyphs](https://user-images.githubusercontent.com/8225057/106171241-9dc4ba80-6191-11eb-8a69-ca1467b206d1.png)

### 使用 OpenType SVG 字体 (SVGinOT)
- *SVG in Open Type* 是 Adobe 和 Mozilla 制定的标准，适用于彩色 OpenType 和 Open Font Format 字体。它允许字体创作者在字体中嵌入完整的 SVG 文件，从而实现全彩甚至动画。
- 诸如 [twemoji](https://github.com/13rac1/twemoji-color-font) 之类的流行字体，以及用 [scfbuild](https://github.com/13rac1/scfbuild) 制作的字体，都是 SVGinOT。
- 渲染 SVG 字体有两种可选方案：使用 "lunasvg" 或 "plutosvg"。plutosvg 会多支持一些字体（例如 NotoColorEmoji-Regular），并且加载它们可能更快。

#### 使用 lunasvg
要求：[lunasvg](https://github.com/sammycage/lunasvg) v2.3.2 及以上
- 在你的 `imconfig.h` 中添加 `#define IMGUI_ENABLE_FREETYPE_LUNASVG`。
- 获取最新的 lunasvg 二进制文件，或自行构建。在 Windows 下可以使用 vcpkg：`vcpkg install lunasvg --triplet=x64-windows`。

#### 使用 plutosvg（以及 plutovg）
- 在你的 `imconfig.h` 中添加 `#define IMGUI_ENABLE_FREETYPE_PLUTOSVG`。
- 获取最新的 plutosvg 二进制文件，或自行构建。在 Windows 下可以使用 vcpkg：`vcpkg install plutosvg --triplet=x64-windows`。或者，如果你通过 vcpkg 构建 imgui，只需启用 plutosvg 特性：`vcpkg install imgui[plutosvg] --triplet=x64-windows`
- 如果你倾向于手动构建 plutosvg：
    - plutovg 的编译提示：编译 `plutovg/source/*.c` 中的所有源文件 + 添加包含目录：`plutovg/include` + `plutovg/stb`
    - plutosvg 的编译提示：编译 `plutosvg/source/plutosvg.c` + 添加包含目录：`plutosvg/source` + 添加定义：`PLUTOSVG_HAS_FREETYPE` + 链接：plutovg、freetype
