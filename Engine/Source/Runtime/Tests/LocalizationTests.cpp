#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <atomic>
#include <thread>

#include "Localization/Localization.h"

using namespace Lumina;

namespace
{
    // A table small enough to spell out inline, so the suite never depends on a mounted file system.
    constexpr const char* kChineseTable = R"({
        "Save": "保存",
        "Save All": "全部保存",
        "Language": "语言",
        "Usage": "使用量",
        "Usage|texture": "用途"
    })";
}

TEST(Localization, MissingKeyFallsBackToSource)
{
    FLocalization& Localization = FLocalization::Get();
    Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable);

    // A string with no entry must stay readable rather than going blank.
    EXPECT_STREQ(Localization.Translate("Something Nobody Translated"), "Something Nobody Translated");

    Localization.ClearTable();
}

TEST(Localization, TranslatesFromTheActiveTable)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    EXPECT_STREQ(Localization.Translate("Save"), "保存");
    EXPECT_STREQ(Localization.Translate("Save All"), "全部保存");

    Localization.ClearTable();
}

TEST(Localization, SwitchingLanguageSwapsLookups)
{
    FLocalization& Localization = FLocalization::Get();

    Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable);
    EXPECT_STREQ(Localization.Translate("Save"), "保存");

    // English ships no table; every lookup falls back to the source it was keyed by.
    Localization.SetLanguageFromString(ELanguage::English, "{}");
    EXPECT_STREQ(Localization.Translate("Save"), "Save");

    Localization.ClearTable();
}

TEST(Localization, EmptyTableLeavesSourceStringsIntact)
{
    FLocalization& Localization = FLocalization::Get();
    Localization.ClearTable();

    EXPECT_STREQ(Localization.Translate("Language"), "Language");
}

TEST(Localization, MalformedTableIsRejected)
{
    FLocalization& Localization = FLocalization::Get();

    EXPECT_FALSE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, "{ this is not json"));

    // The failed load must not leave a half-built table behind.
    EXPECT_STREQ(Localization.Translate("Save"), "Save");

    Localization.ClearTable();
}

TEST(Localization, NonStringValuesAreSkipped)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified,
        R"({ "Save": "保存", "Nested": { "Ignored": true }, "Count": 3 })"));

    EXPECT_STREQ(Localization.Translate("Save"), "保存");
    EXPECT_STREQ(Localization.Translate("Nested"), "Nested");
    EXPECT_STREQ(Localization.Translate("Count"), "Count");

    Localization.ClearTable();
}

TEST(Localization, IconTextCombinesIconAndTranslation)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // The menus are all "<icon> <text>"; the icon must survive translation and the lookup must be
    // driven by the text alone.
    EXPECT_STREQ(Localization::IconText("ICON", "Save"), "ICON 保存");
    EXPECT_STREQ(Localization::IconText("ICON", "Never Translated"), "ICON Never Translated");

    Localization.ClearTable();
}

TEST(Localization, TranslateViewHandlesASourceThatIsNotTerminated)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // LOC_VIEW exists for a source that arrives as a view, so the lookup must never assume the view is
    // null-terminated -- a longer buffer with the name at the front is the shape a viewer would get.
    const char Buffer[] = "Save All~~~";
    const FStringView NotTerminated(Buffer, 8);

    EXPECT_EQ(Localization.TranslateView(NotTerminated), FStringView("全部保存"));
    EXPECT_EQ(Localization.TranslateView(FStringView("Nothing Here")), FStringView("Nothing Here"));

    Localization.ClearTable();
}

TEST(Localization, TranslateTemporaryRoundTripsThroughItsRotatingBuffer)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // The temporary form hands back a terminated pointer into a rotating buffer, so several can be held
    // at once -- a single ImGui row often draws two of them in the same expression.
    const char* First  = Localization::TranslateTemporary(FStringView("Save"));
    const char* Second = Localization::TranslateTemporary(FStringView("Save All"));

    EXPECT_STREQ(First, "保存");
    EXPECT_STREQ(Second, "全部保存");

    Localization.ClearTable();
}

