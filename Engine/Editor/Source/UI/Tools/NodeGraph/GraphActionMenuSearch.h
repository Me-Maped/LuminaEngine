#pragma once

#include "Containers/Vector.h"

namespace Lumina
{
    // UTF-8 codepoint subsequence match, with ASCII case folding and word-boundary bonuses.
    // Each match index is the byte offset of one complete character in Target.
    // Failure clears both outputs; matches beyond the uint16 byte-offset range are rejected.
    EDITOR_API bool FuzzyMatch(const char* Query, const char* Target, int32& OutScore, TVector<uint16>& OutMatchIndices);

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
