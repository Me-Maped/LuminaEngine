#include <gtest/gtest.h>
#include "Search/TextSearch.h"

using Lumina::TextSearch::FQuery;
using Lumina::TextSearch::ContainsIgnoreCase;

TEST(TextSearch, EnglishSourceAndLocalizedDisplayFindTheSameTarget)
{
    EXPECT_TRUE(FQuery("MOVEMENT speed").Matches({"移动速度", "Movement Speed", "MovementSpeed"}));
    EXPECT_TRUE(FQuery("移动速度").Matches({"移动速度", "Movement Speed"}));
    EXPECT_FALSE(FQuery("Rotation").Matches({"移动速度", "Movement Speed"}));
}

TEST(TextSearch, EveryTermMayMatchADifferentAlias)
{
    EXPECT_TRUE(FQuery("移动 SPEED").Matches({"移动速度", "Movement Speed"}));
    EXPECT_TRUE(FQuery("camera 速度").Matches({"相机速度", "Camera Speed"}));
    EXPECT_FALSE(FQuery("camera 重力").Matches({"相机速度", "Camera Speed"}));
}

TEST(TextSearch, EmptyQueriesAndFieldsAreWellDefined)
{
    EXPECT_TRUE(FQuery().Matches({}));
    EXPECT_TRUE(FQuery(" \t \r\n ").Matches({"任何内容"}));
    EXPECT_FALSE(FQuery("camera").Matches({}));
    EXPECT_FALSE(FQuery("camera").Matches({"", ""}));
}

TEST(TextSearch, MatchingHonorsViewBounds)
{
    const char Text[] = "speedHidden";
    EXPECT_TRUE(ContainsIgnoreCase(std::string_view(Text, 5), "SPEED"));
    EXPECT_FALSE(ContainsIgnoreCase(std::string_view(Text, 5), "Hidden"));
    EXPECT_FALSE(ContainsIgnoreCase(std::string_view(Text, 2), "speed"));
}

TEST(TextSearch, Utf8CharactersCannotMatchPartialBytes)
{
    EXPECT_TRUE(ContainsIgnoreCase("场景相机速度", "相机"));
    EXPECT_FALSE(ContainsIgnoreCase("相机", std::string_view("\x9c\xba", 2)));
    EXPECT_FALSE(FQuery(std::string_view("\xe7\x9b", 2)).Matches({"相机"}));
    EXPECT_FALSE(ContainsIgnoreCase(std::string_view("speed\xff", 6), "speed"));
    EXPECT_FALSE(ContainsIgnoreCase("camera", std::string_view("\xc0\xaf", 2)));
    EXPECT_TRUE(ContainsIgnoreCase("🌍 World", "🌍 world"));
}

TEST(TextSearch, QueryOwnsItsBytesAndCopyOrMoveRetainsTerms)
{
    std::string Input = "移动 speed";
    FQuery Original(Input);
    Input = "discarded";
    FQuery Copy = Original;
    FQuery Moved = std::move(Copy);
    EXPECT_TRUE(Original.Matches({"移动速度", "Movement Speed"}));
    EXPECT_TRUE(Moved.Matches({"移动速度", "Movement Speed"}));
    EXPECT_FALSE(Original.SetText("移动 speed"));
    EXPECT_TRUE(Original.SetText("rotation"));
    EXPECT_FALSE(Original.Matches({"移动速度", "Movement Speed"}));
    EXPECT_TRUE(Original.Matches({"旋转", "Rotation"}));
}

TEST(TextSearch, OrdinaryModeKeepsLiteralPunctuation)
{
    EXPECT_TRUE(FQuery("-debug").Matches({"run -debug"}));
    EXPECT_FALSE(FQuery("-debug").Matches({"release"}));
    EXPECT_TRUE(FQuery("a,b").Matches({"a,b"}));
    EXPECT_FALSE(FQuery("a,b").Matches({"a", "b"}));
}

TEST(TextSearch, FullWidthAndNonbreakingSpacesSeparateMixedTerms)
{
    EXPECT_TRUE(FQuery("移动　SPEED").Matches({"移动速度", "Movement Speed"}));
    EXPECT_TRUE(FQuery("camera\xc2\xa0速度").Matches({"相机速度", "Camera Speed"}));
    EXPECT_TRUE(FQuery("　\xc2\xa0\t").Matches({}));
}

TEST(TextSearch, IndividualTermsCannotCrossFieldBoundaries)
{
    EXPECT_FALSE(FQuery("cameraspeed").Matches({"camera", "speed"}));
    EXPECT_TRUE(FQuery("camera speed").Matches({"camera", "speed"}));
}
