#include "GraphActionMenuSearch.h"

#include <cstring>
#include <limits>

namespace Lumina
{
    namespace
    {
        constexpr uint32 InvalidCodepoint = 0xFFFFFFFFu;

        struct FUtf8Character
        {
            uint32 Value = InvalidCodepoint;
            size_t ByteCount = 1;
        };

        // Invalid input consumes one byte, so callers always advance without reading past End.
        FUtf8Character DecodeCharacter(const char* Text, size_t Remaining)
        {
            const uint8 First = static_cast<uint8>(Text[0]);
            if (First < 0x80)
            {
                return {First, 1};
            }

            const size_t Count = First >= 0xC2 && First <= 0xDF ? 2
                : First >= 0xE0 && First <= 0xEF ? 3
                : First >= 0xF0 && First <= 0xF4 ? 4 : 0;
            if (Count == 0 || Remaining < Count)
            {
                return {};
            }

            uint32 Value = First & (0x7Fu >> Count);
            for (size_t Index = 1; Index < Count; ++Index)
            {
                const uint8 Byte = static_cast<uint8>(Text[Index]);
                if ((Byte & 0xC0u) != 0x80u)
                {
                    return {};
                }
                Value = (Value << 6) | (Byte & 0x3Fu);
            }

            const uint32 Minimum = Count == 2 ? 0x80u : Count == 3 ? 0x800u : 0x10000u;
            if (Value < Minimum || Value > 0x10FFFFu || (Value >= 0xD800u && Value <= 0xDFFFu))
            {
                return {};
            }
            return {Value, Count};
        }

        FORCEINLINE uint32 ToLowerCharacter(uint32 C)
        {
            return C >= 'A' && C <= 'Z' ? C + ('a' - 'A') : C;
        }

        FORCEINLINE bool IsWordSeparator(uint32 C)
        {
            return C == ' ' || C == '_' || C == '-' || C == '.' || C == '/';
        }

        FORCEINLINE bool IsLower(uint32 C) { return C >= 'a' && C <= 'z'; }
        FORCEINLINE bool IsUpper(uint32 C) { return C >= 'A' && C <= 'Z'; }
    }

    bool FuzzyMatch(const char* Query, const char* Target, int32& OutScore, TVector<uint16>& OutMatchIndices)
    {
        OutScore = 0;
        OutMatchIndices.clear();

        if (Query == nullptr || Query[0] == '\0')
        {
            return true;
        }
        if (Target == nullptr)
        {
            return false;
        }

        const int32 StartBonus        = 50;
        const int32 PrefixBonus       = 10;
        const int32 SeparatorBonus    = 30;
        const int32 CamelCaseBonus    = 30;
        const int32 ConsecutiveBonus  = 20;
        const int32 UnmatchedLeading  = -3;
        const int32 UnmatchedPenalty  = -1;

        int32 Score = 0;
        const size_t QueryLength = strlen(Query);
        const size_t TargetLength = strlen(Target);
        size_t QueryIdx = 0;
        size_t TargetIdx = 0;
        int32 CharacterIndex = 0;
        uint32 PreviousCharacter = InvalidCodepoint;
        bool bPrevMatched = false;
        bool bMatchedAny = false;

        while (TargetIdx < TargetLength && QueryIdx < QueryLength
            && TargetIdx <= std::numeric_limits<uint16>::max())
        {
            const FUtf8Character QC = DecodeCharacter(Query + QueryIdx, QueryLength - QueryIdx);
            const FUtf8Character TC = DecodeCharacter(Target + TargetIdx, TargetLength - TargetIdx);
            if (QC.Value == InvalidCodepoint)
            {
                break;
            }

            if (TC.Value != InvalidCodepoint && ToLowerCharacter(QC.Value) == ToLowerCharacter(TC.Value))
            {
                OutMatchIndices.push_back(static_cast<uint16>(TargetIdx));

                if (CharacterIndex == 0)
                {
                    Score += StartBonus;
                }
                else
                {
                    if (IsWordSeparator(PreviousCharacter))
                    {
                        Score += SeparatorBonus;
                    }
                    else if (IsLower(PreviousCharacter) && IsUpper(TC.Value))
                    {
                        Score += CamelCaseBonus;
                    }
                }

                if (bPrevMatched)
                {
                    Score += ConsecutiveBonus;
                }
                else if (!bMatchedAny && CharacterIndex > 0)
                {
                    Score += CharacterIndex * UnmatchedLeading;
                }

                if (QueryIdx == 0 && CharacterIndex == 0)
                {
                    Score += PrefixBonus;
                }

                QueryIdx += QC.ByteCount;
                bPrevMatched = true;
                bMatchedAny = true;
            }
            else
            {
                if (bMatchedAny)
                {
                    Score += UnmatchedPenalty;
                }
                bPrevMatched = false;
            }

            PreviousCharacter = TC.Value;
            TargetIdx += TC.ByteCount;
            ++CharacterIndex;
        }

        if (Query[QueryIdx] != '\0')
        {
            OutMatchIndices.clear();
            OutScore = 0;
            return false;
        }

        OutScore = Score;
        return true;
    }

    FGraphActionTextSpans BuildGraphActionHighlightSpans(const char* Text, const TVector<uint16>& MatchIndices)
    {
        FGraphActionTextSpans Spans;
        if (Text == nullptr)
        {
            return Spans;
        }
        const size_t Length = strlen(Text);
        size_t Cursor = 0;
        size_t MatchIdx = 0;

        while (Cursor < Length)
        {
            const FUtf8Character Character = DecodeCharacter(Text + Cursor, Length - Cursor);
            while (MatchIdx < MatchIndices.size() && MatchIndices[MatchIdx] < Cursor)
            {
                ++MatchIdx;
            }
            const bool bInMatch = Character.Value != InvalidCodepoint
                && MatchIdx < MatchIndices.size() && MatchIndices[MatchIdx] == Cursor;
            const size_t SpanEnd = Cursor + Character.ByteCount;

            if (!Spans.empty() && Spans.back().bMatched == bInMatch)
            {
                Spans.back().End = SpanEnd;
            }
            else
            {
                Spans.push_back({Cursor, SpanEnd, bInMatch});
            }
            Cursor = SpanEnd;
        }
        return Spans;
    }
}
