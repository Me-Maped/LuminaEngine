#include <gtest/gtest.h>
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace
{
    // Real ImGui frames without a window system/GPU. Acknowledge texture requests exactly as a
    // renderer would, but do not upload pixels. No production engine context/global is replaced.
    class ImGuiUpgradeTest : public ::testing::Test
    {
    protected:
        ImGuiContext* Previous = nullptr;
        void SetUp() override
        {
            Previous = ImGui::GetCurrentContext();
            ImGui::CreateContext();
            ImGuiIO& IO = ImGui::GetIO();
            IO.IniFilename = nullptr;
            IO.LogFilename = nullptr;
            IO.DisplaySize = ImVec2(1280, 1024);
            IO.DeltaTime = 1.0f / 60.0f;
            IO.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
            IO.ConfigInputTrickleEventQueue = false;
        }
        void TearDown() override
        {
            if (GImGui->WithinFrameScope)
                ImGui::EndFrame();
            ImGui::DestroyContext();
            ImGui::SetCurrentContext(Previous);
        }
        void Frame(const std::function<void()>& Body)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(1000, 900), ImGuiCond_Always);
            ImGui::Begin("Upgrade tests");
            Body();
            ImGui::End();
            ImGui::Render();
            for (ImTextureData* Texture : ImGui::GetPlatformIO().Textures)
            {
                if (Texture->Status == ImTextureStatus_WantCreate)
                    Texture->SetTexID(static_cast<ImTextureID>(Texture->UniqueID + 1));
                if (Texture->Status == ImTextureStatus_WantCreate || Texture->Status == ImTextureStatus_WantUpdates)
                    Texture->SetStatus(ImTextureStatus_OK);
                else if (Texture->Status == ImTextureStatus_WantDestroy)
                {
                    Texture->SetTexID(ImTextureID_Invalid);
                    Texture->SetStatus(ImTextureStatus_Destroyed);
                }
            }
        }
        void Key(ImGuiKey Value, const std::function<void()>& Body)
        {
            ImGui::GetIO().AddKeyEvent(Value, true);
            Frame(Body);
            ImGui::GetIO().AddKeyEvent(Value, false);
            Frame(Body);
        }
        static void ExpectValidGeometry(const ImDrawList& List)
        {
            for (const ImDrawVert& Vertex : List.VtxBuffer)
            {
                EXPECT_TRUE(std::isfinite(Vertex.pos.x));
                EXPECT_TRUE(std::isfinite(Vertex.pos.y));
                EXPECT_TRUE(std::isfinite(Vertex.uv.x));
                EXPECT_TRUE(std::isfinite(Vertex.uv.y));
            }
            for (const ImDrawCmd& Command : List.CmdBuffer)
            {
                ASSERT_LE(Command.IdxOffset + Command.ElemCount, static_cast<unsigned>(List.IdxBuffer.Size));
                for (unsigned Index = Command.IdxOffset; Index < Command.IdxOffset + Command.ElemCount; ++Index)
                    ASSERT_LT(Command.VtxOffset + List.IdxBuffer[Index], static_cast<unsigned>(List.VtxBuffer.Size));
            }
        }
    };
}

TEST_F(ImGuiUpgradeTest, DeepTreesBalanceAt31And32And64IncludingClippedNodes)
{
    for (ImGuiTreeNodeFlags Lines : {ImGuiTreeNodeFlags_DrawLinesFull, ImGuiTreeNodeFlags_DrawLinesToNodes})
        Frame([&]
        {
            ImGuiWindow* Window = ImGui::GetCurrentWindow();
            const int InitialIDs = Window->IDStack.Size;
            for (int Depth = 0; Depth < 70; ++Depth)
            {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                EXPECT_TRUE(ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(Depth + 1)), Lines, "Depth %d", Depth));
                EXPECT_EQ(Window->DC.TreeDepth, Depth + 1);
            }
            for (int Depth = 0; Depth < 70; ++Depth)
                ImGui::TreePop();
            EXPECT_EQ(Window->DC.TreeDepth, 0);
            EXPECT_EQ(Window->IDStack.Size, InitialIDs);
            EXPECT_EQ(GImGui->TreeNodeStack.Size, 0);
        });
}

