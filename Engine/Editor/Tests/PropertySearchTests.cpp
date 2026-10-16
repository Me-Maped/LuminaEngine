#include <gtest/gtest.h>

#include "Config/EngineSettings.h"
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
    // Inspect existing protected row state without constructing a fake row or adding a production API.
    struct FSearchRowAccess : FPropertyRow
    {
        static const auto& ChildrenOf(FPropertyRow& Row)
        {
            const auto Member = &FSearchRowAccess::Children;
            return Row.*Member;
        }
        static bool& Expanded(FPropertyRow& Row)
        {
            const auto Member = &FSearchRowAccess::bExpanded;
            return Row.*Member;
        }
    };

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

        static FPropertyRow* FindRow(FPropertyTable& Table, FStringView Name)
        {
            auto* Category = Table.FindOrCreateCategoryRow(FName("General"));
            for (const auto& Row : FSearchRowAccess::ChildrenOf(*Category))
                if (Row->GetFilterInternalName() == Name) return Row.get();
            return nullptr;
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
    FPropertyRow* Scores = FindRow(Table, "Scores");
    FPropertyRow* Unrelated = FindRow(Table, "Unrelated");
    ASSERT_NE(Scores, nullptr);
    ASSERT_NE(Unrelated, nullptr);
    FSearchRowAccess::Expanded(*Scores) = false;

    ASSERT_TRUE(Matches(Table, "Scores"));
    EXPECT_FALSE(FSearchRowAccess::Expanded(*Scores));
    ASSERT_EQ(FSearchRowAccess::ChildrenOf(*Scores).size(), 1u);
    EXPECT_TRUE(FSearchRowAccess::ChildrenOf(*Scores)[0]->PassesFilter());
    EXPECT_FALSE(Unrelated->PassesFilter());
    EXPECT_FALSE(FSearchRowAccess::ChildrenOf(*Unrelated)[0]->PassesFilter());

    FSearchRowAccess::Expanded(*Scores) = true; // Opening a matching header must reveal the entry.
    ASSERT_TRUE(Matches(Table, "Scores"));
    EXPECT_TRUE(FSearchRowAccess::Expanded(*Scores));
    EXPECT_TRUE(FSearchRowAccess::ChildrenOf(*Scores)[0]->PassesFilter());
    Table.ClearSearch();
    EXPECT_FALSE(FSearchRowAccess::Expanded(*Scores));
    EXPECT_TRUE(FSearchRowAccess::ChildrenOf(*Unrelated)[0]->PassesFilter());
    EXPECT_EQ(Data.Scores.at(1), 2.0f);
}

TEST_F(PropertySearchTest, MatchingStructShowsContentButLeafSearchStillFiltersOtherMembers)
{
    FCompositeSearchData Data;
    FPropertyTable Table(&Data, MakeCompositeType(), nullptr);
    ASSERT_TRUE(Matches(Table, "Nested"));
    FPropertyRow* Nested = FindRow(Table, "Nested");
    ASSERT_NE(Nested, nullptr);
    EXPECT_FALSE(FSearchRowAccess::Expanded(*Nested));
    const auto& Children = FSearchRowAccess::ChildrenOf(*Nested);
    ASSERT_EQ(Children.size(), 2u);
    EXPECT_TRUE(Children[0]->PassesFilter());
    EXPECT_TRUE(Children[1]->PassesFilter());

    ASSERT_TRUE(Matches(Table, "Speed"));
    EXPECT_TRUE(FSearchRowAccess::Expanded(*Nested));
    EXPECT_TRUE(Children[0]->PassesFilter());
    EXPECT_FALSE(Children[1]->PassesFilter());
    Table.ClearSearch();
    EXPECT_FALSE(FSearchRowAccess::Expanded(*Nested));
    EXPECT_TRUE(Children[0]->PassesFilter());
    EXPECT_TRUE(Children[1]->PassesFilter());
}

TEST_F(PropertySearchTest, RowsRebuiltDuringSearchRestoreTheirOwnInitialExpansion)
{
    FCompositeSearchData Data;
    FPropertyTable Table(&Data, MakeCompositeType(), nullptr);
    ASSERT_TRUE(Matches(Table, "Speed"));
    Table.MarkDirty();
    ASSERT_TRUE(Table.PrepareAndTestFilter());
    FPropertyRow* Nested = FindRow(Table, "Nested");
    ASSERT_NE(Nested, nullptr);
    EXPECT_TRUE(FSearchRowAccess::Expanded(*Nested));
    Table.ClearSearch();
    EXPECT_FALSE(FSearchRowAccess::Expanded(*Nested));
}

TEST_F(PropertySearchTest, ExternallyCreatedOptionalPayloadIsUpdatedBeforeFiltering)
{
    FCompositeSearchData Data;
    FPropertyTable Table(&Data, MakeCompositeType(), nullptr);
    ASSERT_TRUE(Matches(Table, "Optional"));
    FPropertyRow* Optional = FindRow(Table, "Optional");
    ASSERT_NE(Optional, nullptr);
    EXPECT_TRUE(FSearchRowAccess::ChildrenOf(*Optional).empty());
    FSearchRowAccess::Expanded(*Table.FindOrCreateCategoryRow(FName("General"))) = false;
    Data.Optional = 8.0f;
    ASSERT_TRUE(Table.PrepareAndTestFilter());
    ASSERT_EQ(FSearchRowAccess::ChildrenOf(*Optional).size(), 1u);
    EXPECT_TRUE(FSearchRowAccess::ChildrenOf(*Optional)[0]->PassesFilter());
    EXPECT_EQ(*Data.Optional, 8.0f);
}

TEST_F(PropertySearchTest, PopulatedArrayStillIncludesItsNumberedEntries)
{
    Settings->RecentProjects.emplace_back("First.project");
    FPropertyTable Table(Settings);
    ASSERT_TRUE(Matches(Table, "Recent"));
    FPropertyRow* Recent = nullptr;
    auto* Category = Table.FindOrCreateCategoryRow(FName("Loading"));
    for (const auto& Row : FSearchRowAccess::ChildrenOf(*Category))
        if (Row->GetFilterInternalName() == "RecentProjects") Recent = Row.get();
    ASSERT_NE(Recent, nullptr);
    ASSERT_EQ(FSearchRowAccess::ChildrenOf(*Recent).size(), 1u);
    EXPECT_TRUE(FSearchRowAccess::ChildrenOf(*Recent)[0]->PassesFilter());
    EXPECT_EQ(Settings->RecentProjects.size(), 1u);
    Table.ClearSearch();
    EXPECT_TRUE(FSearchRowAccess::ChildrenOf(*Recent)[0]->PassesFilter());
}

TEST_F(PropertySearchTest, LiteralPunctuationRemainsAnActiveOrdinaryQuery)
{
    FPropertyTable Table(Settings);
    for (const char* Query : {",", "-", "\"\""})
        EXPECT_FALSE(Matches(Table, Query)) << Query;
    EXPECT_TRUE(Matches(Table, "\t　"));
}
