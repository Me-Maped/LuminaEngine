#include "GraphActionMenu.h"

#include "EdGraphNode.h"
#include "EdNodeGraph.h"
#include "Localization/Localization.h"
#include "Search/TextSearch.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Core/Object/Cast.h"
#include "Core/Object/Class.h"
#include "imgui-node-editor/imgui_node_editor.h"

namespace Lumina
{
    namespace
    {
        bool GraphQueryIsActive(const char* Text)
        {
            const TextSearch::FQuery Query(Text != nullptr ? Text : "");
            return !Query.IsValid() || Query.GetTermCount() != 0;
        }

        void DrawHighlightedText(const char* Text, const TVector<uint16>& MatchIndices, const ImVec4& MatchColor)
        {
            if (Text == nullptr)
            {
                return;
            }
            if (Text[0] == '\0' || MatchIndices.empty())
            {
                ImGui::TextUnformatted(Text);
                return;
            }

            const FGraphActionTextSpans Spans = BuildGraphActionHighlightSpans(Text, MatchIndices);
            for (const FGraphActionTextSpan& Span : Spans)
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

    void FGraphActionMenu::Reset()
    {
        QueryBuffer[0] = '\0';
        CachedQueryForScore.clear();
        Actions.clear();
        VisibleIndices.clear();
        SelectedIndex = 0;
        LastSelectedIndex = -1;
        bActionsDirty = true;
        bFocusInput = true;
        bPendingScrollToSelected = true;
    }

    void FGraphActionMenu::ClampSelection()
    {
        if (VisibleIndices.empty())
        {
            SelectedIndex = 0;
            return;
        }

        if (SelectedIndex < 0)
        {
            SelectedIndex = 0;
        }
        else if (SelectedIndex >= static_cast<int32>(VisibleIndices.size()))
        {
            SelectedIndex = static_cast<int32>(VisibleIndices.size()) - 1;
        }
    }

    void FGraphActionMenu::RebuildActions(CEdNodeGraph* Graph, const FLocalizationSnapshot& Snapshot)
    {
        Actions.clear();
        VisibleIndices.clear();

        if (Graph == nullptr)
        {
            return;
        }

        const THashSet<CClass*>& SupportedNodes = Graph->GetSupportedNodes();
        Actions.reserve(SupportedNodes.size());

        for (CClass* NodeClass : SupportedNodes)
        {
            CEdGraphNode* CDO = Cast<CEdGraphNode>(NodeClass->GetDefaultObject());
            if (CDO == nullptr)
            {
                continue;
            }

            FGraphAction Action;
            Action.NodeClass   = NodeClass;
            Action.SourceName = CDO->GetNodeDisplayName();
            Action.SourceCategory = CDO->GetNodeCategory();
            Action.Key = NodeClass->GetName().ToString();
            // Own both languages and translate all retained labels from one immutable table.
            Action.DisplayName = FString(Snapshot.TranslateView(Action.SourceName, FStringView("node")));
            Action.Category = FString(Snapshot.TranslateView(Action.SourceCategory));
            Action.Tooltip = FString(Snapshot.TranslateView(CDO->GetNodeTooltip()));
            Actions.push_back(Move(Action));
        }

        ActionsSnapshot = Snapshot;
        bActionsDirty = false;
    }

    void FGraphActionMenu::Rescore()
    {
        VisibleIndices.clear();
        VisibleIndices.reserve(Actions.size());

        const TextSearch::FQuery Query(QueryBuffer);
        const bool bHasQuery = GraphQueryIsActive(QueryBuffer);

        for (int32 i = 0; i < static_cast<int32>(Actions.size()); ++i)
        {
            FGraphAction& Action = Actions[i];

            if (!bHasQuery)
            {
                Action.Score = 0;
                Action.MatchIndices.clear();
                VisibleIndices.push_back(i);
                continue;
            }

            if (!FuzzyMatchAliases(Query, {
                {Action.DisplayName.c_str(), 100, true},
                {Action.SourceName.c_str(), 100},
                {Action.Key.c_str(), 50},
                {Action.Category.c_str()},
                {Action.SourceCategory.c_str()}}, Action.Score, Action.MatchIndices))
            {
                continue;
            }

            VisibleIndices.push_back(i);
        }

        if (bHasQuery)
        {
            Algo::Sort(VisibleIndices, [this](int32 A, int32 B)
            {
                const FGraphAction& LHS = Actions[A];
                const FGraphAction& RHS = Actions[B];
                if (LHS.Score != RHS.Score)
                {
                    return LHS.Score > RHS.Score;
                }
                return LHS.DisplayName < RHS.DisplayName;
            });
        }
        else
        {
            Algo::Sort(VisibleIndices, [this](int32 A, int32 B)
            {
                const FGraphAction& LHS = Actions[A];
                const FGraphAction& RHS = Actions[B];
                if (LHS.Category != RHS.Category)
                {
                    return LHS.Category < RHS.Category;
                }
                return LHS.DisplayName < RHS.DisplayName;
            });
        }

        CachedQueryForScore = QueryBuffer;
        ClampSelection();
    }

    bool FGraphActionMenu::Draw(CEdNodeGraph* Graph)
    {
        if (Graph == nullptr)
        {
            return false;
        }

        const FLocalizationSnapshot Snapshot = FLocalization::Get().ReadForCurrentFrame();
        if (bActionsDirty || !ActionsSnapshot.HasSameTable(Snapshot))
        {
            CClass* SelectedClass = SelectedIndex >= 0 && SelectedIndex < static_cast<int32>(VisibleIndices.size())
                ? Actions[VisibleIndices[SelectedIndex]].NodeClass : nullptr;
            RebuildActions(Graph, Snapshot);
            Rescore();
            bool bRestored = false;
            if (SelectedClass != nullptr)
            {
                for (int32 Index = 0; Index < static_cast<int32>(VisibleIndices.size()); ++Index)
                {
                    if (Actions[VisibleIndices[Index]].NodeClass == SelectedClass)
                    {
                        SelectedIndex = Index;
                        bRestored = true;
                        break;
                    }
                }
            }
            if (!bRestored)
            {
                SelectedIndex = 0;
            }
            bPendingScrollToSelected = bRestored;
        }

        constexpr ImVec2 PopupSize(360, 460);
        constexpr ImVec2 SearchBarPadding(10, 8);
        constexpr float  ItemHeight         = 26.0f;
        constexpr float  HeaderAreaHeight   = 48.0f;
        constexpr float  FooterAreaHeight   = 26.0f;

        const ImVec4 AccentColor(0.92f, 0.55f, 0.23f, 1.0f);
        const ImVec4 MatchColor (1.0f, 0.85f, 0.35f, 1.0f);
        const ImVec4 CategoryColor(0.55f, 0.55f, 0.58f, 1.0f);
        const ImVec4 SelectedBg (AccentColor.x * 0.35f, AccentColor.y * 0.35f, AccentColor.z * 0.35f, 0.75f);

        ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SearchBarPadding);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.16f, 0.16f, 0.18f, 1.0f));

        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::PushItemWidth(PopupSize.x - 20);

