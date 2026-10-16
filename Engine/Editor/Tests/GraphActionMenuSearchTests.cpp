#include <gtest/gtest.h>
#include "UI/Tools/NodeGraph/GraphActionMenuSearch.h"
#include "Search/TextSearch.h"

namespace Lumina
{
    namespace
    {
        bool Match(std::string_view QueryText, std::string_view Display, std::string_view Source,
            std::string_view Category, std::string_view SourceCategory, std::string_view Key,
            int32& Score, TVector<uint16>& Matches)
        {
            const TextSearch::FQuery Query(QueryText);
            return FuzzyMatchAliases(Query, {{Display, 100, true}, {Source, 100}, {Key, 50},
                {Category}, {SourceCategory}}, Score, Matches);
        }
    }

    TEST(GraphActionMenuSearch, EnglishSourceFindsTranslatedNameWithoutFalseHighlight)
    {
        int32 Score = 0;
        TVector<uint16> Matches;
        ASSERT_TRUE(Match("create", "创建向量", "Create Vector", "数学", "Math", "CVectorNode", Score, Matches));
        EXPECT_GT(Score, 100);
        EXPECT_TRUE(Matches.empty());
    }

    TEST(GraphActionMenuSearch, MixedTermsCanMatchDifferentLanguagesAndFields)
    {
        int32 Score = 0;
        TVector<uint16> Matches;
        ASSERT_TRUE(Match("create 向量 Math", "创建向量", "Create Vector", "数学", "Math", "CVectorNode", Score, Matches));
        ASSERT_EQ(Matches.size(), 2u);
        EXPECT_EQ(Matches[0], 6u);
        EXPECT_EQ(Matches[1], 9u);
        const auto Spans = BuildGraphActionHighlightSpans("创建向量", Matches);
        ASSERT_EQ(Spans.size(), 2u);
        EXPECT_EQ(Spans[0].Begin, 0u);
        EXPECT_EQ(Spans[0].End, 6u);
        EXPECT_FALSE(Spans[0].bMatched);
        EXPECT_EQ(Spans[1].Begin, 6u);
        EXPECT_EQ(Spans[1].End, 12u);
        EXPECT_TRUE(Spans[1].bMatched);
    }

    TEST(GraphActionMenuSearch, TechnicalKeyAndReorderedTermsRemainSearchable)
    {
        int32 Score = 0;
        TVector<uint16> Matches;
        EXPECT_TRUE(Match("CVecNd 向量", "创建向量", "Create Vector", "数学", "Math", "CVectorNode", Score, Matches));
        EXPECT_TRUE(Match("vector create", "创建向量", "Create Vector", "数学", "Math", "CVectorNode", Score, Matches));
        EXPECT_TRUE(Matches.empty());
    }

    TEST(GraphActionMenuSearch, EveryTermMustMatchAndFailureClearsHighlights)
    {
        int32 Score = 99;
        TVector<uint16> Matches = {0, 3};
        EXPECT_FALSE(Match("创建 impossible", "创建向量", "Create Vector", "数学", "Math", "CVectorNode", Score, Matches));
        EXPECT_EQ(Score, 0);
        EXPECT_TRUE(Matches.empty());
    }

    TEST(GraphActionMenuSearch, SourceAliasOffsetsNeverIndexTranslatedDisplay)
    {
        int32 Score = 0;
        TVector<uint16> Matches;
        ASSERT_TRUE(Match("Beyond", "节点", "A very long original source label Beyond", "", "", "", Score, Matches));
        EXPECT_TRUE(Matches.empty());
        const auto Spans = BuildGraphActionHighlightSpans("节点", Matches);
        ASSERT_EQ(Spans.size(), 1u);
        EXPECT_FALSE(Spans[0].bMatched);
        EXPECT_EQ(Spans[0].End, 6u);
    }

    TEST(GraphActionMenuSearch, FuzzyPrefixAndWordBoundaryRankingIsPreserved)
    {
        int32 Prefix = 0, Subsequence = 0, AliasScore = 0;
        TVector<uint16> Matches;
        ASSERT_TRUE(FuzzyMatch("cv", "CreateVector", Prefix, Matches));
        ASSERT_TRUE(FuzzyMatch("cv", "uncategorized value", Subsequence, Matches));
        EXPECT_GT(Prefix, Subsequence);
        ASSERT_TRUE(Match("cv", "创建向量", "CreateVector", "", "", "", AliasScore, Matches));
        EXPECT_EQ(AliasScore, Prefix + 100);
        EXPECT_TRUE(Matches.empty());
    }

    TEST(GraphActionMenuSearch, UnicodeWhitespaceSplitsTermsAndAsciiCaseFolds)
    {
        int32 Score = 0;
        TVector<uint16> Matches;
        EXPECT_TRUE(Match("CREATE\u3000向量\tMATH", "创建向量", "Create Vector", "数学", "Math", "", Score, Matches));
    }

    TEST(GraphActionMenuSearch, DuplicateDisplayMatchesAreSortedAndDeduplicated)
    {
        int32 Score = 0;
        TVector<uint16> Matches;
        ASSERT_TRUE(Match("向量 向", "创建向量", "Create Vector", "", "", "", Score, Matches));
        ASSERT_EQ(Matches.size(), 2u);
        EXPECT_EQ(Matches[0], 6u);
        EXPECT_EQ(Matches[1], 9u);
    }

    TEST(GraphActionMenuSearch, EmptyWhitespaceQueryMatchesWithoutHighlights)
    {
        int32 Score = 99;
        TVector<uint16> Matches = {0};
        EXPECT_TRUE(Match(" \t\u3000", "创建", "Create", "", "", "", Score, Matches));
        EXPECT_EQ(Score, 0);
        EXPECT_TRUE(Matches.empty());
    }

    TEST(GraphActionMenuSearch, InvalidUtf8QueryCannotBecomeMatchAll)
    {
        int32 Score = 99;
        TVector<uint16> Matches = {0};
        const char Invalid[] = {'\xE5', '\x90', '\0'};
        EXPECT_FALSE(Match(Invalid, "创建", "Create", "", "", "", Score, Matches));
        EXPECT_EQ(Score, 0);
        EXPECT_TRUE(Matches.empty());
    }

    TEST(GraphActionMenuSearch, ChineseSubsequenceMatchesWholeCodepoints)
    {
        int32 Score = 0;
        TVector<uint16> Matches;
        EXPECT_FALSE(FuzzyMatch("名", "各外", Score, Matches));
        ASSERT_TRUE(FuzzyMatch("创建", "创A建", Score, Matches));
        ASSERT_EQ(Matches.size(), 2u);
        EXPECT_EQ(Matches[0], 0u);
        EXPECT_EQ(Matches[1], 4u);
    }
}
