#include "imgui_themes.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>

using namespace ImGuiThemes;
static void Require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
static bool Near(float a, float b) { return std::fabs(a - b) < 0.00001f; }
static void Write(const char* path, const char* text) { std::ofstream(path) << text; }

static void CanonicalDefaultIsReadOnly()
{
    const ImGuiStyle native;
    Theme baseline=Make(Id::Default);
    Require(Near(baseline.style.FramePadding.x,native.FramePadding.x)
        && Near(baseline.style.ItemSpacing.y,native.ItemSpacing.y)
        && Near(baseline.style.ChildRounding,native.ChildRounding),"00 must start with native metrics");
    baseline.style.FramePadding=ImVec2(32,32);
    baseline.style.Colors[ImGuiCol_Button]=ImVec4(1,0,0,1);
    baseline.shadow_offset=7;
    Require(Apply(baseline),"native baseline must apply");
    const ImGuiStyle& active=ImGui::GetStyle();
    Require(Near(active.FramePadding.x,native.FramePadding.x),"baseline application must ignore edits");
    for (int i=0;i<ImGuiCol_COUNT;++i)
    {
        const ImVec4 a=active.Colors[i],b=native.Colors[i];
        Require(Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z)&&Near(a.w,b.w),"baseline must restore native colors");
    }
    const char* path="native-readonly-test.ini";
    Write(path,"[theme]\npreset=default\n");
    std::string error;
    Require(!Save(path,baseline,error) && !error.empty(),"baseline cannot be saved");
    Theme target=Make(Id::Bento);
    Require(!Load(path,target,error) && target.id==Id::Bento,"config cannot replace canonical baseline");
    std::remove(path);
}

// Catches cumulative scaling and accidentally overwriting non-visual runtime settings.
static void ApplicationIsRepeatable()
{
    Theme theme = Make(Id::Cyberpunk);
    theme.style.FramePadding = ImVec2(7, 5);
    ImGuiStyle& active = ImGui::GetStyle();
    active.HoverDelayNormal = 0.83f;
    active.FontScaleMain = 1.3f;
    active.FontScaleDpi = 1.7f;
    active.Alpha = 0.8f;
    Require(Apply(theme, 2.0f), "valid theme must apply");
    Require(Near(active.FramePadding.x, 14), "DPI must scale baseline padding");
    Require(Apply(theme, 2.0f) && Near(active.FramePadding.x, 14), "reapply must not compound DPI");
    Require(Apply(theme, 1.0f) && Near(active.FramePadding.x, 7), "DPI reset must restore baseline");
    Require(Near(active.HoverDelayNormal, 0.83f) && Near(active.FontScaleMain, 1.3f)
        && Near(active.FontScaleDpi, 1.7f) && Near(active.Alpha, 0.8f), "theme must preserve runtime settings");
    Require(!Apply(theme, std::numeric_limits<float>::quiet_NaN()), "NaN DPI must be rejected");
    Require(Near(active.FramePadding.x, 7), "invalid DPI must not mutate style");
}

static void DownscalingKeepsEnabledOutlines()
{
    Theme theme=Make(Id::Wireframe);
    theme.style.FrameBorderSize=1;
    theme.style.ChildBorderSize=1;
    theme.style.PopupBorderSize=1;
    theme.style.TabBorderSize=0;
    Require(Apply(theme,0.75f),"fractional DPI must apply");
    const ImGuiStyle& s=ImGui::GetStyle();
    Require(s.FrameBorderSize>=1 && s.ChildBorderSize>=1 && s.PopupBorderSize>=1,"downscaling must preserve enabled outlines");
    Require(s.TabBorderSize==0,"downscaling must not enable disabled outlines");
}

// Catches partial writes into the active theme and silently accepted corrupt settings.
static void BadConfigDoesNotReplaceTheme()
{
    const char* path = "theme-invalid-test.ini";
    const char* cases[] = {
        "[theme]\npreset=cyberpunk\n[style]\nFrameRounding=6\nFramePadding=bad\n",
        "[theme]\npreset=minimal\n[colors]\nText=nan,1,1,1\n",
        "[theme]\npreset=minimal\n[style]\nFramePadding=-1,8\n",
        "[theme]\npreset=minimal\n[colors]\nText=2,1,1,1\n",
        "[theme]\npreset=unknown\n",
        "[theme]\npreset=minimal\n[style]\nFrameRouding=4\n",
        "[theme]\npreset=minimal\n[extras]\noutline_buttons=perhaps\n",
        "[theme]\npreset=minimal\n[style]\nFrameRounding=4px\n",
        "[style]\nFrameRounding=4\n"
    };
    for (const char* text : cases)
    {
        Theme theme = Make(Id::Bento);
        theme.style.FrameRounding = 11;
        Write(path, text);
        std::string error;
        Require(!Load(path, theme, error), "bad config must fail");
        Require(!error.empty(), "bad config needs a useful error");
        Require(theme.id == Id::Bento && Near(theme.style.FrameRounding, 11), "failed load must be transactional");
    }
    std::remove(path);
}

// Catches lost user edits in config persistence, including semantic button settings.
static void EditedThemesRoundTrip()
{
    const char* path = "theme-roundtrip-test.ini";
    for (int i = 0; i < static_cast<int>(Id::Count); ++i)
    {
        if (static_cast<Id>(i)==Id::Default) continue;
        Theme before = Make(static_cast<Id>(i));
        before.style.FramePadding = ImVec2(11, 7);
        before.style.Colors[ImGuiCol_Button] = ImVec4(0.13f, 0.27f, 0.61f, 0.92f);
        before.accent = ImVec4(0.11f, 0.22f, 0.33f, 1);
        before.shadow_offset = 3;
        before.outline_buttons = true;
        std::string error;
        Require(Save(path, before, error), "save edited theme");
        Theme after = Make(Id::Minimal);
        Require(Load(path, after, error), "load edited theme");
        Require(after.id == before.id, "preset identity survives save/load");
        Require(Near(after.style.FramePadding.x, 11) && Near(after.style.FramePadding.y, 7), "padding edits survive save/load");
        Require(Near(after.accent.z, 0.33f) && after.outline_buttons && Near(after.shadow_offset, 3), "semantic edits survive save/load");
        for (int c = 0; c < ImGuiCol_COUNT; ++c)
        {
            const ImVec4& a = before.style.Colors[c];
            const ImVec4& b = after.style.Colors[c];
            Require(Near(a.x,b.x) && Near(a.y,b.y) && Near(a.z,b.z) && Near(a.w,b.w), "every color survives save/load");
        }
        before.style.FrameRounding = -1;
        Require(!Save(path, before, error), "invalid edit must not overwrite saved file");
        Require(Load(path, after, error), "original file survives rejected save");
    }
    std::remove(path);
}

int main()
{
    ImGui::CreateContext();
    try
    {
        CanonicalDefaultIsReadOnly();
        ApplicationIsRepeatable();
        DownscalingKeepsEnabledOutlines();
        BadConfigDoesNotReplaceTheme();
        EditedThemesRoundTrip();
        std::puts("PASS: appearance/DPI isolation, transactional validation, edited theme round trips");
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        ImGui::DestroyContext();
        return 1;
    }
    ImGui::DestroyContext();
    return 0;
}