TEST_F(ImGuiUpgradeTest, ReadOnlyWrappedInputHomePreservesUtf8)
{
    char Text[512] = "first line\nChinese: \xe4\xb8\xad\xe6\x96\x87 and enough words to wrap around a narrow text field";
    const std::string Original = Text;
    ImGuiID ID = 0;
    auto Input = [&]
    {
        ID = ImGui::GetID("readonly");
        ImGui::InputTextMultiline("readonly", Text, sizeof(Text), ImVec2(180, 200),
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
    };
    Frame(Input);
    Frame([&] { ImGui::SetKeyboardFocusHere(); Input(); });
    Frame(Input);
    ASSERT_NE(ImGui::GetInputTextState(ID), nullptr);
    Key(ImGuiKey_End, Input);
    const int BeforeHome = ImGui::GetInputTextState(ID)->GetCursorPos();
    Key(ImGuiKey_Home, Input);
    EXPECT_LE(ImGui::GetInputTextState(ID)->GetCursorPos(), BeforeHome);
    EXPECT_STREQ(Text, Original.c_str());
}

TEST_F(ImGuiUpgradeTest, ReadOnlyWrappedEmptyInputHome)
{
    char Text[1] = "";
    ImGuiID ID = 0;
    auto Input = [&]
    {
        ID = ImGui::GetID("empty");
        ImGui::InputTextMultiline("empty", Text, sizeof(Text), ImVec2(180, 100),
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
    };
    Frame(Input);
    Frame([&] { ImGui::SetKeyboardFocusHere(); Input(); });
    Frame(Input);
    ASSERT_NE(ImGui::GetInputTextState(ID), nullptr);
    Key(ImGuiKey_Home, Input);
    EXPECT_EQ(ImGui::GetInputTextState(ID)->GetCursorPos(), 0);
    EXPECT_EQ(Text[0], '\0');
}

TEST_F(ImGuiUpgradeTest, DynamicWindowTitleKeepsDrawListOwnerAlive)
{
    Frame([] { ImGui::Begin("A###StableOwner"); ImGui::End(); });
    Frame([] {}); // Reactivating a window makes its persisted visible name update.
    const std::string LongTitle = std::string(4096, 'z') + "###StableOwner";
    Frame([&]
    {
        ImGui::Begin(LongTitle.c_str());
        ImGuiWindow* Window = ImGui::GetCurrentWindow();
        EXPECT_STREQ(Window->Name, LongTitle.c_str());
        EXPECT_EQ(Window->DrawList->_OwnerName, Window->Name);
        // Avoid dereferencing a stale pointer on the pre-fix core.
        if (Window->DrawList->_OwnerName == Window->Name)
        {
            EXPECT_STREQ(Window->DrawList->_OwnerName, LongTitle.c_str());
        }
        ImGui::End();
    });
}

TEST_F(ImGuiUpgradeTest, PopupOpeningPositionRemainsQueryableAfterClose)
{
    const ImVec2 Opening(123, 234);
    ImGui::GetIO().AddMousePosEvent(Opening.x, Opening.y);
    Frame([&]
    {
        ImGui::OpenPopup("popup");
        ASSERT_TRUE(ImGui::BeginPopup("popup"));
        ImGui::CloseCurrentPopup();
        EXPECT_FALSE(ImGui::IsPopupOpen("popup"));
        const ImVec2 Actual = ImGui::GetMousePosOnOpeningCurrentPopup();
        EXPECT_FLOAT_EQ(Actual.x, Opening.x);
        EXPECT_FLOAT_EQ(Actual.y, Opening.y);
        ImGui::EndPopup();
    });
}

TEST_F(ImGuiUpgradeTest, LogarithmicSignedRangesStayFiniteAndMonotonic)
{
    // Exercise the linked implementation rather than template specializations that optimized
    // ImGui archives may inline away. The old integer multiplication overflows in this path.
    Frame([&]
    {
        const ImRect Bounds(ImVec2(50, 100), ImVec2(350, 125));
        const auto Check32 = [&](int Min, int Max)
        {
            float PreviousPosition = -FLT_MAX;
            for (int Step = 0; Step <= 10; ++Step)
            {
                int Value = static_cast<int>(static_cast<long long>(Min) + (static_cast<long long>(Max) - Min) * Step / 10);
                const int Original = Value;
                ImRect Grab;
                EXPECT_FALSE(ImGui::SliderBehavior(Bounds, ImGui::GetID("log32"), ImGuiDataType_S32,
                    &Value, &Min, &Max, "%d", ImGuiSliderFlags_Logarithmic, &Grab));
                const float Position = (Grab.Min.x + Grab.Max.x) * 0.5f;
                EXPECT_TRUE(std::isfinite(Position));
                EXPECT_GE(Position, PreviousPosition);
                EXPECT_GE(Grab.Min.x, Bounds.Min.x);
                EXPECT_LE(Grab.Max.x, Bounds.Max.x);
                EXPECT_EQ(Value, Original);
                PreviousPosition = Position;
            }
        };
        Check32(-1000000000, 1000000000);
        Check32(50000, 1000000000);
        Check32(-1000000000, -50000);
        const ImS64 Min = -4000000000000000000LL, Max = 4000000000000000000LL;
        ImS64 Value = 0;
        ImRect Grab;
        EXPECT_FALSE(ImGui::SliderBehavior(Bounds, ImGui::GetID("log64"), ImGuiDataType_S64,
            &Value, &Min, &Max, "%lld", ImGuiSliderFlags_Logarithmic, &Grab));
        EXPECT_NEAR((Grab.Min.x + Grab.Max.x) * 0.5f, (Bounds.Min.x + Bounds.Max.x) * 0.5f, 1);
        EXPECT_EQ(Value, 0);
    });
}

TEST_F(ImGuiUpgradeTest, SortableTableSavesUntouchedColumnIdentities)
{
    auto Table = []
    {
        ASSERT_TRUE(ImGui::BeginTable("saved", 3, ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_SizingStretchSame));
        ImGui::TableSetupColumn("A", 0, 1, 0xA);
        ImGui::TableSetupColumn("B", 0, 1, 0xB);
        ImGui::TableSetupColumn("C", 0, 1, 0xC);
        ImGui::TableHeadersRow();
        ASSERT_NE(ImGui::TableGetSortSpecs(), nullptr);
        EXPECT_EQ(ImGui::TableGetSortSpecs()->Specs[0].ColumnUserID, 0xAu);
        ImGui::EndTable();
    };
    Frame(Table);
    Frame(Table);
    const std::string Ini = ImGui::SaveIniSettingsToMemory();
    for (const char* Column : {"A", "B", "C"})
    {
        char Identity[32];
        std::snprintf(Identity, sizeof(Identity), "ID=0x%08X", ImHashStr(Column));
        EXPECT_NE(Ini.find(Identity), std::string::npos) << Ini;
    }
    ImGui::LoadIniSettingsFromMemory(Ini.c_str());
    Frame(Table);
}

TEST_F(ImGuiUpgradeTest, InsertedTableColumnRespectsReorderPolicy)
{
    for (bool Reorderable : {false, true})
    {
        const char* Name = Reorderable ? "reorder" : "fixed";
        ImGuiID ID = 0;
        Frame([&] { ID = ImGui::GetID(Name); });
        char Ini[256];
        // Persisted order can predate the application's switch to a non-reorderable table.
        std::snprintf(Ini, sizeof(Ini), "[Table][0x%08X,2]\nColumn 0 Order=1 ID=0x%08X\nColumn 1 Order=0 ID=0x%08X\n\n",
            ID, ImHashStr("A"), ImHashStr("C"));
        ImGui::LoadIniSettingsFromMemory(Ini);
        Frame([&]
        {
            ASSERT_TRUE(ImGui::BeginTable(Name, 3, ImGuiTableFlags_Sortable | (Reorderable ? ImGuiTableFlags_Reorderable : 0)));
            ImGui::TableSetupColumn("A", 0, 0, 0xA);
            ImGui::TableSetupColumn("B", 0, 0, 0xB);
            ImGui::TableSetupColumn("C", 0, 0, 0xC);
            ImGui::TableHeadersRow();
            ImGuiTable* Table = ImGui::GetCurrentTable();
            EXPECT_EQ(Table->Columns[0].DisplayOrder, Reorderable ? 1 : 0);
            EXPECT_EQ(Table->Columns[1].DisplayOrder, Reorderable ? 2 : 1);
            EXPECT_EQ(Table->Columns[2].DisplayOrder, Reorderable ? 0 : 2);
            ImGui::EndTable();
        });
    }
}

TEST_F(ImGuiUpgradeTest, ForegroundAndBackgroundResetAfterLongUptime)
{
    GImGui->Time = 16777216.0; // Float seconds can no longer distinguish adjacent 60 Hz frames.
    Frame([]
    {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(1, 1), ImVec2(10, 10), IM_COL32_WHITE);
        ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(1, 1), ImVec2(10, 10), IM_COL32_WHITE);
    });
    Frame([]
    {
        EXPECT_EQ(ImGui::GetBackgroundDrawList()->VtxBuffer.Size, 0);
        EXPECT_EQ(ImGui::GetForegroundDrawList()->VtxBuffer.Size, 0);
    });
}

