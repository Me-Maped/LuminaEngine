#include "ProfilerEditorTool.h"
#include "ProfilerViewCommon.h"

#include "imgui.h"

#if USING(WITH_EDITOR)

namespace Lumina
{
    namespace
    {
        void FormatBytes(uint64 Bytes, char* Out, size_t Capacity)
        {
            if (Bytes >= Constants::kMiB)
            {
                snprintf(Out, Capacity, LOC("%.1f MiB"), (double)Bytes / double(Constants::kMiB));
            }
            else if (Bytes >= 1024ull)
            {
                snprintf(Out, Capacity, LOC("%.1f KiB"), (double)Bytes / double(Constants::kKiB));
            }
            else
            {
                snprintf(Out, Capacity, LOC("%llu B"), (unsigned long long)Bytes);
            }
        }

        // Read at a glance only; the thresholds mark what is worth opening rather than what is wrong.
        ImVec4 CostColor(double Ms)
        {
            if (Ms >= 50.0) return ImVec4(0.89f, 0.41f, 0.36f, 1.0f);
            if (Ms >= 10.0) return ImVec4(0.88f, 0.69f, 0.33f, 1.0f);
            return ImVec4(0.59f, 0.78f, 0.63f, 1.0f);
        }

        ImVec4 OutcomeColor(EAssetLoadOutcome Outcome)
        {
            switch (Outcome)
            {
            case EAssetLoadOutcome::Loaded:          return ImVec4(0.59f, 0.78f, 0.63f, 1.0f);
            case EAssetLoadOutcome::AlreadyResident: return ImVec4(0.55f, 0.60f, 0.68f, 1.0f);
            case EAssetLoadOutcome::Joined:          return ImVec4(0.52f, 0.68f, 0.86f, 1.0f);
            case EAssetLoadOutcome::Failed:          return ImVec4(0.90f, 0.42f, 0.38f, 1.0f);
            }
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        void SortStats(TVector<FAssetLoadStat>& Stats, const ImGuiTableColumnSortSpecs& Spec)
        {
            const bool bAscending = Spec.SortDirection == ImGuiSortDirection_Ascending;
            Algo::Sort(Stats,
                [&Spec, bAscending](const FAssetLoadStat& A, const FAssetLoadStat& B)
                {
                    double LHS = 0.0;
                    double RHS = 0.0;
                    switch (Spec.ColumnIndex)
                    {
                    case 1: LHS = A.Count;         RHS = B.Count;         break;
                    case 2: LHS = A.LastMs;        RHS = B.LastMs;        break;
                    case 3: LHS = A.AverageMs();   RHS = B.AverageMs();   break;
                    case 4: LHS = A.MaxMs;         RHS = B.MaxMs;         break;
                    case 5: LHS = A.TotalMs;       RHS = B.TotalMs;       break;
                    case 6: LHS = (double)A.Bytes; RHS = (double)B.Bytes; break;
                    default:
                        return bAscending
                            ? A.Name.ToString() < B.Name.ToString()
                            : B.Name.ToString() < A.Name.ToString();
                    }
                    return bAscending ? LHS < RHS : RHS < LHS;
                });
        }
    }

