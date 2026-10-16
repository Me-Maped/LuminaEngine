#include "TextSearch.h"
#include <cstdint>

namespace Lumina::TextSearch
{
    namespace
    {
        size_t SpaceLength(std::string_view Text, size_t Index)
        {
            const char C = Text[Index];
            if (C == ' ' || C == '\t' || C == '\r' || C == '\n' || C == '\v' || C == '\f') { return 1; }
            // Common pasted/IME separators: NBSP and full-width ideographic space.
            if (Text.substr(Index, 2) == "\xc2\xa0") { return 2; }
            if (Text.substr(Index, 3) == "\xe3\x80\x80") { return 3; }
            return 0;
        }
        unsigned char Fold(unsigned char C) { return C >= 'A' && C <= 'Z' ? C + ('a' - 'A') : C; }

        bool ValidUtf8(std::string_view Text)
        {
            for (size_t Index = 0; Index < Text.size();)
            {
                const auto First = static_cast<unsigned char>(Text[Index]);
                if (First < 0x80) { ++Index; continue; }
                const size_t Count = First >= 0xc2 && First <= 0xdf ? 2
                    : First >= 0xe0 && First <= 0xef ? 3 : First >= 0xf0 && First <= 0xf4 ? 4 : 0;
                if (Count == 0 || Count > Text.size() - Index) { return false; }
                uint32_t Value = First & (0x7fu >> Count);
                for (size_t Offset = 1; Offset < Count; ++Offset)
                {
                    const auto Byte = static_cast<unsigned char>(Text[Index + Offset]);
                    if ((Byte & 0xc0u) != 0x80u) { return false; }
                    Value = (Value << 6) | (Byte & 0x3fu);
                }
                const uint32_t Minimum = Count == 2 ? 0x80u : Count == 3 ? 0x800u : 0x10000u;
                if (Value < Minimum || Value > 0x10ffffu || (Value >= 0xd800u && Value <= 0xdfffu)) { return false; }
                Index += Count;
            }
            return true;
        }
    }

    bool ContainsIgnoreCase(std::string_view Text, std::string_view Needle)
    {
        if (!ValidUtf8(Text) || !ValidUtf8(Needle) || Needle.size() > Text.size()) { return false; }
        for (size_t Start = 0; Start <= Text.size() - Needle.size(); ++Start)
        {
            size_t Index = 0;
            while (Index < Needle.size() && Fold(Text[Start + Index]) == Fold(Needle[Index])) { ++Index; }
            if (Index == Needle.size()) { return true; }
        }
        return false;
    }

    FQuery::FQuery(std::string_view Text) { SetText(Text); }

    bool FQuery::SetText(std::string_view Text)
    {
        if (Query == Text) { return false; }
        if (Text.empty()) { Query.clear(); }
        else { Query.assign(Text.data(), Text.size()); }
        Terms.clear();
        bValid = ValidUtf8(Query);
        if (!bValid) { return true; }
        for (size_t Index = 0; Index < Query.size();)
        {
            while (Index < Query.size())
            {
                const size_t Space = SpaceLength(Query, Index);
                if (Space == 0) { break; }
                Index += Space;
            }
            const size_t Begin = Index;
            while (Index < Query.size() && SpaceLength(Query, Index) == 0) { ++Index; }
            if (Index > Begin) { Terms.push_back({Begin, Index - Begin}); }
        }
        return true;
    }
}
