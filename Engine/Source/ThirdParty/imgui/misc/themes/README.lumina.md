# Theme module provenance

Imported unchanged from `Me-Maped/imgui`, master commit
`101bb0511ad5a0fa5b7fe95182245ecc4521854a` (2026-10-16):

- `misc/themes/imgui_themes.h`
- `misc/themes/imgui_themes.cpp`
- `examples/example_theme_studio/tests.cpp` → `tests.cpp`

These application-side themes use the existing Dear ImGui license in this vendor tree.
The engine uses its integrated 1.93.1 WIP docking/custom core and backends. `ImGui.Build.cs`
explicitly compiles the theme implementation, not the tests or Theme Studio application.

Engine integration lives in `Runtime/Source/Tools/UI/ImGui/EditorTheme.h`,
`ImGuiRenderer.cpp`, `EditorColors.h`, and the editor Theme menu. The engine adapter
owns DPI composition and docking/multi-viewport presentation. Keep those adaptations
out of the imported module to make future updates reviewable.

From the engine root, run the original theme contracts and engine integration tests:

```sh
python3 BuildScripts/Tests/RunImGuiThemeTests.py
```

This compiles against the engine's actual core and `imconfig.h`; it does not validate
Vulkan presentation or editor interaction. See `docs/imgui-themes.md` for usage and
runtime acceptance checks.