        if (bFocusInput)
        {
            ImGui::SetKeyboardFocusHere();
            bFocusInput = false;
        }

        const bool bEnterCommitted = ImGui::InputTextWithHint(
            "##GraphActionSearch",
            LOC("Search nodes..."),
            QueryBuffer,
            IM_ARRAYSIZE(QueryBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        const bool bQueryChanged = CachedQueryForScore != FString(QueryBuffer);

        ImGui::PopItemWidth();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        if (bQueryChanged)
        {
            Rescore();
            SelectedIndex = 0;
            bPendingScrollToSelected = true;
        }

        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.25f, 0.25f, 0.27f, 1.0f));
        ImGui::SetCursorPosY(HeaderAreaHeight);
        ImGui::Separator();
        ImGui::PopStyleColor();

        const bool bArrowDown = ImGui::IsKeyPressed(ImGuiKey_DownArrow, true);
        const bool bArrowUp   = ImGui::IsKeyPressed(ImGuiKey_UpArrow,   true);

        if (bArrowDown && !VisibleIndices.empty())
        {
            SelectedIndex = (SelectedIndex + 1) % static_cast<int32>(VisibleIndices.size());
            bPendingScrollToSelected = true;
        }
        if (bArrowUp && !VisibleIndices.empty())
        {
            SelectedIndex = (SelectedIndex - 1 + static_cast<int32>(VisibleIndices.size())) % static_cast<int32>(VisibleIndices.size());
            bPendingScrollToSelected = true;
        }

        constexpr ImVec2 ListSize(PopupSize.x, PopupSize.y - HeaderAreaHeight - FooterAreaHeight - 1.0f);

