#pragma once

#include "Config/EngineSettings.h"
#include "Core/Object/ObjectCore.h"
#include "imgui.h"
#include "EditorTheme.h"

// Lumina keeps its editable palette in preferences. Presets override presentation only, so switching
// back restores user colors. Read the applied preset rather than the pending setting or pushed ImGui
// colors: every module and every widget in a frame must see the same semantic palette.
namespace Lumina::EditorColors
{
    inline ImVec4 ToImVec4(const FVector4& C) { return ImVec4(C.x, C.y, C.z, C.w); }

    inline const CEditorColorSettings& Palette() { return *GetDefault<CEditorColorSettings>(); }

    inline ImVec4 PresetColor(ImGuiCol Color, const FVector4& Fallback)
    {
        const auto* Preset = EditorThemes::GetActivePreset();
        return Preset ? Preset->style.Colors[Color] : ToImVec4(Fallback);
    }

    inline ImVec4 Mix(const ImVec4& A, const ImVec4& B, float T)
    {
        return ImVec4(A.x + (B.x - A.x) * T, A.y + (B.y - A.y) * T, A.z + (B.z - A.z) * T, A.w + (B.w - A.w) * T);
    }

    inline ImVec4 Accent()
    {
        const auto* Preset = EditorThemes::GetActivePreset();
        if (Preset == nullptr)
        {
            return ToImVec4(Palette().Accent);
        }
        // Default aliases accent to body text. CheckMark is the actual accent of StyleColorsDark.
        if (Preset->id == ImGuiThemes::Id::Default)
        {
            return Preset->style.Colors[ImGuiCol_CheckMark];
        }
        return Preset->accent;
    }
    inline ImVec4 Danger()
    {
        const auto* Preset = EditorThemes::GetActivePreset();
        if (Preset == nullptr)
        {
            return ToImVec4(Palette().Danger);
        }
        // Default stores body text in danger. PlotLinesHovered is the dark style's error-adjacent color.
        if (Preset->id == ImGuiThemes::Id::Default)
        {
            return Preset->style.Colors[ImGuiCol_PlotLinesHovered];
        }
        return Preset->danger;
    }
    inline ImVec4 StateColor(bool IsWarning)
    {
        const auto* Preset = EditorThemes::GetActivePreset();
        if (!Preset)
        {
            return ToImVec4(IsWarning ? Palette().Warning : Palette().Success);
        }
        const bool Light = Preset->id == ImGuiThemes::Id::Minimal || Preset->id == ImGuiThemes::Id::Brutalist;
        return IsWarning ? (Light ? ImVec4(0.60f, 0.36f, 0.05f, 1) : ImVec4(0.95f, 0.75f, 0.30f, 1))
                         : (Light ? ImVec4(0.16f, 0.43f, 0.22f, 1) : ImVec4(0.40f, 0.82f, 0.45f, 1));
    }
    inline ImVec4 Success() { return StateColor(false); }
    inline ImVec4 Warning() { return StateColor(true); }
    inline ImVec4 AccentAlt()
    {
        const auto* Preset = EditorThemes::GetActivePreset();
        if (Preset == nullptr)
        {
            return ToImVec4(Palette().AccentAlt);
        }
        return Mix(Accent(), Preset->style.Colors[ImGuiCol_Text], 0.35f);
    }
    inline ImVec4 SectionHeader()
    {
        const auto* Preset = EditorThemes::GetActivePreset();
        return Preset ? Preset->style.Colors[ImGuiCol_Text] : ToImVec4(Palette().SectionHeader);
    }
    inline ImVec4 EntityIcon()
    {
        const auto* Preset = EditorThemes::GetActivePreset();
        if (Preset == nullptr)
        {
            return ToImVec4(Palette().EntityIcon);
        }
        const ImVec4 A = Accent();
        return ImVec4(A.x * 0.85f + 0.15f, A.y * 0.90f, A.z * 0.75f + 0.10f, A.w);
    }
    inline ImVec4 TextPrimary() { return PresetColor(ImGuiCol_Text, Palette().TextPrimary); }
    inline ImVec4 TextDim()
    {
        const auto* Preset = EditorThemes::GetActivePreset();
        if (Preset == nullptr)
        {
            return ToImVec4(Palette().TextDim);
        }
        return Mix(Preset->style.Colors[ImGuiCol_Text], Preset->style.Colors[ImGuiCol_TextDisabled], 0.5f);
    }
    inline ImVec4 TextMuted() { return PresetColor(ImGuiCol_TextDisabled, Palette().TextMuted); }
    inline ImVec4 WindowBg() { return PresetColor(ImGuiCol_WindowBg, Palette().WindowBg); }
    inline ImVec4 FrameBg() { return PresetColor(ImGuiCol_FrameBg, Palette().FrameBg); }
    inline ImVec4 TitleBg() { return PresetColor(ImGuiCol_TitleBg, Palette().TitleBg); }
    inline ImVec4 Button() { return PresetColor(ImGuiCol_Button, Palette().Button); }
    inline ImVec4 Header() { return PresetColor(ImGuiCol_Header, Palette().Header); }
    inline ImVec4 Border() { return PresetColor(ImGuiCol_Border, Palette().Border); }
    inline ImVec4 PanelBg() { return PresetColor(ImGuiCol_ChildBg, Palette().PanelBg); }
    inline ImVec4 RowBg() { return PresetColor(ImGuiCol_TableRowBgAlt, Palette().RowBg); }
    inline ImVec4 RowBgHovered() { return PresetColor(ImGuiCol_HeaderHovered, Palette().RowBgHovered); }
    inline ImVec4 RowBgActive() { return PresetColor(ImGuiCol_HeaderActive, Palette().RowBgActive); }
    inline ImVec4 ComponentHeader() { return PresetColor(ImGuiCol_Header, Palette().ComponentHeader); }

    // Derivations.
    inline float  Clamp01(float V)                    { return V < 0.0f ? 0.0f : (V > 1.0f ? 1.0f : V); }
    inline ImVec4 WithAlpha(const ImVec4& C, float A) { return ImVec4(C.x, C.y, C.z, A); }
    inline ImVec4 Lighten(const ImVec4& C, float D)   { return ImVec4(Clamp01(C.x + D), Clamp01(C.y + D), Clamp01(C.z + D), C.w); }
    inline ImU32  U32(const ImVec4& C)                { return ImGui::ColorConvertFloat4ToU32(C); }
}
