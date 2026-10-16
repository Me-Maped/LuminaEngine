#pragma once

#include "imgui/misc/themes/imgui_themes.h"
#include <cmath>

namespace Lumina::EditorThemes
{
    inline float ClampScale(float Scale)
    {
        if (!std::isfinite(Scale))
        {
            return 1.0f;
        }
        return Scale < 0.5f ? 0.5f : (Scale > 4.0f ? 4.0f : Scale);
    }

    // Pure style composition, also exercised without creating an engine/window/RHI.
    inline bool ApplyStyle(const ImGuiStyle& LuminaStyle, const ImGuiThemes::Theme* Preset,
        float Scale, bool MultiViewport)
    {
        if (!std::isfinite(Scale) || Scale < 0.5f || Scale > 4.0f)
        {
            return false;
        }
        ImGuiStyle& Style = ImGui::GetStyle();
        const ImGuiStyle Previous = Style;
        Style = LuminaStyle;
        Style.ScaleAllSizes(Scale);
        Style.FontScaleMain = Scale;
        if (Preset)
        {
            if (!ImGuiThemes::Apply(*Preset, Scale))
            {
                Style = Previous;
                return false;
            }
            // The upstream theme module targets master, which has no docking colors.
            if (Preset->id != ImGuiThemes::Id::Default)
            {
                Style.Colors[ImGuiCol_DockingPreview] = Preset->accent;
                Style.Colors[ImGuiCol_DockingPreview].w = 0.7f;
                Style.Colors[ImGuiCol_DockingEmptyBg] = Preset->style.Colors[ImGuiCol_WindowBg];
            }
            if (MultiViewport)
            {
                Style.WindowRounding = 0;
                Style.Colors[ImGuiCol_WindowBg].w = 1;
            }
        }
        return true;
    }

    // The renderer publishes the applied preset at the frame boundary, shared across DLLs.
    // nullptr means the user's original Lumina palette is active.
    RUNTIME_API const ImGuiThemes::Theme* GetActivePreset();
}