TEST_F(ImGuiUpgradeTest, LiveTableColumnInsertionPreservesDeclarationOrderWhenLocked)
{
    for (bool Reorderable : {false, true})
    {
        bool Insert = false;
        auto Table = [&]
        {
            ASSERT_TRUE(ImGui::BeginTable(Reorderable ? "live reorder" : "live fixed", Insert ? 3 : 2,
                ImGuiTableFlags_Sortable | (Reorderable ? ImGuiTableFlags_Reorderable : 0)));
            ImGui::TableSetupColumn("A");
            if (Insert) ImGui::TableSetupColumn("B");
            ImGui::TableSetupColumn("C");
            ImGui::TableHeadersRow();
            if (Insert)
            {
                ImGuiTable* Current = ImGui::GetCurrentTable();
                EXPECT_EQ(Current->Columns[0].DisplayOrder, 0);
                EXPECT_EQ(Current->Columns[1].DisplayOrder, Reorderable ? 2 : 1);
                EXPECT_EQ(Current->Columns[2].DisplayOrder, Reorderable ? 1 : 2);
            }
            ImGui::EndTable();
        };
        Frame(Table);
        Frame(Table);
        Insert = true;
        Frame(Table);
        Frame(Table);
    }
}

TEST_F(ImGuiUpgradeTest, TextureQueuedForNextFrameDestructionCanStillDraw)
{
    ImTextureData Texture;
    Texture.SetTexID(static_cast<ImTextureID>(123));
    Texture.SetStatus(ImTextureStatus_OK);
    Texture.WantDestroyNextFrame = true;
    Frame([&]
    {
        ImDrawList* List = ImGui::GetForegroundDrawList();
        List->AddImage(Texture.GetTexRef(), ImVec2(10, 10), ImVec2(30, 30));
        EXPECT_EQ(List->VtxBuffer.Size, 4);
        EXPECT_EQ(List->CmdBuffer.front().GetTexID(), static_cast<ImTextureID>(123));
    });
}

