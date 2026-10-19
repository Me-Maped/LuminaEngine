#include "CategorizedPicker.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "Containers/Algorithm.h"
#include "Containers/StringFormat.h"
#include "Core/Math/Math.h"
#include "Localization/Localization.h"
#include "Search/TextSearch.h"
#include "Tools/UI/ImGui/ImGuiDesignIcons.h"
#include "UI/Tools/NodeGraph/GraphActionMenu.h"

namespace Lumina
{
    namespace
    {
        constexpr float kPickerWidth      = 380.0f;
        constexpr float kPickerListHeight = 380.0f;

        // Up to this many items, every category starts open, since collapsing a short list only hides it.
        constexpr int32 kOpenAllBelowCount = 24;

        const FString& PickerItemIdentity(const FPickerItem& Item)
        {
            return !Item.Key.empty() ? Item.Key : !Item.SourceName.empty() ? Item.SourceName : Item.Name;
        }

        FStringView NextCategoryPathSegment(FStringView Path, size_t& Cursor)
        {
            // Source and display paths may have different leading/repeated separators.
            // Skip only separators, leaving the caller's folder names unchanged.
            while (Cursor < Path.size() && Path[Cursor] == '/')
            {
                ++Cursor;
            }
            const size_t Begin = Cursor;
            while (Cursor < Path.size() && Path[Cursor] != '/')
            {
                ++Cursor;
            }
            return Path.substr(Begin, Cursor - Begin);
        }

