#pragma once

#include "Containers/Vector.h"
#include <initializer_list>
#include <string_view>

namespace Lumina
{
    namespace TextSearch { class FQuery; }
    // UTF-8 codepoint subsequence match, with ASCII case folding and word-boundary bonuses.
    // Each match index is the byte offset of one complete character in Target.
    // Failure clears both outputs; matches beyond the uint16 byte-offset range are rejected.
    EDITOR_API bool FuzzyMatch(const char* Query, const char* Target, int32& OutScore, TVector<uint16>& OutMatchIndices);

    struct FFuzzySearchAlias
    {
        std::string_view Text;
        int32 ScoreBonus = 0;
        // Only the displayed name may contribute highlight byte offsets.
        bool bDisplayText = false;
    };

    // Every query term must match one alias. Terms may match different languages/fields.
    // Scores retain fuzzy subsequence/boundary bonuses; highlights never come from hidden aliases.
    EDITOR_API bool FuzzyMatchAliases(const TextSearch::FQuery& Query,
        std::initializer_list<FFuzzySearchAlias> Aliases, int32& OutScore, TVector<uint16>& OutDisplayMatches);

    struct FGraphActionTextSpan
    {
        size_t Begin;
        size_t End;
        bool bMatched;
    };

    using FGraphActionTextSpans = TVector<FGraphActionTextSpan, 16>;

    // Adjacent characters with the same highlight state share one [Begin, End) byte range.
    EDITOR_API FGraphActionTextSpans BuildGraphActionHighlightSpans(const char* Text, const TVector<uint16>& MatchIndices);
}
