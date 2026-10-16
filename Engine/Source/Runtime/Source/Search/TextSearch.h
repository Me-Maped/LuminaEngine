#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace Lumina::TextSearch
{
    // Valid UTF-8, bounded substring matching; only ASCII letters are case folded.
    RUNTIME_API bool ContainsIgnoreCase(std::string_view Text, std::string_view Needle);

    // Ordinary editor search: whitespace-separated AND terms, literal punctuation.
    // Candidate fields are borrowed for the call; query bytes are owned.
    class RUNTIME_API FQuery
    {
    public:
        explicit FQuery(std::string_view Text = {});
        bool SetText(std::string_view Text);
        bool IsValid() const { return bValid; }
        size_t GetTermCount() const { return Terms.size(); }
        std::string_view GetTerm(size_t Index) const
        {
            const auto& Term = Terms.at(Index);
            return std::string_view(Query).substr(Term.Begin, Term.Length);
        }
        bool Matches(std::initializer_list<std::string_view> Fields) const { return MatchesFields(Fields); }

        template<class Range>
        bool MatchesFields(const Range& Fields) const
        {
            if (!bValid) { return false; }
            if (Terms.empty()) { return true; }
            for (size_t Index = 0; Index < Terms.size(); ++Index)
            {
                bool bFound = false;
                for (const auto& Field : Fields)
                {
                    if (ContainsIgnoreCase(static_cast<std::string_view>(Field), GetTerm(Index)))
                    {
                        bFound = true;
                        break;
                    }
                }
                if (!bFound) { return false; }
            }
            return true;
        }

    private:
        struct FTerm { size_t Begin; size_t Length; };
        std::string Query;
        std::vector<FTerm> Terms;
        bool bValid = true;
    };
}
