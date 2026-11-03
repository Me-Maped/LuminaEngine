#include <gtest/gtest.h>

#include "Config/EngineSettings.h"
#include "Containers/Algorithm.h"
#include "Core/Engine/Engine.h"
#include "Core/Object/Class.h"
#include "Core/Object/ObjectCore.h"
#include "Core/Reflection/PropertyTypeTraits.h"
#include "Core/Reflection/Type/LuminaTypes.h"
#include "Localization/Localization.h"
#include "UI/Properties/PropertyTable.h"

using namespace Lumina;

namespace
{
    using FRows = TVector<FPropertyTable::FRowProbe>;

    // The row implementation lives in PropertyTable.cpp, so a suite reads the tree through the
    // table's probe snapshot instead of reaching into the rows themselves.
    const FPropertyTable::FRowProbe* FindRow(const FRows& Rows, FStringView InternalName)
    {
        for (const FPropertyTable::FRowProbe& Row : Rows)
        {
            if (Row.InternalName == InternalName)
            {
                return &Row;
            }
        }
        return nullptr;
    }

    // The first child a container row grew. Rows are probed parents-first, so the row after a
    // container in that order is its first child; a map entry still carries its key property's name.
    const FPropertyTable::FRowProbe* FindChild(const FRows& Rows, const FPropertyTable::FRowProbe& Parent)
    {
        for (size_t Index = 0; Index + 1 < Rows.size(); ++Index)
        {
            if (&Rows[Index] == &Parent && Rows[Index + 1].Depth == Parent.Depth + 1)
            {
                return &Rows[Index + 1];
            }
        }
        return nullptr;
    }

    struct FNestedSearchData
    {
        float Speed = 3.0f;
        float Height = 4.0f;
    };

    struct FCompositeSearchData
    {
        THashMap<int32, float> Scores;
        THashMap<int32, float> Unrelated;
        FNestedSearchData Nested;
        TOptional<float> Optional;
    };

    // Supply only the customization registry queried by the table; no renderer or window.
    class FSearchTestUI final : public IDevelopmentToolUI
    {
    public:
        FSearchTestUI() { PropertyCustomizationRegistry = &Registry; }
        void Initialize(const FUpdateContext&) override {}
        void Deinitialize(const FUpdateContext&) override {}
    private:
        FPropertyCustomizationRegistry Registry;
    };

    class FSearchTestEngine final : public FEngine
    {
    public:
        FSearchTestEngine() { DeveloperToolUI.Reset(CreateDevelopmentTools()); }
        IDevelopmentToolUI* CreateDevelopmentTools() override { return new FSearchTestUI(); }
    };

    class PropertySearchTest : public ::testing::Test
    {
    protected:
        FSearchTestEngine Engine;
        FEngine* PreviousEngine = nullptr;
        CEditorSettings* Settings = nullptr;
        TVector<CStruct*> OwnedTypes;

        void SetUp() override
        {
            PreviousEngine = GEngine;
            GEngine = &Engine;
            RegisterPendingTypes();
            Settings = NewObject<CEditorSettings>();
            ASSERT_NE(Settings, nullptr);
            Settings->AddToRoot();
            Settings->MaxBackgroundFPS = 37;
            Settings->RecentProjects.clear();
            ASSERT_TRUE(FLocalization::Get().SetLanguageFromString(ELanguage::ChineseSimplified,
                R"({"Max Background FPS":"后台帧率","Performance|category":"性能",
                    "Recent Projects":"最近工程","Loading|category":"加载"})"));
            const FProperty* Property = CEditorSettings::StaticClass()->GetProperty(FName("MaxBackgroundFPS"));
            ASSERT_NE(Property, nullptr);
            ASSERT_STREQ(Property->GetPropertyDisplayName().c_str(), "Max Background FPS");
        }

        void TearDown() override
        {
            FLocalization::Get().ClearTable();
            if (Settings != nullptr) Settings->RemoveFromRoot();
            for (CStruct* Type : OwnedTypes) Type->RemoveFromRoot();
            GEngine = PreviousEngine;
        }

        static bool Matches(FPropertyTable& Table, FStringView Query)
        {
            Table.SetSearchText(Query);
            return Table.PrepareAndTestFilter();
        }