TEST(Localization, StoredTextSurvivesPastTheDrawnLabelBuffer)
{
    FLocalization& Localization = FLocalization::Get();

    // A node tooltip is several hundred characters and is stored on the action rather than drawn on
    // the spot, so GraphActionMenu reads it through TranslateView and copies the result out. That is
    // the only form that can carry an entry this long: the drawn-label family (LOC_VIEW) writes
    // through a fixed rotating buffer sized for a label, which is why a tooltip does not use it.
    const std::string Long(700, 'x');
    const std::string Table = R"({ "A Long Tooltip": ")" + Long + R"(" })";
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, Table.c_str()));

    const FStringView Stored = Localization.TranslateView(FStringView("A Long Tooltip"));
    EXPECT_EQ(Stored.size(), Long.size());
    EXPECT_EQ(Stored, FStringView(Long.data(), Long.size()));

    // The drawn-label form is capped, which is exactly why the tooltip site does not reach for it.
    EXPECT_LT(std::strlen(::Lumina::Localization::TranslateTemporary(FStringView("A Long Tooltip"))),
              Long.size());

    Localization.ClearTable();
}

TEST(Localization, ContextualEntryWinsOverThePlainOne)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // The whole point of a context: the plain word keeps one reading while a domain that reads it
    // differently gets its own, without the call site having to spell out a different key.
    EXPECT_STREQ(Localization.Translate("Usage"), "使用量");
    EXPECT_STREQ(Localization.Translate("Usage", "texture"), "用途");
    EXPECT_EQ(Localization.TranslateView(FStringView("Usage"), FStringView("texture")), FStringView("用途"));
    EXPECT_STREQ(Localization::TranslateTemporary(FStringView("Usage"), FStringView("texture")), "用途");

    Localization.ClearTable();
}

TEST(Localization, MissingContextFallsBackToTheSourceThenThePlainEntry)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // A context nobody has translated must not blank the label: it falls back to the plain entry,
    // then to the source, and an empty context is simply the plain lookup.
    EXPECT_STREQ(Localization.Translate("Usage", "geometry"), "使用量");
    EXPECT_STREQ(Localization.Translate("Usage", ""), "使用量");
    EXPECT_STREQ(Localization.Translate("Save", "texture"), "保存");
    EXPECT_STREQ(Localization.Translate("Nothing Here", "texture"), "Nothing Here");

    Localization.ClearTable();
}

TEST(Localization, ContextualLookupAcceptsANonTerminatedSource)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // The contextual lookup is reached from a pin name, which is a view into a longer string, so the
    // key has to be built from the view's length rather than from a scan for a terminator.
    const char Buffer[] = "Usage~~~";
    const FStringView NotTerminated(Buffer, 5);

    EXPECT_STREQ(Localization::TranslateTemporary(NotTerminated, FStringView("texture")), "用途");
    EXPECT_STREQ(Localization::IconText("ICON", "Usage", "texture"), "ICON 用途");
    EXPECT_STREQ(Localization::IconText("ICON", "Usage"), "ICON 使用量");

    Localization.ClearTable();
}

TEST(Localization, LocalizedIdTranslatesTheLabelAndKeepsTheSourceAsIdentity)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // The visible half is translated while the half ImGui hashes stays the English source, which is
    // what lets a table column or a collapsing header survive a language switch. An untranslated
    // source still round-trips, so the form is safe to use on any label.
    EXPECT_STREQ(Localization::LocalizedId(FStringView("Save")), "保存###Save");
    EXPECT_STREQ(Localization::LocalizedId(FStringView("Nobody Translated This")),
                 "Nobody Translated This###Nobody Translated This");
    EXPECT_STREQ(Localization::LocalizedId(FStringView("Usage"), FStringView("texture")), "用途###Usage");

    Localization.ClearTable();
}