        bool bCommitted = false;
        int32 CommittedActionIndex = -1;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 1));

        if (ImGui::BeginChild("##GraphActionList", ListSize, false))
        {
            FString CurrentCategory;
            const bool bGroupByCategory = !GraphQueryIsActive(QueryBuffer);

            for (int32 Position = 0; Position < static_cast<int32>(VisibleIndices.size()); ++Position)
            {
                const int32 ActionIdx = VisibleIndices[Position];
                const FGraphAction& Action = Actions[ActionIdx];

                if (bGroupByCategory && Action.Category != CurrentCategory)
                {
                    CurrentCategory = Action.Category;

                    if (Position > 0)
                    {
                        ImGui::Dummy(ImVec2(0, 4));
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, CategoryColor);
                    ImGui::SetCursorPosX(8);
                    ImGui::TextUnformatted(CurrentCategory.c_str());
                    ImGui::PopStyleColor();

                    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
                    ImGui::Separator();
                    ImGui::PopStyleColor();
                }

                const bool bIsSelected = (Position == SelectedIndex);

                ImGui::PushID(Action.NodeClass);

                if (bIsSelected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header,        SelectedBg);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, SelectedBg);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  SelectedBg);
                }

                const ImVec2 RowStart = ImGui::GetCursorScreenPos();
                const bool bClicked = ImGui::Selectable("##Row", bIsSelected, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0, ItemHeight));
                const bool bHovered = ImGui::IsItemHovered();

                if (bIsSelected)
                {
                    ImGui::PopStyleColor(3);
                }

                if (bIsSelected)
                {
                    ImDrawList* DrawList = ImGui::GetWindowDrawList();
                    DrawList->AddRectFilled(
                        ImVec2(RowStart.x, RowStart.y + ItemHeight * 0.25f),
                        ImVec2(RowStart.x + 3.0f, RowStart.y + ItemHeight * 0.75f),
                        ImGui::GetColorU32(AccentColor));
                }
                
                if (bIsSelected && bPendingScrollToSelected)
                {
                    ImGui::SetScrollHereY(0.5f);
                }

                ImGui::SetCursorScreenPos(ImVec2(RowStart.x + 14.0f, RowStart.y + (ItemHeight - ImGui::GetTextLineHeight()) * 0.5f));
                DrawHighlightedText(Action.DisplayName.c_str(), Action.MatchIndices, MatchColor);

                if (!bGroupByCategory && !Action.Category.empty())
                {
                    const float CategoryWidth = ImGui::CalcTextSize(Action.Category.c_str()).x;
                    ImGui::SameLine(PopupSize.x - CategoryWidth - 20.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, CategoryColor);
                    ImGui::TextUnformatted(Action.Category.c_str());
                    ImGui::PopStyleColor();
                }

                if (bHovered && !Action.Tooltip.empty())
                {
                    ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 25.0f), ImVec2(200.0f, 700.0f));
                    ImGui::BeginTooltip();
                    ImGui::PushStyleColor(ImGuiCol_Text, AccentColor);
                    ImGui::TextUnformatted(Action.DisplayName.c_str());
                    ImGui::PopStyleColor();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::TextWrapped("%s", Action.Tooltip.c_str());
                    ImGui::PopStyleColor();
                    ImGui::EndTooltip();
                }

                if (bClicked)
                {
                    SelectedIndex = Position;
                    CommittedActionIndex = ActionIdx;
                    bCommitted = true;
                }

                ImGui::PopID();
            }

            if (VisibleIndices.empty())
            {
                const char* NoResults = QueryBuffer[0] == '\0' ? LOC("No nodes registered") : LOC("No nodes found");
                const float TextWidth = ImGui::CalcTextSize(NoResults).x;
                ImGui::SetCursorPosY(ListSize.y * 0.4f);
                ImGui::SetCursorPosX((PopupSize.x - TextWidth) * 0.5f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::TextUnformatted(NoResults);
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(4);

        if (bEnterCommitted && SelectedIndex >= 0 && SelectedIndex < static_cast<int32>(VisibleIndices.size()))
        {
            CommittedActionIndex = VisibleIndices[SelectedIndex];
            bCommitted = true;
        }

        bPendingScrollToSelected = false;
        LastSelectedIndex = SelectedIndex;

        if (bCommitted && CommittedActionIndex >= 0)
        {
            const FGraphAction& Action = Actions[CommittedActionIndex];
            CEdGraphNode* NewNode = Graph->CreateNode(Action.NodeClass);
            if (NewNode != nullptr)
            {
                ax::NodeEditor::SetNodePosition(NewNode->GetNodeID(),
                    ax::NodeEditor::ScreenToCanvas(ImGui::GetMousePosOnOpeningCurrentPopup()));

                if (Graph->PendingSourcePin != nullptr)
                {
                    if (CEdNodeGraphPin* TargetPin = Graph->FindAutoConnectPin(NewNode, Graph->PendingSourcePin))
                    {
                        Graph->TryAutoConnect(Graph->PendingSourcePin, TargetPin);
                    }
                }

                ax::NodeEditor::ClearSelection();
                ax::NodeEditor::SelectNode(NewNode->GetNodeID(), false);

                if (Graph->NodeSelectedCallback)
                {
                    Graph->NodeSelectedCallback(NewNode);
                }
            }
            Graph->PendingSourcePin = nullptr;
            return true;
        }

        return false;
    }
}