        CStruct* MakeCompositeType()
        {
            CStruct* Inner = NewObject<CStruct>();
            Inner->Size = sizeof(FNestedSearchData);
            Inner->Alignment = alignof(FNestedSearchData);
            Inner->AddToRoot();
            OwnedTypes.push_back(Inner);
            static constexpr FPropertyDesc InnerProperties[] = {
                {.Name = "Speed", .Offset = offsetof(FNestedSearchData, Speed), .Flags = EPropertyFlags::Editable, .Type = PropertyTypeOf<float>},
                {.Name = "Height", .Offset = offsetof(FNestedSearchData, Height), .Flags = EPropertyFlags::Editable, .Type = PropertyTypeOf<float>},
            };
            InitializeAndCreateFProperties(Inner, InnerProperties, std::size(InnerProperties));
            Inner->Link();

            CStruct* Outer = NewObject<CStruct>();
            Outer->Size = sizeof(FCompositeSearchData);
            Outer->Alignment = alignof(FCompositeSearchData);
            Outer->AddToRoot();
            OwnedTypes.push_back(Outer);
            // Runtime descriptors live in the type's arena, just like script-minted properties.
            auto& Arena = Outer->GetPropertyArena();
            const auto* NestedType = Arena.EmplaceData<FPropertyType>(FPropertyType{.Kind = EPropertyTypeFlags::Struct, .Resolved = Inner});
            const auto* Properties = Arena.EmplaceData<std::array<FPropertyDesc, 4>>(std::array<FPropertyDesc, 4>{
                FPropertyDesc{.Name = "Scores", .Offset = offsetof(FCompositeSearchData, Scores), .Flags = EPropertyFlags::Editable,
                    .Type = PropertyTypeOf<THashMap<int32, float>>},
                FPropertyDesc{.Name = "Unrelated", .Offset = offsetof(FCompositeSearchData, Unrelated), .Flags = EPropertyFlags::Editable,
                    .Type = PropertyTypeOf<THashMap<int32, float>>},
                FPropertyDesc{.Name = "Nested", .Offset = offsetof(FCompositeSearchData, Nested), .Flags = EPropertyFlags::Editable,
                    .Type = NestedType, .EditorFlags = EPropertyEditorFlags::DefaultCollapsed},
                FPropertyDesc{.Name = "Optional", .Offset = offsetof(FCompositeSearchData, Optional), .Flags = EPropertyFlags::Editable,
                    .Type = PropertyTypeOf<TOptional<float>>},
            });
            InitializeAndCreateFProperties(Outer, Properties->data(), Properties->size());
            Outer->Link();
            return Outer;
        }

    };
}

TEST_F(PropertySearchTest, RealMetadataSourceTranslationAndInternalNameRemainSearchable)
{
    FPropertyTable Table(Settings);
    for (const char* Query : {"Background", "后台", "MAXBACKGROUNDFPS", "Background 帧率"})
        EXPECT_TRUE(Matches(Table, Query)) << Query;
    EXPECT_FALSE(Matches(Table, "Background missing"));
    EXPECT_EQ(Settings->MaxBackgroundFPS, 37);
}

TEST_F(PropertySearchTest, CategoryAndOwningComponentContextCombineWithPropertyTerms)
{
    FPropertyTable Table(Settings);
    EXPECT_TRUE(Matches(Table, "Performance 帧率"));
    EXPECT_TRUE(Matches(Table, "性能 Background"));
    {
        FString TemporaryAlias = "MovementSystem";
        Table.SetSearchContext({TemporaryAlias, "移动系统"});
    } // Retained aliases must own their text after the caller's temporary is gone.
    EXPECT_TRUE(Matches(Table, "MovementSystem 帧率"));
    EXPECT_TRUE(Matches(Table, "移动系统 Background"));
    EXPECT_FALSE(Matches(Table, "MovementSystem missing"));
    EXPECT_EQ(Settings->MaxBackgroundFPS, 37);
}

TEST_F(PropertySearchTest, ExistingTableReadsReloadedTranslationWithoutLosingSource)
{
    FPropertyTable Table(Settings);
    ASSERT_TRUE(Matches(Table, "后台"));
    ASSERT_TRUE(FLocalization::Get().SetLanguageFromString(ELanguage::ChineseSimplified,
        R"({"Max Background FPS":"空闲速率"})"));
    EXPECT_FALSE(Matches(Table, "后台"));
    EXPECT_TRUE(Matches(Table, "空闲速率"));
    EXPECT_TRUE(Matches(Table, "Background"));
    ASSERT_TRUE(FLocalization::Get().SetLanguageFromString(ELanguage::English, "{}"));
    EXPECT_TRUE(Matches(Table, "Background"));
    EXPECT_FALSE(Matches(Table, "空闲速率"));
}

