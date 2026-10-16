#include <gtest/gtest.h>
#include "Localization/LocalizationFormat.h"

TEST(LocalizationFormat, SharedArgumentContracts)
{
#define FORMAT_CASE(Name, Source, Translation, Expected) \
    EXPECT_EQ(Lumina::Localization::HaveCompatibleFormatArguments(Source, Translation), Expected) << Name;
#include "LocalizationFormatCases.inc"
#undef FORMAT_CASE
}

TEST(LocalizationFormat, NonTerminatedViews)
{
    const char Source[] = "%llu trailing";
    const char Translation[] = "%ls trailing";
    EXPECT_FALSE(Lumina::Localization::HaveCompatibleFormatArguments(
        Lumina::FStringView(Source, 4), Lumina::FStringView(Translation, 3)));
}
