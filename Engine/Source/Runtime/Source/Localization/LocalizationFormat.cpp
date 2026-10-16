#include "LocalizationFormat.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace Lumina::Localization
{
    namespace
    {
        // Keep in step with BuildScripts/LocalizationFormat.py; both consume the shared test fixture.
        bool IsDigit(char C) { return C >= '0' && C <= '9'; }
        bool IsLetter(char C) { return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z'); }
        bool Contains(std::string_view Set, char C) { return Set.find(C) != std::string_view::npos; }

        struct FPrintfSignature
        {
            bool Valid = true;
            std::vector<std::string> Arguments;
            size_t Escapes = 0;
            bool Active() const { return !Arguments.empty() || Escapes != 0; }
        };

        FPrintfSignature ParsePrintf(std::string_view Text, bool NaturalSpace = false)
        {
            FPrintfSignature Result;
            for (size_t I = 0; I < Text.size();)
            {
                if (Text[I] != '%') { ++I; continue; }
                if (I + 1 < Text.size() && Text[I + 1] == '%')
                {
                    ++Result.Escapes;
                    I += 2;
                    continue;
                }
                const size_t Start = I++;
                bool Space = false;
                while (I < Text.size() && Contains("-+ #0'", Text[I]))
                {
                    Space |= Text[I] == ' ';
                    ++I;
                }
                const bool Width = I < Text.size() && Text[I] == '*';
                if (Width) { ++I; }
                else { while (I < Text.size() && IsDigit(Text[I])) { ++I; } }
                bool Precision = false;
                if (I < Text.size() && Text[I] == '.')
                {
                    ++I;
                    Precision = I < Text.size() && Text[I] == '*';
                    if (Precision) { ++I; }
                    else { while (I < Text.size() && IsDigit(Text[I])) { ++I; } }
                }
                std::string Length;
                if (I < Text.size() && Contains("hljztL", Text[I]))
                {
                    Length += Text[I++];
                    if (I < Text.size() && (Length == "h" || Length == "l") && Text[I] == Length[0])
                    {
                        Length += Text[I++];
                    }
                }
                bool Allowed = false;
                char Kind = '\0';
                if (I < Text.size())
                {
                    Kind = Text[I++];
                    if (Contains("diouxXn", Kind)) { Allowed = Length != "L"; }
                    else if (Contains("fFeEgGaA", Kind)) { Allowed = Length.empty() || Length == "l" || Length == "L"; }
                    else if (Contains("cs", Kind)) { Allowed = Length.empty() || Length == "l"; }
                    else if (Kind == 'p') { Allowed = Length.empty(); }
                }
                // A space flag followed by a word suffix is prose, e.g. "Utilization % is ...".
                const bool Prose = Space && (NaturalSpace || (I < Text.size() && IsLetter(Text[I])));
                if (!Allowed || Prose)
                {
                    Result.Valid = false;
                    I = Start + 1;
                    continue;
                }
                if (Width) { Result.Arguments.emplace_back("width:int"); }
                if (Precision) { Result.Arguments.emplace_back("precision:int"); }
                Result.Arguments.push_back(Length + Kind);
            }
            return Result;
        }

        using FField = std::tuple<uint32_t, std::string, int64_t, int64_t>;
        struct FBraceSignature
        {
            bool Valid = true;
            std::vector<FField> Fields;
            std::array<size_t, 2> Escapes{};
        };

        class FBraceParser
        {
        public:
            explicit FBraceParser(std::string_view InText) : Text(InText) {}

            FBraceSignature Parse()
            {
                while (I < Text.size() && Result.Valid)
                {
                    const char C = Text[I];
                    if (C != '{' && C != '}') { ++I; continue; }
                    if (I + 1 < Text.size() && Text[I + 1] == C)
                    {
                        ++Result.Escapes[C == '}' ? 1 : 0];
                        I += 2;
                        continue;
                    }
                    if (C == '}') { Fail(); break; }
                    ++I;
                    const uint32_t Arg = ReadIndex();
                    std::string Spec;
                    int64_t Width = -1, Precision = -1;
                    if (At(':'))
                    {
                        ++I;
                        const size_t Start = I;
                        if (I + 1 < Text.size() && Contains("<>^", Text[I + 1]) && !Contains("{}", Text[I])) { I += 2; }
                        else if (In("<>^")) { ++I; }
                        if (In("+- ")) { ++I; }
                        if (At('#')) { ++I; }
                        if (At('0')) { ++I; }
                        Spec = Text.substr(Start, I - Start);
                        if (At('{')) { Width = Dynamic(); Spec += '*'; }
                        else { Spec += Digits(); }
                        if (At('.'))
                        {
                            ++I;
                            Spec += '.';
                            if (At('{')) { Precision = Dynamic(); Spec += '*'; }
                            else
                            {
                                const std::string_view Value = Digits();
                                if (Value.empty()) { Fail(); }
                                Spec += Value;
                            }
                        }
                        if (At('L')) { ++I; Spec += 'L'; }
                        if (I < Text.size() && !At('}'))
                        {
                            if (!In("sbcBdoxXaAeEfFgGpP")) { Fail(); }
                            Spec += Text[I++];
                        }
                    }
                    if (!At('}')) { Fail(); break; }
                    ++I;
                    Result.Fields.emplace_back(Arg, Spec, Width, Precision);
                }
                if (!Result.Valid) { Result.Fields.clear(); }
                std::sort(Result.Fields.begin(), Result.Fields.end());
                return Result;
            }

        private:
            bool At(char C) const { return I < Text.size() && Text[I] == C; }
            bool In(std::string_view Set) const { return I < Text.size() && Contains(Set, Text[I]); }
            void Fail() { Result.Valid = false; }
            std::string_view Digits()
            {
                const size_t Start = I;
                while (I < Text.size() && IsDigit(Text[I])) { ++I; }
                return Text.substr(Start, I - Start);
            }
            uint32_t ReadIndex()
            {
                const std::string_view Value = Digits();
                const int NewMode = Value.empty() ? 1 : 2;
                if (Mode != 0 && Mode != NewMode) { Fail(); }
                Mode = NewMode;
                if (Value.empty()) { return Automatic++; }
                uint64_t Number = 0;
                for (char C : Value)
                {
                    Number = Number * 10 + static_cast<unsigned>(C - '0');
                    if (Number > 0xFFFFFFFFULL) { Fail(); return 0; }
                }
                return static_cast<uint32_t>(Number);
            }
            int64_t Dynamic()
            {
                ++I;
                const uint32_t Value = ReadIndex();
                if (!At('}')) { Fail(); }
                else { ++I; }
                return Value;
            }
            std::string_view Text;
            size_t I = 0;
            uint32_t Automatic = 0;
            int Mode = 0;
            FBraceSignature Result;
        };

        bool HasBraceStart(std::string_view Text)
        {
            for (size_t I = 0; I + 1 < Text.size(); ++I)
            {
                if (Text[I] == '{' && (IsDigit(Text[I + 1]) || Contains("}:", Text[I + 1]))) { return true; }
            }
            return false;
        }
        bool HasBraces(std::string_view Text) { return Text.find_first_of("{}") != std::string_view::npos; }
    }

    bool HaveCompatibleFormatArguments(FStringView Source, FStringView Translation)
    {
        const std::string_view S(Source.data(), Source.size()), T(Translation.data(), Translation.size());
        const FPrintfSignature SP = ParsePrintf(S);
        const FPrintfSignature TP = ParsePrintf(T, !SP.Active() && Contains(S, '%'));
        if (SP.Active())
        {
            if (!SP.Valid || !TP.Valid || SP.Arguments != TP.Arguments || SP.Escapes != TP.Escapes) { return false; }
        }
        else if (TP.Active() || (!TP.Valid && !Contains(S, '%'))) { return false; }

        const FBraceSignature SB = FBraceParser(S).Parse(), TB = FBraceParser(T).Parse();
        if (!SB.Fields.empty() || SB.Escapes[0] || SB.Escapes[1] || HasBraceStart(S))
        {
            return SB.Valid && TB.Valid && SB.Fields == TB.Fields && SB.Escapes == TB.Escapes;
        }
        if (!TB.Fields.empty() || TB.Escapes[0] || TB.Escapes[1] || HasBraceStart(T)) { return false; }
        return TB.Valid || HasBraces(S);
    }
}
