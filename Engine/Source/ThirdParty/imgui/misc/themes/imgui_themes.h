#pragma once

#include "imgui.h"
#include <string>

// Application-side styling. No dependency on imgui_internal.h or a renderer.
namespace ImGuiThemes
{
enum class Id { Default, Cyberpunk, Minimal, Wireframe, Brutalist, Bento, Count };
constexpr int ThemeCount = static_cast<int>(Id::Count);
enum class ButtonRole { Primary, Secondary, Danger };

struct Theme
{
    Id id = Id::Bento;
    ImGuiStyle style; // Unscaled appearance; only the documented visual fields are applied.
    ImVec4 accent = ImVec4(1, 1, 1, 1);
    ImVec4 accent_text = ImVec4(0, 0, 0, 1);
    ImVec4 danger = ImVec4(1, 0, 0, 1);
    ImVec4 danger_text = ImVec4(1, 1, 1, 1);
    ImVec4 shadow = ImVec4(0, 0, 0, 1);
    float shadow_offset = 0.0f;
    bool outline_buttons = false;
};

const char* Name(Id id);
const char* Slug(Id id);
Theme Make(Id id);
// Call before NewFrame(), with an unscaled Theme. Invalid input leaves the style intact.
bool Apply(const Theme& theme, float scale = 1.0f);
// Load is transactional; unknown keys and invalid values are errors.
bool Load(const char* path, Theme& theme, std::string& error);
bool Save(const char* path, const Theme& theme, std::string& error);
// Standard ImGui button behavior with scoped semantic colors and optional hard shadow.
bool Button(const Theme& theme, ButtonRole role, const char* label, ImVec2 size = ImVec2(0, 0), float scale = 1.0f);
}