TEST(Localization, LocalizedIdGivesUpTheVisibleHalfBeforeTheIdentity)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // A label long enough to overflow the buffer has to keep its identity half, because that is what
    // ImGui's stored state is keyed on -- dropping any of it would change the widget's ID.
    const std::string Long(4000, 'x');
    const std::string Result(Localization::LocalizedId(FStringView(Long.c_str(), Long.size())));

    const size_t Separator = Result.find("###");
    ASSERT_NE(Separator, std::string::npos);

    const size_t Identity = Result.size() - Separator - 3;
    EXPECT_GT(Identity, 0u);
    EXPECT_EQ(Result.compare(Separator + 3, Identity, Long, 0, Identity), 0);

    Localization.ClearTable();
}

TEST(Localization, FileDialogFilterTranslatesDisplayHalvesAndLeavesPatterns)
{
    FLocalization& Localization = FLocalization::Get();
    ASSERT_TRUE(Localization.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));

    // A Win32 filter is one string of NUL-separated display/pattern pairs, so the scan has to be
    // driven by those terminators rather than by a strlen, and the patterns are data that has to come
    // through byte for byte -- the two are indistinguishable to a translator.
    const FString Filter = Localization::LocalizeFileDialogFilter("Save\0*.sav\0Usage\0*.use\0");

    const char Expected[] = "保存\0*.sav\0使用量\0*.use\0\0";
    ASSERT_EQ(Filter.size(), sizeof(Expected) - 1);
    EXPECT_EQ(std::memcmp(Filter.c_str(), Expected, sizeof(Expected) - 1), 0);

    // An untranslated display name comes back unchanged, and no filter at all stays empty rather than
    // turning into a run of bare terminators that the dialog would read as a filter list.
    const FString Untranslated = Localization::LocalizeFileDialogFilter("Nobody's Files\0*.x\0");
    const char ExpectedUntranslated[] = "Nobody's Files\0*.x\0\0";
    ASSERT_EQ(Untranslated.size(), sizeof(ExpectedUntranslated) - 1);
    EXPECT_EQ(std::memcmp(Untranslated.c_str(), ExpectedUntranslated, sizeof(ExpectedUntranslated) - 1), 0);

    EXPECT_TRUE(Localization::LocalizeFileDialogFilter(nullptr).empty());

    Localization.ClearTable();
}

TEST(Localization, LanguageMetadataIsStable)
{
    // Every entry has to round-trip, since a picker drives the switch through this pair.
    ASSERT_EQ(FLocalization::GetLanguageCount(), 2u);

    for (size_t Index = 0; Index < FLocalization::GetLanguageCount(); ++Index)
    {
        const ELanguage Language = FLocalization::GetLanguageByIndex(Index);
        EXPECT_STRNE(FLocalization::GetLanguageCode(Language), "");
        EXPECT_STRNE(FLocalization::GetLanguageDisplayName(Language), "");
    }

    EXPECT_STREQ(FLocalization::GetLanguageCode(ELanguage::ChineseSimplified), "zh-Hans");
    EXPECT_STREQ(FLocalization::GetLanguageCode(ELanguage::English), "en");
}

TEST(Localization, ReadSnapshotKeepsPointersAndContextAfterPublication)
{
    auto& Service = FLocalization::Get();
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));
    const auto Reader = Service.Read();
    const char* Saved = Reader.Translate("Save");
    const FStringView Contextual = Reader.TranslateView("Usage", "texture");

    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::English, "{}"));
    Service.ClearTable();
    EXPECT_STREQ(Saved, "保存");
    EXPECT_EQ(Contextual, FStringView("用途"));
    EXPECT_EQ(Reader.GetLanguage(), ELanguage::ChineseSimplified);
    EXPECT_STREQ(Service.Read().Translate("Save"), "Save");
}

TEST(Localization, SnapshotIdentityChangesOnSameLanguageReload)
{
    auto& Service = FLocalization::Get();
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));
    const auto Before = Service.Read();
    EXPECT_TRUE(Before.HasSameTable(Service.Read()));
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::ChineseSimplified, R"({"Save":"另存"})"));
    const auto After = Service.Read();
    EXPECT_FALSE(Before.HasSameTable(After));
    EXPECT_STREQ(Before.Translate("Save"), "保存");
    EXPECT_STREQ(After.Translate("Save"), "另存");
    Service.ClearTable();
}