TEST_F(ImGuiUpgradeTest, DisabledColorSwatchesPreserveColorAndAlphaPreview)
{
    ImGui::GetStyle().Alpha = 0.8f;
    ImGui::GetStyle().DisabledAlpha = 0.25f;
    ImGui::GetStyle().FrameRounding = 0;
    Frame([&]
    {
        for (ImGuiColorEditFlags Preview : {0, static_cast<int>(ImGuiColorEditFlags_AlphaNoBg),
                                            static_cast<int>(ImGuiColorEditFlags_AlphaPreviewHalf)})
        {
            ImGui::PushID(Preview);
            const ImGuiColorEditFlags Flags = Preview | ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip;
            ImDrawList* List = ImGui::GetWindowDrawList();
            const int EnabledBegin = List->VtxBuffer.Size;
            ImGui::ColorButton("enabled", ImVec4(0.8f, 0.2f, 0.4f, 0.5f), Flags, ImVec2(60, 30));
            std::vector<ImU32> Colors;
            for (int Index = EnabledBegin; Index < List->VtxBuffer.Size; ++Index)
                Colors.push_back(List->VtxBuffer[Index].col);
            const int DisabledBegin = List->VtxBuffer.Size;
            ImGui::BeginDisabled();
            EXPECT_FALSE(ImGui::ColorButton("disabled", ImVec4(0.8f, 0.2f, 0.4f, 0.5f), Flags, ImVec2(60, 30)));
            ImGui::EndDisabled();
            EXPECT_EQ(List->VtxBuffer.Size - DisabledBegin, static_cast<int>(Colors.size()));
            for (int Index = 0; Index < static_cast<int>(Colors.size()) && DisabledBegin + Index < List->VtxBuffer.Size; ++Index)
                EXPECT_EQ(List->VtxBuffer[DisabledBegin + Index].col, Colors[Index]);
            EXPECT_FLOAT_EQ(ImGui::GetStyle().Alpha, 0.8f);
            ImGui::PopID();
        }
    });
}