TEST_F(PropertySearchTest, EmptyReflectedArrayHeaderMatchesWithoutInventingElementNames)
{
    FPropertyTable Table(Settings);
    ASSERT_TRUE(Settings->RecentProjects.empty());
    EXPECT_TRUE(Matches(Table, "Recent 工程"));
    EXPECT_TRUE(Matches(Table, "recentprojects"));
    EXPECT_TRUE(Matches(Table, "Loading 工程"));
    EXPECT_TRUE(Matches(Table, "加载 Projects"));
    EXPECT_FALSE(Matches(Table, "Recent nonexistent"));
    EXPECT_TRUE(Settings->RecentProjects.empty());
}

TEST_F(PropertySearchTest, ClearingAndChangingQueryRestoresUnfilteredVisibility)
{
    FPropertyTable Table(Settings);
    EXPECT_FALSE(Matches(Table, "a term absent from every reflected field"));
    Table.ClearSearch();
    EXPECT_TRUE(Table.PrepareAndTestFilter());
    EXPECT_TRUE(Matches(Table, "Recent"));
    EXPECT_TRUE(Matches(Table, ""));
    EXPECT_TRUE(Matches(Table, "Background"));
    EXPECT_EQ(Settings->MaxBackgroundFPS, 37);
    EXPECT_TRUE(Settings->RecentProjects.empty());
}

TEST_F(PropertySearchTest, PopulatedMapMatchIncludesEntriesWithoutForcingExpansion)
{
    FCompositeSearchData Data;
    Data.Scores.emplace(1, 2.0f);
    Data.Unrelated.emplace(9, 7.0f);
    FPropertyTable Table(&Data, MakeCompositeType(), nullptr);
    ASSERT_TRUE(Matches(Table, ""));
    ASSERT_TRUE(Table.SetRowExpanded("Scores", false));

    ASSERT_TRUE(Matches(Table, "Scores"));
    FRows Rows;
    Table.ProbeRows(Rows);
    const FPropertyTable::FRowProbe* Scores = FindRow(Rows, "Scores");
    const FPropertyTable::FRowProbe* Unrelated = FindRow(Rows, "Unrelated");
    ASSERT_NE(Scores, nullptr);
    ASSERT_NE(Unrelated, nullptr);
    EXPECT_FALSE(Scores->bExpanded);
    ASSERT_EQ(Scores->ChildCount, 1u);
    ASSERT_NE(FindChild(Rows, *Scores), nullptr);
    EXPECT_TRUE(FindChild(Rows, *Scores)->bPassesFilter);
    EXPECT_FALSE(Unrelated->bPassesFilter);
    EXPECT_FALSE(FindChild(Rows, *Unrelated)->bPassesFilter);

    // Opening a matching header must reveal the entry.
    ASSERT_TRUE(Table.SetRowExpanded("Scores", true));
    ASSERT_TRUE(Matches(Table, "Scores"));
    Table.ProbeRows(Rows);
    Scores = FindRow(Rows, "Scores");
    Unrelated = FindRow(Rows, "Unrelated");
    ASSERT_NE(Scores, nullptr);
    ASSERT_NE(Unrelated, nullptr);
    EXPECT_TRUE(Scores->bExpanded);
    EXPECT_TRUE(FindChild(Rows, *Scores)->bPassesFilter);
    Table.ClearSearch();
    Table.ProbeRows(Rows);
    Scores = FindRow(Rows, "Scores");
    Unrelated = FindRow(Rows, "Unrelated");
    ASSERT_NE(Scores, nullptr);
    ASSERT_NE(Unrelated, nullptr);
    EXPECT_FALSE(Scores->bExpanded);
    EXPECT_TRUE(FindChild(Rows, *Unrelated)->bPassesFilter);
    EXPECT_EQ(Data.Scores.at(1), 2.0f);
}