TEST(Localization, SearchCacheSnapshotFollowsThePinnedFrame)
{
    auto& Service = FLocalization::Get();
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));
    const auto Before = Service.Read();
    {
        FLocalizationFrameScope Frame;
        ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::English, "{}"));
        EXPECT_TRUE(Service.ReadForCurrentFrame().HasSameTable(Before));
        EXPECT_STREQ(Service.ReadForCurrentFrame().Translate("Save"), LOC("Save"));
        EXPECT_FALSE(Service.Read().HasSameTable(Before));
    }
    EXPECT_TRUE(Service.ReadForCurrentFrame().HasSameTable(Service.Read()));
    Service.ClearTable();
}

TEST(Localization, FrameSwitchesOnlyAfterItsSnapshotIsReleased)
{
    auto& Service = FLocalization::Get();
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));
    {
        FLocalizationFrameScope Frame;
        const char* Saved = LOC("Save");
        ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::English, "{}"));
        EXPECT_STREQ(LOC("Save All"), "全部保存");
        EXPECT_STREQ(Saved, "保存");
    }
    {
        FLocalizationFrameScope Frame;
        EXPECT_STREQ(LOC("Save"), "Save");
    }
    Service.ClearTable();
}

TEST(Localization, ExplicitSnapshotCanMoveAcrossThreads)
{
    auto& Service = FLocalization::Get();
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));
    auto Reader = Service.Read();
    const char* Saved = Reader.Translate("Save");
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::English, "{}"));
    std::thread Worker([Reader = std::move(Reader), Saved]()
    {
        EXPECT_STREQ(Saved, "保存");
        EXPECT_STREQ(Reader.Translate("Usage", "texture"), "用途");
    });
    Worker.join();
    Service.ClearTable();
}

TEST(Localization, ConcurrentReadersObserveWholeTablesDuringReload)
{
    auto& Service = FLocalization::Get();
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable));
    std::atomic<bool> Start{false};
    std::atomic<bool> Consistent{true};
    std::thread Readers[4];
    for (auto& Reader : Readers)
    {
        Reader = std::thread([&]()
        {
            while (!Start.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            for (int Iteration = 0; Iteration < 3000; ++Iteration)
            {
                const auto Snapshot = Service.Read();
                const char* Save = Snapshot.Translate("Save");
                std::this_thread::yield();
                const char* Usage = Snapshot.Translate("Usage", "texture");
                const bool Chinese = Snapshot.GetLanguage() == ELanguage::ChineseSimplified;
                if (std::strcmp(Save, Chinese ? "保存" : "Save") != 0
                    || std::strcmp(Usage, Chinese ? "用途" : "Usage") != 0)
                {
                    Consistent.store(false, std::memory_order_relaxed);
                }
            }
        });
    }
    Start.store(true, std::memory_order_release);
    for (int Iteration = 0; Iteration < 1000; ++Iteration)
    {
        Service.SetLanguageFromString(ELanguage::English, "{}");
        Service.SetLanguageFromString(ELanguage::ChineseSimplified, kChineseTable);
    }
    for (auto& Reader : Readers) { Reader.join(); }
    EXPECT_TRUE(Consistent.load());
    Service.ClearTable();
}

TEST(Localization, IncompatibleTranslationFallsBackWithoutDiscardingValidEntries)
{
    auto& Service = FLocalization::Get();
    ASSERT_TRUE(Service.SetLanguageFromString(ELanguage::ChineseSimplified,
        R"({"%llu Elements":"%ls 个元素", "{:.2f}":"{:.2s}", "Save":"保存"})"));
    const auto Reader = Service.Read();
    EXPECT_STREQ(Reader.Translate("%llu Elements"), "%llu Elements");
    EXPECT_STREQ(Reader.Translate("{:.2f}"), "{:.2f}");
    EXPECT_STREQ(Reader.Translate("Save"), "保存");
    Service.ClearTable();
}