TEST_F(ImGuiUpgradeTest, FiltersSupportAndQuotedPhrasesAndExclusionPriority)
{
    ImGuiTextFilter And("red fox");
    EXPECT_TRUE(And.PassFilter("fox is red"));
    EXPECT_FALSE(And.PassFilter("red bird"));
    ImGuiTextFilter Phrase("\"red fox\"");
    EXPECT_TRUE(Phrase.PassFilter("a red fox"));
    EXPECT_FALSE(Phrase.PassFilter("red quick fox"));
    ImGuiTextFilter Exclude("fox,-blue");
    EXPECT_TRUE(Exclude.PassFilter("red fox"));
    EXPECT_FALSE(Exclude.PassFilter("blue fox"));
    ImGuiTextFilter Or("fox,bird");
    EXPECT_TRUE(Or.PassFilter("bird"));
    EXPECT_FALSE(Or.PassFilter("cat"));
}

TEST_F(ImGuiUpgradeTest, MixedScalarDisplayDoesNotOverwriteBackingValues)
{
    int Integer = 173;
    float Floating = 0.314f;
    Frame([&]
    {
        ImGui::LogToBuffer();
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        EXPECT_FALSE(ImGui::SliderInt("mixed slider", &Integer, 0, 200));
        EXPECT_FALSE(ImGui::DragFloat("mixed drag", &Floating));
        ImGui::PopItemFlag();
        const std::string Log = GImGui->LogBuffer.c_str();
        EXPECT_NE(Log.find("-"), std::string::npos) << Log;
        EXPECT_EQ(Log.find("173"), std::string::npos);
        EXPECT_EQ(Log.find("0.314"), std::string::npos);
        ImGui::LogFinish();
    });
    EXPECT_EQ(Integer, 173);
    EXPECT_FLOAT_EQ(Floating, 0.314f);
}

