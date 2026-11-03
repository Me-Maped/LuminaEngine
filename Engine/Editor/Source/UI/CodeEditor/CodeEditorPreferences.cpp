#include "CodeEditorPreferences.h"

#include <imgui.h>
#include "CodeEditorThemes.h"
#include "Config/Config.h"
#include "Core/Reflection/Type/LuminaTypes.h"
#include "Localization/Localization.h"
#include "Settings/EditorSettings.h"
#include "Tools/UI/ImGui/EditorColors.h"
#include "Tools/UI/ImGui/ImGuiDesignIcons.h"
#include "UI/Tools/EditorToolContext.h"

namespace Lumina::CodeEditorPreferences
{
    void DrawQuickSettings(IEditorToolContext* Tools)
    {
        CCodeEditorSettings* Settings = GetMutableDefault<CCodeEditorSettings>();
        bool bSave = false;

        if (ImGui::BeginCombo(LOC_ID("Theme"), Settings->Theme.Name.c_str()))
        {
            for (const FString& Theme : CodeEditorThemes::List())
            {
                if (ImGui::Selectable(Theme.c_str(), Theme == Settings->Theme.Name))
                {
                    Settings->Theme.Name = Theme;
                    bSave = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SliderFloat("Font scale", &Settings->FontScale, 0.75f, 3.0f, "%.2fx");
        bSave |= ImGui::IsItemDeactivatedAfterEdit();
        bSave |= ImGui::Checkbox(LOC_ID("Suggest while typing"), &Settings->bSuggestWhileTyping);
        bSave |= ImGui::Checkbox(LOC_ID("Line numbers"), &Settings->bShowLineNumbers);
        bSave |= ImGui::Checkbox(LOC_ID("Minimap"), &Settings->bShowMiniMap);
        bSave |= ImGui::Checkbox(LOC_ID("Indent guides"), &Settings->bShowIndentGuides);
        bSave |= ImGui::Checkbox(LOC_ID("Whitespace"), &Settings->bShowWhitespace);
        bSave |= ImGui::Checkbox(LOC_ID("Highlight occurrences"), &Settings->bHighlightOccurrences);

        if (Tools != nullptr)
        {
            ImGui::Separator();
            if (ImGui::MenuItem(LOC_ICON_ID(LE_ICON_COG, "All Code Editor Settings")))
            {
                Tools->OpenSettings(CCodeEditorSettings::StaticClass());
            }
            if (ImGui::MenuItem(LOC_ICON_ID(LE_ICON_KEYBOARD, "Keyboard Shortcuts")))
            {
                Tools->OpenSettings(CCodeEditorShortcuts::StaticClass());
            }
        }

        if (bSave)
        {
            GConfig->SaveSettings(CCodeEditorSettings::StaticClass());
        }
    }

    void DrawShortcutReference()
    {
        if (!ImGui::BeginTable("##CodeShortcuts", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            return;
        }

        const CCodeEditorShortcuts* Keys = GetDefault<CCodeEditorShortcuts>();
        FString Category;
        for (FProperty* Property : CCodeEditorShortcuts::StaticClass()->GetOwnProperties())
        {
            const FCStringView PropertyCategory = Property->GetMetadata("Category");
            if (Category != PropertyCategory.c_str())
            {
                Category = PropertyCategory.c_str();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(EditorColors::SectionHeader(), "%s", Category.c_str());
            }

            const SShortcut* Shortcut = Property->GetValuePtr<SShortcut>(Keys);
            FString Bound = Shortcut->Primary.IsValid() ? Shortcut->Primary.GetDisplayName() : FString();
            if (Shortcut->Alternate.IsValid())
            {
                Bound += (Bound.empty() ? FString() : FString(", ")) + Shortcut->Alternate.GetDisplayName();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(Property->GetPropertyDisplayName().c_str());
            ImGui::TableNextColumn();
            if (Bound.empty())
            {
                ImGui::TextColored(EditorColors::TextMuted(), LOC("Unbound"));
            }
            else
            {
                ImGui::TextColored(EditorColors::Accent(), "%s", Bound.c_str());
            }
        }
        ImGui::EndTable();
    }
}
