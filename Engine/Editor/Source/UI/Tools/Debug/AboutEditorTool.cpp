#include "AboutEditorTool.h"

#include <Lumina.h>

#include "Localization/Localization.h"
#include "Platform/Process/PlatformProcess.h"

namespace Lumina
{
    void FAboutEditorTool::OnInitialize()
    {
        CreateToolWindow("About Lumina", [this](bool bIsFocused)
        {
            DrawWindow(bIsFocused);
        });
    }

    void FAboutEditorTool::OnDeinitialize(const FUpdateContext& UpdateContext)
    {
    }

    void FAboutEditorTool::DrawHelpMenu()
    {
        DrawHelpTextRow(LOC("Engine Version"),
            LOC("Shown on the About tab, combine with the build hash when filing issues."));
        DrawHelpTextRow(LOC("Documentation"),
            LOC("Long-form docs live on the GitHub wiki linked from the About tab."));
        DrawHelpTextRow(LOC("Help in Other Tools"),
            LOC("Every editor tool has its own Help menu (this menu) with a quick reference for that tool's "
                "controls, plus a Keybinds submenu listing every registered keyboard shortcut."));
    }

    void FAboutEditorTool::DrawWindow(bool bIsFocused)
    {
        if (ImGui::BeginTabBar("##AboutTabs"))
        {
            if (ImGui::BeginTabItem(LOC("About")))
            {
                DrawAboutTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(LOC("Contributors")))
            {
                DrawContributorsTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void FAboutEditorTool::DrawAboutTab()
    {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "%s", LOC("LUMINA ENGINE"));
        ImGui::PopFont();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::TextUnformatted(LOC("A modern, high-performance game engine built with Vulkan."));
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", LOC("Engine Information"));
        ImGui::Spacing();

        if (ImGui::BeginTable("##EngineInfoTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX))
        {
            ImGui::TableSetupColumn("##Property", ImGuiTableColumnFlags_WidthFixed, 180);
            ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);

            auto Label = [](const char* Text)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                ImGui::Text("%s", Text);
                ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(1);
            };

            Label(LOC("Version"));
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "%s", LUMINA_VERSION);

            Label(LOC("Rendering API"));
            ImGui::Text("Vulkan 1.3");

            Label(LOC("Build Configuration"));
#ifdef LUMINA_DEBUG
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", LOC("Debug"));
#elif defined(LUMINA_RELEASE)
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "%s", LOC("Release"));
#else
            ImGui::Text("%s", LOC("Development"));
#endif

            Label(LOC("Platform"));
#ifdef _WIN32
            ImGui::Text("Windows x64");
#elif defined(__linux__)
            ImGui::Text("Linux x64");
#elif defined(__APPLE__)
            ImGui::Text("macOS");
#endif

            Label(LOC("Compiler"));
#ifdef _MSC_VER
            ImGui::Text("MSVC %d", _MSC_VER);
#elif defined(__clang__)
            ImGui::Text("Clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
            ImGui::Text("GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", LOC("Core Features"));
        ImGui::Spacing();

        ImGui::BulletText("%s", LOC("Physically Based Rendering (PBR)"));
        ImGui::BulletText("%s", LOC("Advanced Material System"));
        ImGui::BulletText("%s", LOC("Multi-threaded Asset Pipeline"));
        ImGui::BulletText("%s", LOC("Hot-reload Shader Compilation"));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextUnformatted(LOC("Licensed under the Apache 2.0 License"));
        ImGui::TextUnformatted(LOC("Copyright (c) 2025 Lumina Engine Contributors"));
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float buttonWidth = 150.0f;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((availWidth - buttonWidth * 2 - ImGui::GetStyle().ItemSpacing.x) * 0.5f);

        if (ImGui::Button(LOC("Documentation"), ImVec2(buttonWidth, 0)))
        {
            Platform::LaunchURL(TEXT("https://luminagameengine.com/"));
        }

        ImGui::SameLine();

        if (ImGui::Button(LOC("GitHub"), ImVec2(buttonWidth, 0)))
        {
            Platform::LaunchURL(TEXT("https://github.com/MrDrElliot/LuminaEngine"));
        }
    }

    void FAboutEditorTool::DrawContributorsTab()
    {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s", LOC("Project Contributors"));
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", LOC("The talented people behind this project"));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const ImGuiTableFlags Flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##ContributorsTable", 2, Flags))
        {
            ImGui::TableSetupColumn(LOC_ID("Name"), ImGuiTableColumnFlags_WidthFixed, 200);
            ImGui::TableSetupColumn(LOC_ID("Role"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            auto AddContributor = [](const char* Name, const char* Role, ImVec4 NameColor = ImVec4(0.3f, 0.8f, 1.0f, 1.0f))
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Spacing();
                ImGui::TextColored(NameColor, "%s", Name);

                ImGui::TableSetColumnIndex(1);
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
                ImGui::TextWrapped("%s", Role);
                ImGui::PopStyleColor();
            };

            AddContributor("Dr. Elliot", LOC("Lead Developer & Engine Architect"), ImVec4(1.0f, 0.7f, 0.2f, 1.0f));

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", LOC("Thank you to everyone who contributed!"));
    }
}