TEST_F(PropertySearchTest, MatchingStructShowsContentButLeafSearchStillFiltersOtherMembers)
{
    FCompositeSearchData Data;
    FPropertyTable Table(&Data, MakeCompositeType(), nullptr);
    ASSERT_TRUE(Matches(Table, "Nested"));
    FRows Rows;
    Table.ProbeRows(Rows);
    const FPropertyTable::FRowProbe* Nested = FindRow(Rows, "Nested");
    ASSERT_NE(Nested, nullptr);
    EXPECT_FALSE(Nested->bExpanded);
    EXPECT_EQ(Nested->ChildCount, 2u);

    ASSERT_TRUE(Matches(Table, "Speed"));
    Table.ProbeRows(Rows);
    Nested = FindRow(Rows, "Nested");
    ASSERT_NE(Nested, nullptr);
    EXPECT_TRUE(Nested->bExpanded);
    const FPropertyTable::FRowProbe* Speed = FindRow(Rows, "Speed");
    const FPropertyTable::FRowProbe* Height = FindRow(Rows, "Height");
    ASSERT_NE(Speed, nullptr);
    ASSERT_NE(Height, nullptr);
    EXPECT_TRUE(Speed->bPassesFilter);
    EXPECT_FALSE(Height->bPassesFilter);
    Table.ClearSearch();
    Table.ProbeRows(Rows);
    Nested = FindRow(Rows, "Nested");
    ASSERT_NE(Nested, nullptr);
    EXPECT_FALSE(Nested->bExpanded);
    EXPECT_TRUE(FindRow(Rows, "Speed")->bPassesFilter);
    EXPECT_TRUE(FindRow(Rows, "Height")->bPassesFilter);
}

TEST_F(PropertySearchTest, RowsRebuiltDuringSearchRestoreTheirOwnInitialExpansion)
{
    FCompositeSearchData Data;
    FPropertyTable Table(&Data, MakeCompositeType(), nullptr);
    ASSERT_TRUE(Matches(Table, "Speed"));
    Table.MarkDirty();
    ASSERT_TRUE(Table.PrepareAndTestFilter());
    FRows Rows;
    Table.ProbeRows(Rows);
    const FPropertyTable::FRowProbe* Nested = FindRow(Rows, "Nested");
    ASSERT_NE(Nested, nullptr);
    EXPECT_TRUE(Nested->bExpanded);
    Table.ClearSearch();
    Table.ProbeRows(Rows);
    Nested = FindRow(Rows, "Nested");
    ASSERT_NE(Nested, nullptr);
    EXPECT_FALSE(Nested->bExpanded);
}

TEST_F(PropertySearchTest, ExternallyCreatedOptionalPayloadIsUpdatedBeforeFiltering)
{
    FCompositeSearchData Data;
    FPropertyTable Table(&Data, MakeCompositeType(), nullptr);
    ASSERT_TRUE(Matches(Table, "Optional"));
    FRows Rows;
    Table.ProbeRows(Rows);
    const FPropertyTable::FRowProbe* Optional = FindRow(Rows, "Optional");
    ASSERT_NE(Optional, nullptr);
    EXPECT_EQ(Optional->ChildCount, 0u);
    // A payload still arrives while the row it belongs to is the only one open state allows.
    ASSERT_TRUE(Table.SetRowExpanded("Optional", false));
    Data.Optional = 8.0f;
    ASSERT_TRUE(Table.PrepareAndTestFilter());
    Table.ProbeRows(Rows);
    Optional = FindRow(Rows, "Optional");
    ASSERT_NE(Optional, nullptr);
    ASSERT_EQ(Optional->ChildCount, 1u);
    EXPECT_TRUE(FindChild(Rows, *Optional)->bPassesFilter);
    EXPECT_EQ(*Data.Optional, 8.0f);
}

TEST_F(PropertySearchTest, PopulatedArrayStillIncludesItsNumberedEntries)
{
    Settings->RecentProjects.emplace_back("First.project");
    FPropertyTable Table(Settings);
    ASSERT_TRUE(Matches(Table, "Recent"));
    FRows Rows;
    Table.ProbeRows(Rows);
    const FPropertyTable::FRowProbe* Recent = FindRow(Rows, "RecentProjects");
    ASSERT_NE(Recent, nullptr);
    ASSERT_EQ(Recent->ChildCount, 1u);
    EXPECT_TRUE(FindChild(Rows, *Recent)->bPassesFilter);
    EXPECT_EQ(Settings->RecentProjects.size(), 1u);
    Table.ClearSearch();
    Table.ProbeRows(Rows);
    Recent = FindRow(Rows, "RecentProjects");
    ASSERT_NE(Recent, nullptr);
    EXPECT_TRUE(FindChild(Rows, *Recent)->bPassesFilter);
}

TEST_F(PropertySearchTest, LiteralPunctuationRemainsAnActiveOrdinaryQuery)
{
    FPropertyTable Table(Settings);
    for (const char* Query : {",", "-", "\"\""})
        EXPECT_FALSE(Matches(Table, Query)) << Query;
    EXPECT_TRUE(Matches(Table, "\t　"));
}
