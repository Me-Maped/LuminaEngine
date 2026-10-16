# Lumina ImGui vendor

The core is **Dear ImGui 1.93.1 WIP (docking)**, integrated on 2026-10-16 from:

- Me-Maped/imgui master: `101bb0511ad5a0fa5b7fe95182245ecc4521854a`.
- ocornut/imgui docking: `d88f238d1d363ef387382b2e29ad12cf96f826ac`.
- Old docking baseline for the engine patch: `b48d1afbe8ee8b238e2961dc363a949dd7304e23`.

The two new source branches were merged in a temporary checkout. Core files merged
cleanly; documentation-only conflicts in that temporary checkout were not imported.
The engine's old-base differences were then three-way merged onto the new sources.
Core custom additions/deletions were compared before and after the initial migration and retained.
A subsequent regression test identified a pre-existing elastic-layout bug after a 16-bit
index segment rollover; layout ranges now use absolute VtxBuffer indices.
Trailing whitespace introduced by upstream patches was cleaned.

## Engine contracts

- Docking and multiple platform windows remain enabled.
- Elastic layouts (`BeginHorizontal`, `BeginVertical`, `Spring`, layout stacks) and
  the `PushFontSize` convenience API remain in the core.
- `imconfig.h` retains `IMGUI_DISABLE_OBSOLETE_FUNCTIONS` and `IMGUI_USE_WCHAR32`.
- GLFW retains native frame/caption-hit-test callbacks, mouse passthrough caching,
  and monitor change callbacks plus 250 ms polling. A hidden-cursor array-index and
  multi-viewport cache regression in the new upstream code is fixed locally.
- ImPlot, ImGuizmo and node-editor are independent vendored add-ons; they were not
  replaced by an ImGui directory copy. Their affected drawing/API calls are migrated.
- `misc/themes` is the separately imported application-side theme module. Its
  source and original tests remain pinned to the fork commit above.

The engine renders through `Lumina_RHI`, not the official Vulkan renderer. The latter
and other optional backends are updated as coherent source sets but only the selected
GLFW/Vulkan sources are in `ImGui.Build.cs`.

## Validation entry points

From the engine root:

```sh
python3 BuildScripts/Tests/RunImGuiThemeTests.py
python3 BuildScripts/Tests/RunImGuiUpgradeTests.py
python3 BuildScripts/Tests/RunImGuiConsumerTests.py
python3 BuildScripts/Tests/RunImGuiGlfwCursorTests.py
./LuminaBuild.sh Build Lumina -TargetType=Editor
```

See `docs/imgui-1.93-integration.md` for behavior changes, actual validation results
and platform checks still requiring interactive acceptance.