    void FProfilerEditorTool::DrawAssetTable(const char* Id, TVector<FAssetLoadStat>& Stats, bool bShowSize)
    {
        if (Stats.empty())
        {
            ImGui::TextDisabled(LOC("Nothing recorded at this layer yet."));
            return;
        }

        constexpr ImGuiTableFlags Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

        if (!ImGui::BeginTable(Id, bShowSize ? 8 : 7, Flags, ImVec2(0.0f, 240.0f)))
        {
            return;
        }

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(LOC_ID("Name"),  ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn(LOC_ID("Reqs"),  ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn(LOC_ID("Last"),  ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn(LOC_ID("Avg"),   ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn(LOC_ID("Max"),   ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn(LOC_ID("Total"), ImGuiTableColumnFlags_WidthFixed, 72.0f);
        if (bShowSize)
        {
            ImGui::TableSetupColumn(LOC_ID("Size"), ImGuiTableColumnFlags_WidthFixed, 74.0f);
        }
        ImGui::TableSetupColumn(LOC_ID("Outcome"), ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 130.0f);
        ImGui::TableHeadersRow();

        // Sorted on the display copy, so the tracker never has to hold an order for the UI's sake.
        if (ImGuiTableSortSpecs* Specs = ImGui::TableGetSortSpecs())
        {
            if (Specs->SpecsDirty && Specs->SpecsCount > 0)
            {
                SortStats(Stats, Specs->Specs[0]);
                Specs->SpecsDirty = false;
            }
        }

        char Buffer[32];
        for (const FAssetLoadStat& Stat : Stats)
        {
            const FString Name = Stat.Name.ToString();
            if (AssetFilter[0] != '\0' && Name.find(AssetFilter) == FString::npos)
            {
                continue;
            }

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(Name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%u", Stat.Count);

            ImGui::TableNextColumn();
            ImGui::TextColored(CostColor(Stat.LastMs), LOC("%.2f ms"), Stat.LastMs);
            ImGui::TableNextColumn();
            ImGui::Text(LOC("%.2f ms"), Stat.AverageMs());
            ImGui::TableNextColumn();
            ImGui::TextColored(CostColor(Stat.MaxMs), LOC("%.2f ms"), Stat.MaxMs);
            ImGui::TableNextColumn();
            ImGui::Text(LOC("%.1f ms"), Stat.TotalMs);

            if (bShowSize)
            {
                ImGui::TableNextColumn();
                FormatBytes(Stat.Bytes, Buffer, sizeof(Buffer));
                ImGui::TextUnformatted(Buffer);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(LOC("%u exports, %u imports"), Stat.Exports, Stat.Imports);
                }
            }

            // The breakdown is the point of the row; a name loaded once and hit fifty times is not slow.
            ImGui::TableNextColumn();
            bool bAny = false;
            auto Part = [&bAny](uint32 Count, EAssetLoadOutcome Outcome, const char* Suffix)
            {
                if (Count == 0)
                {
                    return;
                }
                if (bAny)
                {
                    ImGui::SameLine(0.0f, 4.0f);
                }
                ImGui::TextColored(OutcomeColor(Outcome), "%u%s", Count, Suffix);
                bAny = true;
            };
            Part(Stat.LoadedCount,  EAssetLoadOutcome::Loaded,          "L");
            Part(Stat.ResidentHits, EAssetLoadOutcome::AlreadyResident, "R");
            Part(Stat.JoinedCount,  EAssetLoadOutcome::Joined,          "J");
            Part(Stat.Failures,     EAssetLoadOutcome::Failed,          "F");
        }

        ImGui::EndTable();
    }

    void FProfilerEditorTool::DrawAssets()
    {
        FAssetLoadTracker& Tracker = FAssetLoadTracker::Get();

        if (!bFrozen)
        {
            Tracker.Snapshot(AssetRecent, AssetRequests, AssetPackages);
        }

        const uint32 Requests = Tracker.GetTotalRequests();
        const uint32 Resident = Tracker.GetResidentHits();

        ImGui::Text(LOC("%u requests"), Requests);
        ProfilerView::Divider();
        ImGui::Text(LOC("%.1f ms total"), Tracker.GetTotalRequestMs());
        ProfilerView::Divider();
        ImGui::Text(LOC("%u resident hits"), Resident);
        ProfilerView::Divider();
        if (ImGui::SmallButton(LOC("Clear")))
        {
            Tracker.Clear();
            AssetRecent.clear();
            AssetRequests.clear();
            AssetPackages.clear();
        }

        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##assetfilter", LOC("Filter by name"), AssetFilter, sizeof(AssetFilter));
        ImGui::SameLine();
        ImGui::TextDisabled(LOC("L loaded, R resident, J joined, F failed"));

        if (Requests == 0 && AssetPackages.empty())
        {
            ImGui::TextDisabled(LOC("No asset loads recorded yet. Open or import an asset."));
            return;
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader(LOC_ID("Requests (what was asked for)"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawAssetTable("##assetrequests", AssetRequests, false);
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader(LOC_ID("Packages (what the disk did)")))
        {
            DrawAssetTable("##assetpackages", AssetPackages, true);
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader(LOC_ID("Recent")))
        {
            if (ImGui::BeginChild("##assetrecent", ImVec2(0.0f, 200.0f), true))
            {
                // Newest first, which is the order you want when you just clicked something.
                for (size_t Index = AssetRecent.size(); Index > 0; --Index)
                {
                    const FAssetLoadRecord& Record = AssetRecent[Index - 1];
                    const FString Name = Record.Name.ToString();
                    if (AssetFilter[0] != '\0' && Name.find(AssetFilter) == FString::npos)
                    {
                        continue;
                    }

                    ImGui::TextColored(CostColor(Record.DurationMs), LOC("%7.2f ms"), Record.DurationMs);
                    ImGui::SameLine();
                    ImGui::TextColored(OutcomeColor(Record.Outcome), "%-8s", LOC_VIEW(LexAssetLoadOutcome(Record.Outcome)));
                    ImGui::SameLine();
                    ImGui::TextDisabled(Record.Source == EAssetLoadSource::Request ? LOC("req ") : LOC("pkg "));
                    ImGui::SameLine();
                    ImGui::TextUnformatted(Name.c_str());
                }
            }
            ImGui::EndChild();
        }
    }
}

#endif