TEST_F(ImGuiUpgradeTest, EscapeCancelsSliderAndDragToOriginalValue)
{
    for (bool Slider : {false, true})
    {
        float Value = 0.25f;
        ImVec2 Center;
        ImGuiID ID = 0;
        auto Widget = [&]
        {
            ImGui::SetNextItemWidth(300);
            ID = ImGui::GetID(Slider ? "slider" : "drag");
            if (Slider) ImGui::SliderFloat("slider", &Value, 0, 1);
            else ImGui::DragFloat("drag", &Value, 0.01f, 0, 1);
            Center = ImVec2(ImGui::GetItemRectMin().x + 75, (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
        };
        Frame(Widget);
        ImGui::GetIO().AddMousePosEvent(Center.x, Center.y);
        Frame(Widget);
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        Frame(Widget);
        ASSERT_EQ(GImGui->ActiveId, ID);
        ImGui::GetIO().AddMousePosEvent(Center.x + 80, Center.y);
        Frame(Widget);
        ASSERT_NE(Value, 0.25f);
        Key(ImGuiKey_Escape, Widget);
        EXPECT_FLOAT_EQ(Value, 0.25f);
        EXPECT_NE(GImGui->ActiveId, ID);
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        Frame(Widget);
    }
}

TEST_F(ImGuiUpgradeTest, DrawGeometryStaysFiniteAndSupportsLargeVertexOffsets)
{
    Frame([&]
    {
        ImDrawList* List = ImGui::GetForegroundDrawList();
        for (int Index = 0; Index < 17000; ++Index)
            List->AddRectFilled(ImVec2(10, 10), ImVec2(12, 12), IM_COL32_WHITE);
        const ImVec2 Sharp[] = {{20, 20}, {80, 20}, {20.001f, 20.001f}, {50, 50}};
        for (float Thickness : {0.1f, 1.0f, 8.0f, 40.0f})
        {
            List->AddPolyline(Sharp, 4, IM_COL32_WHITE, Thickness, ImDrawFlags_Closed);
            List->AddCircle(ImVec2(100, 100), 0.01f, IM_COL32_WHITE, 0, Thickness);
            List->AddEllipse(ImVec2(200, 200), ImVec2(150, 1), IM_COL32_WHITE, 0, 0, Thickness);
        }
        ExpectValidGeometry(*List);
        bool HasOffset = false;
        for (const ImDrawCmd& Command : List->CmdBuffer)
            HasOffset |= Command.VtxOffset > 0;
        EXPECT_TRUE(HasOffset);
    });
}

TEST_F(ImGuiUpgradeTest, NewElasticRowAfter64KVerticesMovesOnlyItsOwnGeometry)
{
    Frame([&]
    {
        ImDrawList* List = ImGui::GetWindowDrawList();
        for (int Index = 0; Index < 17000; ++Index)
            List->AddRectFilled(ImVec2(1000, 1000), ImVec2(1002, 1002), IM_COL32_WHITE);
        ASSERT_GT(List->VtxBuffer.Size, 65535);
        std::vector<ImVec2> EarlierPositions;
        for (const ImDrawVert& Vertex : List->VtxBuffer)
            EarlierPositions.push_back(Vertex.pos);

        const ImVec2 Start = ImGui::GetCursorScreenPos();
        ImGui::BeginHorizontal("new row beyond 64K", ImVec2(300, 40));
        ImGui::Dummy(ImVec2(80, 20));
        ImGui::Spring();
        const int MarkerIndex = List->VtxBuffer.Size;
        const ImVec2 MarkerPosition = ImGui::GetCursorScreenPos();
        List->AddRectFilled(MarkerPosition, ImVec2(MarkerPosition.x + 80, MarkerPosition.y + 20), IM_COL32_WHITE);
        ImGui::Dummy(ImVec2(80, 20));
        ImGui::EndHorizontal();

        // The row's first frame must rebalance its spring after the marker was already drawn.
        EXPECT_FLOAT_EQ(List->VtxBuffer[MarkerIndex].pos.x, Start.x + 220);
        int FirstMovedEarlierVertex = -1;
        for (int Index = 0; Index < static_cast<int>(EarlierPositions.size()); ++Index)
            if (List->VtxBuffer[Index].pos.x != EarlierPositions[Index].x ||
                List->VtxBuffer[Index].pos.y != EarlierPositions[Index].y)
            {
                FirstMovedEarlierVertex = Index;
                break;
            }
        EXPECT_EQ(FirstMovedEarlierVertex, -1) << "Elastic layout modified geometry submitted before the row";
        ExpectValidGeometry(*List);
    });
}

#if IMGUI_VERSION_NUM >= 19300
TEST_F(ImGuiUpgradeTest, StrokePlacementDistinguishesInsideAndOutside)
{
    Frame([&]
    {
        ImDrawList* List = ImGui::GetForegroundDrawList();
        List->Flags &= ~ImDrawFlags_AALines;
        const int Begin = List->VtxBuffer.Size;
        List->AddRect(ImVec2(100, 100), ImVec2(200, 200), IM_COL32_WHITE, 0.0f, 10.0f, ImDrawFlags_StrokeInside);
        ASSERT_GT(List->VtxBuffer.Size, Begin);
        for (int Index = Begin; Index < List->VtxBuffer.Size; ++Index)
        {
            EXPECT_GE(List->VtxBuffer[Index].pos.x, 100);
            EXPECT_LE(List->VtxBuffer[Index].pos.x, 200);
            EXPECT_GE(List->VtxBuffer[Index].pos.y, 100);
            EXPECT_LE(List->VtxBuffer[Index].pos.y, 200);
        }
        const int OutsideBegin = List->VtxBuffer.Size;
        List->AddRect(ImVec2(100, 100), ImVec2(200, 200), IM_COL32_WHITE, 0.0f, 10.0f, ImDrawFlags_StrokeOutside);
        bool ExtendsOutside = false;
        for (int Index = OutsideBegin; Index < List->VtxBuffer.Size; ++Index)
            ExtendsOutside |= List->VtxBuffer[Index].pos.x < 100 || List->VtxBuffer[Index].pos.x > 200;
        EXPECT_TRUE(ExtendsOutside);
        ExpectValidGeometry(*List);
    });
}
#endif

TEST_F(ImGuiUpgradeTest, DockingAndElasticLayoutsAndFontSizeRemainAvailable)
{
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImVec2 First, Second;
    for (int Index = 0; Index < 3; ++Index)
        Frame([&]
        {
            const float OriginalSize = ImGui::GetFontSize();
            ImGui::PushFontSize(30);
            EXPECT_FLOAT_EQ(ImGui::GetFontSize(), 30);
            ImGui::PopFontSize();
            EXPECT_FLOAT_EQ(ImGui::GetFontSize(), OriginalSize);
            ImGui::BeginHorizontal("elastic", ImVec2(300, 30));
            ImGui::Button("first", ImVec2(40, 20));
            First = ImGui::GetItemRectMin();
            ImGui::Spring();
            ImGui::BeginVertical("nested");
            ImGui::Button("second", ImVec2(40, 20));
            Second = ImGui::GetItemRectMin();
            ImGui::EndVertical();
            ImGui::EndHorizontal();
            const ImGuiID Dock = ImGui::GetID("dockspace");
            ImGui::DockSpace(Dock, ImVec2(500, 300));
            ImGui::SetNextWindowDockID(Dock, ImGuiCond_Always);
            ImGui::Begin("Docked test window");
            EXPECT_EQ(ImGui::GetCurrentWindow()->DockId, Dock);
            EXPECT_NE(ImGui::GetCurrentWindow()->DockNode, nullptr);
            ImGui::End();
        });
    EXPECT_GT(Second.x - First.x, 200);
    EXPECT_NEAR(Second.y, First.y, 1);
}