        // The matched characters of a fuzzy search drawn in the accent color, coalesced into runs.
        void DrawPickerMatchedName(const FString& Name, const TVector<uint16>& Matches)
        {
            const ImVec4 MatchColor(1.0f, 0.82f, 0.35f, 1.0f);
            const char* Text = Name.c_str();
            for (const FGraphActionTextSpan& Span : BuildGraphActionHighlightSpans(Text, Matches))
            {
                if (Span.Begin != 0)
                {
                    ImGui::SameLine(0.0f, 0.0f);
                }

                if (Span.bMatched)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, MatchColor);
                }
                ImGui::TextUnformatted(Text + Span.Begin, Text + Span.End);
                if (Span.bMatched)
                {
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    const FPickerItem* FCategorizedPicker::DrawButton(const char* StrId, const char* Label, const FGatherItems& Gather, float Width)
    {
        const FPickerItem* Picked = nullptr;

        ImGui::PushID(StrId);
        if (ImGui::Button(Label, ImVec2(Width, 0.0f)))
        {
            ImGui::OpenPopup("##CategorizedPicker");
        }

        // Drops down from the button like a combo, and opens upward when the space below cannot hold it.
        const ImVec2 ButtonMin = ImGui::GetItemRectMin();
        const ImVec2 ButtonMax = ImGui::GetItemRectMax();
        const ImGuiViewport* Viewport = ImGui::GetWindowViewport();
        const float PopupHeight = kPickerListHeight + ImGui::GetFrameHeightWithSpacing() * 3.0f;
        const bool bOpenUpward = ButtonMax.y + PopupHeight > Viewport->WorkPos.y + Viewport->WorkSize.y;
        ImGui::SetNextWindowPos(bOpenUpward ? ButtonMin : ImVec2(ButtonMin.x, ButtonMax.y), ImGuiCond_Appearing, ImVec2(0.0f, bOpenUpward ? 1.0f : 0.0f));

        if (ImGui::BeginPopup("##CategorizedPicker"))
        {
            Picked = DrawContents(Gather);
            if (Picked != nullptr)
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();

        return Picked;
    }

    const FPickerItem* FCategorizedPicker::DrawContents(const FGatherItems& Gather)
    {
        const bool bAppearing = ImGui::IsWindowAppearing();
        const FLocalizationSnapshot Snapshot = FLocalization::Get().ReadForCurrentFrame();
        if (bAppearing || !ItemsSnapshot.HasSameTable(Snapshot))
        {
            const FString SelectedIdentity = !bAppearing && SelectedItem >= 0 && SelectedItem < static_cast<int32>(Items.size())
                ? PickerItemIdentity(Items[SelectedItem]) : FString();
            if (bAppearing)
            {
                Query[0] = '\0';
            }
            Rebuild(Gather, Snapshot);
            Rescore();
            if (!SelectedIdentity.empty())
            {
                for (int32 Index = 0; Index < static_cast<int32>(Items.size()); ++Index)
                {
                    if (PickerItemIdentity(Items[Index]) == SelectedIdentity && ItemScores[Index] >= 0)
                    {
                        SelectedItem = Index;
                        bScrollToSelected = true;
                        break;
                    }
                }
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::SetNextItemWidth(kPickerWidth);
        if (bAppearing)
        {
            ImGui::SetKeyboardFocusHere();
        }
        const FString Hint = Lumina::Format("{} {}", LE_ICON_MAGNIFY, SearchHint);
        const bool bEnter = ImGui::InputTextWithHint("##PickerSearch", Hint.c_str(), Query, sizeof(Query), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar();

        if (ScoredQuery != Query)
        {
            Rescore();
        }

        if (!DrawnOrder.empty())
        {
            int32 Position = 0;
            while (Position < (int32)DrawnOrder.size() && DrawnOrder[Position] != SelectedItem)
            {
                ++Position;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
            {
                SelectedItem = DrawnOrder[Position >= (int32)DrawnOrder.size() ? 0 : (Position + 1) % (int32)DrawnOrder.size()];
                bScrollToSelected = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
            {
                SelectedItem = DrawnOrder[Position >= (int32)DrawnOrder.size() ? 0 : (Position + (int32)DrawnOrder.size() - 1) % (int32)DrawnOrder.size()];
                bScrollToSelected = true;
            }
        }

        int32 Picked = -1;
        DrawingOrder.clear();

        ImGui::Spacing();
        if (ImGui::BeginChild("##PickerList", ImVec2(kPickerWidth, kPickerListHeight)))
        {
            // Searching expands categories under its own ids, so the user's browsing layout survives clearing the query.
            ImGui::PushID(IsSearching() ? 1 : 0);
            if (!Categories.empty())
            {
                for (int32 Child : Categories[0].Children)
                {
                    DrawCategory(Child, Picked);
                }
                for (int32 Item : Categories[0].Items)
                {
                    DrawItem(Item, Picked);
                }
            }
            ImGui::PopID();

            if (DrawingOrder.empty())
            {
                ImGui::Spacing();
                ImGui::TextDisabled(Items.empty() ? LOC("Nothing to pick from.") : LOC("No matches."));
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        const int32 Total = (int32)Items.size();
        const int32 Shown = IsSearching() ? CountItems(0, true) : Total;
        const FString Count = IsSearching()
            ? Lumina::Format("{} of {} {}", Shown, Total, ItemNoun)
            : Lumina::Format("{} {}", Total, ItemNoun);
        ImGui::TextDisabled("%s", Count.c_str());
        if (SelectedItem >= 0)
        {
            constexpr const char* EnterHint = "Enter to pick";
            ImGui::SameLine(kPickerWidth - ImGui::CalcTextSize(EnterHint).x);
            ImGui::TextDisabled("%s", EnterHint);
        }

        DrawnOrder = DrawingOrder;
        bScrollToSelected = false;

        if (bEnter && SelectedItem >= 0 && Algo::Contains(DrawnOrder, SelectedItem))
        {
            Picked = SelectedItem;
        }

        // Enter reaches a disabled item the same as the arrow keys do, so it is refused here rather than per input.
        return Picked >= 0 && Picked < (int32)Items.size() && !Items[Picked].bDisabled ? &Items[Picked] : nullptr;
    }

    void FCategorizedPicker::Rebuild(const FGatherItems& Gather, const FLocalizationSnapshot& Snapshot)
    {
        Items.clear();
        if (Gather)
        {
            Gather(Items);
        }
        ItemsSnapshot = Snapshot;

        Algo::StableSort(Items, [](const FPickerItem& A, const FPickerItem& B) { return A.Name < B.Name; });

        Categories.clear();
        Categories.emplace_back();
        for (int32 Index = 0; Index < (int32)Items.size(); ++Index)
        {
            int32 Category = 0;
            const FStringView Path(Items[Index].Category);
            const FStringView SourcePath(Items[Index].SourceCategory.empty() ? Items[Index].Category : Items[Index].SourceCategory);
            size_t Start = 0;
            size_t SourceStart = 0;
            for (FStringView Label = NextCategoryPathSegment(Path, Start); !Label.empty();
                Label = NextCategoryPathSegment(Path, Start))
            {
                const FStringView SourceLabel = NextCategoryPathSegment(SourcePath, SourceStart);
                Category = FindOrAddChild(Category, Label, SourceLabel.empty() ? Label : SourceLabel);
            }
            Categories[Category].Items.push_back(Index);
        }

        CompressChains(0);
        for (FCategory& Category : Categories)
        {
            Algo::StableSort(Category.Children, [this](int32 A, int32 B) { return Categories[A].Label < Categories[B].Label; });
        }
        Algo::StableSort(Categories[0].Children, [this](int32 A, int32 B)
        {
            return Categories[A].Label != TrailingCategory && Categories[B].Label == TrailingCategory;
        });
        CountItems(0, false);

        bOpenCategoriesByDefault = Items.size() <= (size_t)kOpenAllBelowCount || Categories[0].Children.size() == 1;
        SelectedItem = -1;
        DrawnOrder.clear();
    }

    int32 FCategorizedPicker::FindOrAddChild(int32 Parent, FStringView Label, FStringView SourceLabel)
    {
        const FString Identity = Categories[Parent].Identity + "/" + FString(SourceLabel);
        for (int32 Child : Categories[Parent].Children)
        {
            if (Categories[Child].Identity == Identity)
            {
                return Child;
            }
        }

        const int32 Added = (int32)Categories.size();
        FCategory& Category = Categories.emplace_back();
        Category.Label = FString(Label.data(), Label.size());
        Category.Identity = Identity;
        Categories[Parent].Children.push_back(Added);
        return Added;
    }

    void FCategorizedPicker::CompressChains(int32 CategoryIndex)
    {
        // A folder holding only another folder is one more click with nothing to choose, so the two share a row.
        if (CategoryIndex != 0)
        {
            while (Categories[CategoryIndex].Items.empty() && Categories[CategoryIndex].Children.size() == 1)
            {
                const int32 Only = Categories[CategoryIndex].Children[0];
                Categories[CategoryIndex].Label += " / " + Categories[Only].Label;
                Categories[CategoryIndex].Items = Categories[Only].Items;
                Categories[CategoryIndex].Children = Categories[Only].Children;
                Categories[Only].Children.clear();
                Categories[Only].Items.clear();
            }
        }

        const TVector<int32> Children = Categories[CategoryIndex].Children;
        for (int32 Child : Children)
        {
            CompressChains(Child);
        }
    }

    int32 FCategorizedPicker::CountItems(int32 CategoryIndex, bool bMatchesOnly)
    {
        int32 Count = 0;
        for (int32 Item : Categories[CategoryIndex].Items)
        {
            Count += !bMatchesOnly || ItemScores[Item] >= 0 ? 1 : 0;
        }
        const TVector<int32> Children = Categories[CategoryIndex].Children;
        for (int32 Child : Children)
        {
            Count += CountItems(Child, bMatchesOnly);
        }

        if (bMatchesOnly)
        {
            Categories[CategoryIndex].MatchCount = Count;
        }
        else
        {
            Categories[CategoryIndex].TotalCount = Count;
        }
        return Count;
    }

    bool FCategorizedPicker::IsSearching() const
    {
        const TextSearch::FQuery Parsed(Query);
        return !Parsed.IsValid() || Parsed.GetTermCount() != 0;
    }

    void FCategorizedPicker::Rescore()
    {
        ScoredQuery = Query;
        ItemScores.assign(Items.size(), 0);
        ItemMatches.assign(Items.size(), TVector<uint16>());

        if (!IsSearching())
        {
            return;
        }

        int32 BestScore = -1;
        SelectedItem = -1;
        const TextSearch::FQuery SearchQuery(Query);
        for (int32 Index = 0; Index < (int32)Items.size(); ++Index)
        {
            const FPickerItem& Item = Items[Index];
            int32 Score = 0;
            TVector<uint16> Matches;
            if (FuzzyMatchAliases(SearchQuery, {
                {Item.Name.c_str(), 1000, true},
                {Item.SourceName.c_str(), 1000},
                {Item.Key.c_str(), 500},
                {Item.Category.c_str()},
                {Item.SourceCategory.c_str()}}, Score, Matches))
            {
                // Negative scores are reserved for hidden rows; retain fuzzy ranking for visible hits.
                ItemScores[Index]  = Math::Max(Score, 0);
                ItemMatches[Index] = Move(Matches);
            }
            else
            {
                ItemScores[Index] = -1;
            }

            if (ItemScores[Index] > BestScore)
            {
                BestScore    = ItemScores[Index];
                SelectedItem = Index;
            }
        }

        CountItems(0, true);
        bScrollToSelected = true;
    }

    void FCategorizedPicker::DrawCategory(int32 CategoryIndex, int32& OutPicked)
    {
        const FCategory& Category = Categories[CategoryIndex];
        if (IsSearching() && Category.MatchCount == 0)
        {
            return;
        }

        if (IsSearching())
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }
        else
        {
            ImGui::SetNextItemOpen(bOpenCategoriesByDefault, ImGuiCond_Once);
        }

        ImGui::PushID(Category.Identity.c_str());
        const float RowRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        const bool bOpen = ImGui::TreeNodeEx("##Category", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding);

        // Drawn over the node so the folder icon follows the open state and the count sits at the right edge.
        ImGui::SameLine();
        ImGui::TextUnformatted(bOpen ? LE_ICON_FOLDER_OPEN : LE_ICON_FOLDER);
        ImGui::SameLine();
        ImGui::TextUnformatted(Category.Label.c_str());

        const FString CountText = Lumina::Format("{}", IsSearching() ? Category.MatchCount : Category.TotalCount);
        ImGui::SameLine(RowRight - ImGui::CalcTextSize(CountText.c_str()).x - ImGui::GetStyle().FramePadding.x);
        ImGui::TextDisabled("%s", CountText.c_str());

        if (bOpen)
        {
            for (int32 Child : Category.Children)
            {
                DrawCategory(Child, OutPicked);
            }
            for (int32 Item : Category.Items)
            {
                DrawItem(Item, OutPicked);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void FCategorizedPicker::DrawItem(int32 ItemIndex, int32& OutPicked)
    {
        if (IsSearching() && ItemScores[ItemIndex] < 0)
        {
            return;
        }
        DrawingOrder.push_back(ItemIndex);

        const FPickerItem& Item = Items[ItemIndex];
        const bool bSelected = ItemIndex == SelectedItem;

        // Display translations and locale-dependent sorting must not change row identity.
        ImGui::PushID(PickerItemIdentity(Item).c_str());
        ImGui::BeginDisabled(Item.bDisabled);
        const ImVec2 RowStart = ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##Item", bSelected, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0.0f, ImGui::GetTextLineHeight() + 4.0f)))
        {
            OutPicked = ItemIndex;
        }
        const bool bHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
        if (bSelected && bScrollToSelected)
        {
            ImGui::SetScrollHereY(0.5f);
        }

        // Name and icon sit over the row, indented like a tree leaf so they line up under their folder's label.
        ImGui::SetCursorScreenPos(ImVec2(RowStart.x + ImGui::GetTreeNodeToLabelSpacing() * 0.5f, RowStart.y + 2.0f));
        if (Item.Icon != nullptr)
        {
            ImGui::TextUnformatted(Item.Icon);
            ImGui::SameLine();
        }
        DrawPickerMatchedName(Item.Name, ItemMatches[ItemIndex]);
        ImGui::EndDisabled();

        if (bHovered && !Item.Tooltip.empty())
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(Item.Name.c_str());
            ImGui::Separator();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
            ImGui::TextDisabled("%s", Item.Tooltip.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
}
