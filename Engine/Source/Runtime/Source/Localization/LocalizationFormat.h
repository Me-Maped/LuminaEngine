#pragma once

#include "Containers/StringView.h"
#include "Platform/GenericPlatform.h"

namespace Lumina::Localization
{
    // Source is display text, excluding the table's optional context suffix. Source conversions
    // activate strict parsing; prose percentages and code braces remain literal. Invalid or changed
    // argument contracts fail closed. Numbered brace fields may be reordered, including their specs.
    RUNTIME_API bool HaveCompatibleFormatArguments(FStringView Source, FStringView Translation);
}
