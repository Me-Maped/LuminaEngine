#include <gtest/gtest.h>
#include <limits>
#include "Tools/UI/ImGui/EditorTheme.h"

namespace
{
    class EditorThemeTest : public ::testing::Test
    {
    protected:
        ImGuiContext* Previous = nullptr;
        void SetUp() override
        {
            Previous = ImGui::GetCurrentContext();
            ImGui::CreateContext();
        }
        void TearDown() override
        {
            ImGui::DestroyContext();
            ImGui::SetCurrentContext(Previous);
        }
    };

    void ExpectColor(const ImVec4& Actual, const ImVec4& Expected)
    {
        EXPECT_FLOAT_EQ(Actual.x, Expected.x);
        EXPECT_FLOAT_EQ(Actual.y, Expected.y);
        EXPECT_FLOAT_EQ(Actual.z, Expected.z);
        EXPECT_FLOAT_EQ(Actual.w, Expected.w);
    }
}

TEST_F(EditorThemeTest, AutomaticAndExplicitScaleStayInSupportedRange)
{
    const ImGuiStyle Original;
    const auto Preset = ImGuiThemes::Make(ImGuiThemes::Id::Bento);
    for (float Input : {0.1f, 0.75f, 4.25f, std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()})
    {
        const float Scale = Lumina::EditorThemes::ClampScale(Input);
        ASSERT_TRUE(Lumina::EditorThemes::ApplyStyle(Original, &Preset, Scale, true));
    }
    EXPECT_FLOAT_EQ(Lumina::EditorThemes::ClampScale(4.25f), 4);
    EXPECT_FLOAT_EQ(Lumina::EditorThemes::ClampScale(0.1f), 0.5f);
    EXPECT_FLOAT_EQ(Lumina::EditorThemes::ClampScale(0.75f), 0.75f);
}

TEST_F(EditorThemeTest, PresetSurvivesRepeatedScaleChanges)
{
    const ImGuiStyle Original;
    const auto Preset = ImGuiThemes::Make(ImGuiThemes::Id::Minimal);
    for (float Scale : {1.0f, 2.0f, 2.0f, 0.75f, 1.0f})
    {
        ASSERT_TRUE(Lumina::EditorThemes::ApplyStyle(Original, &Preset, Scale, true));
        const auto& Style = ImGui::GetStyle();
        ExpectColor(Style.Colors[ImGuiCol_WindowBg], Preset.style.Colors[ImGuiCol_WindowBg]);
        EXPECT_FLOAT_EQ(Style.FramePadding.x, float(int(Preset.style.FramePadding.x * Scale)));
        EXPECT_FLOAT_EQ(Style.FontScaleMain, Scale);
    }
}

TEST_F(EditorThemeTest, SwitchingBackRestoresCustomLuminaPaletteAndMetrics)
{
    ImGuiStyle Original;
    Original.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.23f, 0.34f, 0.8f);
    Original.FramePadding = ImVec2(8, 4);
    Original.HoverDelayNormal = 0.83f;
    Original.FontSizeBase = 22;
    for (int Index = 0; Index < ImGuiThemes::ThemeCount; ++Index)
    {
        const auto Preset = ImGuiThemes::Make(static_cast<ImGuiThemes::Id>(Index));
        ASSERT_TRUE(Lumina::EditorThemes::ApplyStyle(Original, &Preset, 1.5f, true));
        EXPECT_FLOAT_EQ(ImGui::GetStyle().HoverDelayNormal, Original.HoverDelayNormal);
        EXPECT_FLOAT_EQ(ImGui::GetStyle().FontSizeBase, Original.FontSizeBase);
        ASSERT_TRUE(Lumina::EditorThemes::ApplyStyle(Original, nullptr, 1.5f, true));
        ExpectColor(ImGui::GetStyle().Colors[ImGuiCol_Button], Original.Colors[ImGuiCol_Button]);
        EXPECT_FLOAT_EQ(ImGui::GetStyle().FramePadding.x, 12);
    }
}

TEST_F(EditorThemeTest, ViewportAndDockingColorsFollowPreset)
{
    const ImGuiStyle Original;
    const auto Preset = ImGuiThemes::Make(ImGuiThemes::Id::Bento);
    ASSERT_TRUE(Lumina::EditorThemes::ApplyStyle(Original, &Preset, 1, true));
    const auto& Style = ImGui::GetStyle();
    EXPECT_FLOAT_EQ(Style.WindowRounding, 0);
    EXPECT_FLOAT_EQ(Style.Colors[ImGuiCol_WindowBg].w, 1);
    EXPECT_FLOAT_EQ(Style.Colors[ImGuiCol_DockingPreview].x, Preset.accent.x);
    ExpectColor(Style.Colors[ImGuiCol_DockingEmptyBg], Preset.style.Colors[ImGuiCol_WindowBg]);
    ASSERT_TRUE(Lumina::EditorThemes::ApplyStyle(Original, &Preset, 1, false));
    EXPECT_FLOAT_EQ(ImGui::GetStyle().WindowRounding, Preset.style.WindowRounding);
}

TEST_F(EditorThemeTest, FractionalScalePreservesThinPresetBorders)
{
    const ImGuiStyle Original;
    const auto Preset = ImGuiThemes::Make(ImGuiThemes::Id::Wireframe);
    ASSERT_TRUE(Lumina::EditorThemes::ApplyStyle(Original, &Preset, 0.5f, true));
    EXPECT_GE(ImGui::GetStyle().FrameBorderSize, 1);
    EXPECT_GE(ImGui::GetStyle().ChildBorderSize, 1);
    EXPECT_FLOAT_EQ(ImGui::GetStyle().TabBorderSize, 0);
}
