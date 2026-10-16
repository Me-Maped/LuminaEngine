#include "RuntimePCH.h"
#include "Localization/Localization.h"

#include "Localization/LocalizationSettings.h"
#include "Localization/LocalizationFormat.h"
#include "Core/Threading/Thread.h"
#include "Core/Object/ObjectCore.h"
#include "Core/Templates/LuminaTemplate.h"
#include "FileSystem/FileSystem.h"
#include "Log/Log.h"

#include "nlohmann/json.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace Lumina
{
    struct FLocalizationTable
    {
        ELanguage Language = ELanguage::ChineseSimplified;
        THashMap<FString, FString> Translations;
    };

    namespace
    {
        thread_local const FLocalizationSnapshot* GFrameSnapshot = nullptr;

        // One shipped language. Kept as data rather than scattered switch statements so adding a
        // language is one line here plus one ELanguage entry plus one table file.
        struct FLanguageDesc
        {
            ELanguage   Language;
            const char* Code;
            const char* DisplayName;
            const char* TableFile;
        };

        // Mirrors ELanguage's declaration order, which is also the order a picker lists them in.
        constexpr FLanguageDesc GLanguages[] =
        {
            { ELanguage::ChineseSimplified, "zh-Hans", "简体中文", "zh-Hans.json" },
            { ELanguage::English,           "en",      "English",   "en.json"      },
        };

        constexpr size_t GLanguageCount = sizeof(GLanguages) / sizeof(GLanguages[0]);

        const FLanguageDesc* FindLanguage(ELanguage InLanguage)
        {
            for (const FLanguageDesc& Desc : GLanguages)
            {
                if (Desc.Language == InLanguage)
                {
                    return &Desc;
                }
            }
            return nullptr;
        }

        // Parses a flat "source" -> "translation" object. Non-string values are skipped rather than
        // failing the load, so a table can carry nested sections for tooling without breaking the UI.
        bool ParseTable(FStringView JsonText, THashMap<FString, FString>& OutTranslations)
        {
            if (JsonText.empty())
            {
                return false;
            }

            try
            {
                const nlohmann::json Root = nlohmann::json::parse(JsonText.data(), JsonText.data() + JsonText.size());
                if (!Root.is_object())
                {
                    LOG_ERROR("Localization: table root is not a JSON object.");
                    return false;
                }

                for (auto It = Root.begin(); It != Root.end(); ++It)
                {
                    if (!It.value().is_string())
                    {
                        continue;
                    }

                    const std::string& Key = It.key();
                    const std::string& Value = It.value().get_ref<const std::string&>();
                    FStringView Source(Key.data(), Key.size());
                    // Context tags are metadata, never part of the displayed format string.
                    const size_t Separator = Source.rfind('|');
                    if (Separator != FStringView::npos && Separator + 1 < Source.size())
                    {
                        const FStringView Context = Source.substr(Separator + 1);
                        bool bContext = Context.front() >= 'a' && Context.front() <= 'z';
                        for (const char Character : Context)
                        {
                            bContext &= (Character >= 'a' && Character <= 'z')
                                || (Character >= '0' && Character <= '9') || Character == '_' || Character == '-';
                        }
                        if (bContext)
                        {
                            Source = Source.substr(0, Separator);
                        }
                    }
                    if (!Localization::HaveCompatibleFormatArguments(Source, FStringView(Value.data(), Value.size())))
                    {
                        LOG_WARN("Localization: incompatible format for '{0}'; using the English source.", Key.c_str());
                        continue;
                    }
                    OutTranslations.emplace(FString(Key.c_str(), Key.size()), FString(Value.c_str(), Value.size()));
                }
            }
            catch (const std::exception& Ex)
            {
                LOG_ERROR("Localization: failed to parse table ({0})", Ex.what());
                return false;
            }

            return true;
        }
    }

    FLocalization& FLocalization::Get()
    {
        static FLocalization Instance;
        return Instance;
    }

    void FLocalization::Initialize()
    {
        // The class default is the out-of-the-box language, so this still lands on the right language
        // when nothing has run DiscoverAndLoadSettings yet (headless tools and tests).
        ELanguage Desired = ELanguage::ChineseSimplified;
        if (const CLocalizationSettings* Settings = GetDefault<CLocalizationSettings>())
        {
            Desired = Settings->Language;
        }

        SetLanguage(Desired);
    }

    FLocalization::FLocalization()
        : ActiveTable(MakeShared<FLocalizationTable>())
    {
    }

    FLocalizationSnapshot::FLocalizationSnapshot(TSharedPtr<const FLocalizationTable> InTable)
        : Table(Move(InTable))
    {
    }

    FLocalizationSnapshot FLocalization::Read() const
    {
        FScopeLock Lock(TableMutex);
        return FLocalizationSnapshot(ActiveTable);
    }

    FLocalizationSnapshot FLocalization::ReadForCurrentFrame() const
    {
        return CurrentRead();
    }

    bool FLocalizationSnapshot::HasSameTable(const FLocalizationSnapshot& Other) const
    {
        return Table.Get() == Other.Table.Get();
    }

    void FLocalization::Publish(TSharedPtr<const FLocalizationTable> NewTable)
    {
        // Parse/I/O and destruction stay outside the short publication lock.
        {
            FScopeLock Lock(TableMutex);
            ActiveTable.Swap(NewTable);
        }
    }

    ELanguage FLocalizationSnapshot::GetLanguage() const
    {
        return Table ? Table->Language : ELanguage::ChineseSimplified;
    }

    ELanguage FLocalization::GetLanguage() const
    {
        return Read().GetLanguage();
    }

    FLocalizationFrameScope::FLocalizationFrameScope()
        : Snapshot(FLocalization::Get().Read())
        , Previous(GFrameSnapshot)
    {
        DEBUG_ASSERT(Threading::IsMainThread());
        GFrameSnapshot = &Snapshot;
    }

    FLocalizationFrameScope::~FLocalizationFrameScope()
    {
        DEBUG_ASSERT(Threading::IsMainThread() && GFrameSnapshot == &Snapshot);
        GFrameSnapshot = Previous;
    }

    const FLocalizationSnapshot& FLocalization::CurrentRead() const
    {
        if (GFrameSnapshot != nullptr)
        {
            return *GFrameSnapshot;
        }
        // Compatibility for immediate lookups outside a UI frame. Fibers must use explicit Read()
        // ownership, because their stack can resume on another thread after a wait.
        thread_local FLocalizationSnapshot Snapshot;
        Snapshot = Read();
        return Snapshot;
    }

    bool FLocalization::SetLanguage(ELanguage NewLanguage)
    {
        const FLanguageDesc* Desc = FindLanguage(NewLanguage);
        if (Desc == nullptr || Desc->TableFile == nullptr)
        {
            auto Table = MakeShared<FLocalizationTable>();
            Table->Language = NewLanguage;
            Publish(Move(Table));
            return false;
        }

        const FString Path = GetTablePath(NewLanguage);
        FString Text;
        if (!VFS::ReadFile(Text, Path))
        {
            LOG_WARN("Localization: no table at '{0}'; strings fall back to their English source.", Path.c_str());
            SetLanguageFromString(NewLanguage, "{}");
            return false;
        }
        const bool bLoaded = SetLanguageFromString(NewLanguage, FStringView(Text.c_str(), Text.size()));
        if (bLoaded)
        {
            LOG_DISPLAY("Localization: {0} loaded.", Desc->Code);
        }
        return bLoaded;
    }

    bool FLocalization::SetLanguageFromString(ELanguage InLanguage, FStringView JsonText)
    {
        auto Table = MakeShared<FLocalizationTable>();
        Table->Language = InLanguage;
        const bool bLoaded = ParseTable(JsonText, Table->Translations);
        if (!bLoaded)
        {
            Table->Translations.clear();
        }
        Publish(Move(Table));
        return bLoaded;
    }

    void FLocalization::ClearTable()
    {
        auto Table = MakeShared<FLocalizationTable>();
        Table->Language = GetLanguage();
        Publish(Move(Table));
    }

    const char* FLocalizationSnapshot::Translate(FStringView Source) const
    {
        return Translate(Source, FStringView());
    }

    const char* FLocalizationSnapshot::Translate(FStringView Source, FStringView Context) const
    {
        const FStringView Text = TranslateView(Source, Context);
        return Text.data() != nullptr ? Text.data() : "";
    }

    FStringView FLocalizationSnapshot::TranslateView(FStringView Source) const
    {
        return TranslateView(Source, FStringView());
    }

    FStringView FLocalizationSnapshot::TranslateView(FStringView Source, FStringView Context) const
    {
        if (Source.empty() || !Table)
        {
            return Source;
        }
        if (!Context.empty())
        {
            thread_local FString Key;
            Key.clear();
            Key.append(Source.data(), Source.size());
            Key.push_back('|');
            Key.append(Context.data(), Context.size());
            const auto Found = Table->Translations.find(FStringView(Key.c_str(), Key.size()));
            if (Found != Table->Translations.end())
            {
                return FStringView(Found->second.c_str(), Found->second.size());
            }
        }
        const auto Found = Table->Translations.find(Source);
        return Found != Table->Translations.end()
            ? FStringView(Found->second.c_str(), Found->second.size()) : Source;
    }

    const char* FLocalization::Translate(FStringView Source) const
    {
        return CurrentRead().Translate(Source);
    }

    const char* FLocalization::Translate(FStringView Source, FStringView Context) const
    {
        return CurrentRead().Translate(Source, Context);
    }

    FStringView FLocalization::TranslateView(FStringView Source) const
    {
        return CurrentRead().TranslateView(Source);
    }

    FStringView FLocalization::TranslateView(FStringView Source, FStringView Context) const
    {
        return CurrentRead().TranslateView(Source, Context);
    }

    FString FLocalization::GetTablePath(ELanguage InLanguage)
    {
        const FLanguageDesc* Desc = FindLanguage(InLanguage);
        if (Desc == nullptr || Desc->TableFile == nullptr)
        {
            return FString();
        }

        FString Directory = "/Engine/Resources/Localization";
        if (const CLocalizationSettings* Settings = GetDefault<CLocalizationSettings>())
        {
            if (!Settings->TableDirectory.empty())
            {
                Directory = Settings->TableDirectory;
            }
        }

        if (Directory.empty() || Directory.back() != '/')
        {
            Directory.push_back('/');
        }

        Directory.append(Desc->TableFile);
        return Directory;
    }

    const char* FLocalization::GetLanguageCode(ELanguage InLanguage)
    {
        const FLanguageDesc* Desc = FindLanguage(InLanguage);
        return Desc != nullptr ? Desc->Code : "";
    }

    const char* FLocalization::GetLanguageDisplayName(ELanguage InLanguage)
    {
        const FLanguageDesc* Desc = FindLanguage(InLanguage);
        return Desc != nullptr ? Desc->DisplayName : "";
    }

    size_t FLocalization::GetLanguageCount()
    {
        return GLanguageCount;
    }

    ELanguage FLocalization::GetLanguageByIndex(size_t Index)
    {
        return Index < GLanguageCount ? GLanguages[Index].Language : ELanguage::ChineseSimplified;
    }

    namespace Localization
    {
        const char* IconText(const char* Icon, const char* Source)
        {
            return IconText(Icon, Source, nullptr);
        }

        const char* IconText(const char* Icon, const char* Source, const char* Context)
        {
            constexpr size_t BufferCount = 8;
            constexpr size_t BufferSize  = 512;

            static thread_local char   Buffers[BufferCount][BufferSize];
            static thread_local size_t Next = 0;

            char* Out = Buffers[Next];
            Next = (Next + 1) % BufferCount;

            // A view rather than a pointer because the icon and the text are concatenated here
            // instead of by ImGui, so nothing else guarantees the text is terminated.
            const FStringView Text = FLocalization::Get().TranslateView(
                FStringView(Source), Context != nullptr ? FStringView(Context) : FStringView());

            snprintf(Out, BufferSize, "%s %.*s",
                Icon != nullptr ? Icon : "",
                (int)Text.size(),
                Text.empty() ? "" : Text.data());
            return Out;
        }

        const char* TranslateTemporary(FStringView Source)
        {
            return TranslateTemporary(Source, FStringView());
        }

        const char* TranslateTemporary(FStringView Source, FStringView Context)
        {
            constexpr size_t BufferCount = 8;
            constexpr size_t BufferSize  = 256;

            static thread_local char   Buffers[BufferCount][BufferSize];
            static thread_local size_t Next = 0;

            char* Out = Buffers[Next];
            Next = (Next + 1) % BufferCount;

            const FStringView Text = FLocalization::Get().TranslateView(Source, Context);
            const size_t Length = Math::Min(Text.size(), BufferSize - 1);

            if (Length > 0)
            {
                memcpy(Out, Text.data(), Length);
            }
            Out[Length] = '\0';
            return Out;
        }

        namespace
        {
            constexpr size_t GLabelBufferCount = 8;
            constexpr size_t GLabelBufferSize  = 1024;

            // One rotation shared by both label forms below, so a line that draws several of them
            // keeps them all valid until ImGui has copied them.
            char* NextLabelBuffer()
            {
                static thread_local char   Buffers[GLabelBufferCount][GLabelBufferSize];
                static thread_local size_t Next = 0;

                char* Out = Buffers[Next];
                Next = (Next + 1) % GLabelBufferCount;
                return Out;
            }

            // Writes "[<icon> ]<translated>###<source>". The identity half is what ImGui hashes, so
            // it is written whole while the visible halves yield when the three do not fit: a label
            // long enough to overflow the buffer must still not change the widget's identity.
            void WriteLabelWithIdentity(char* Out, const char* Icon, FStringView Text, FStringView Source)
            {
                constexpr size_t Size      = GLabelBufferSize;
                constexpr size_t Separator = 3;

                const size_t Identity = Math::Min(Source.size(), Size - 1 - Separator);
                size_t Room = Size - 1 - Separator - Identity;
                size_t Write = 0;

                if (Icon != nullptr && Icon[0] != '\0')
                {
                    const size_t IconLength = Math::Min(::strlen(Icon), Room);
                    if (IconLength > 0)
                    {
                        memcpy(Out + Write, Icon, IconLength);
                        Write += IconLength;
                        Room -= IconLength;
                    }

                    if (Room > 0)
                    {
                        Out[Write++] = ' ';
                        --Room;
                    }
                }

                const size_t Visible = Math::Min(Text.size(), Room);
                if (Visible > 0)
                {
                    memcpy(Out + Write, Text.data(), Visible);
                    Write += Visible;
                }

                memcpy(Out + Write, "###", Separator);
                Write += Separator;

                if (Identity > 0)
                {
                    memcpy(Out + Write, Source.data(), Identity);
                    Write += Identity;
                }

                Out[Write] = '\0';
            }
        }

        const char* LocalizedId(FStringView Source)
        {
            return LocalizedId(Source, FStringView());
        }

        const char* LocalizedId(FStringView Source, FStringView Context)
        {
            char* Out = NextLabelBuffer();
            WriteLabelWithIdentity(Out, nullptr, FLocalization::Get().TranslateView(Source, Context), Source);
            return Out;
        }

        const char* IconTextId(const char* Icon, FStringView Source)
        {
            char* Out = NextLabelBuffer();
            WriteLabelWithIdentity(Out, Icon, FLocalization::Get().TranslateView(Source), Source);
            return Out;
        }

        FString LocalizeFileDialogFilter(const char* Filter)
        {
            FString Out;
            if (Filter == nullptr || Filter[0] == '\0')
            {
                return Out;
            }

            const char* Cursor = Filter;
            while (*Cursor != '\0')
            {
                // A display half followed by its pattern. The scan is bounded by the nulls rather
                // than by strlen, since the whole run is one string with embedded terminators.
                const FStringView Display(Cursor);
                const FStringView Translated = FLocalization::Get().TranslateView(Display);
                Out.append(Translated.data(), Translated.size());
                Out.push_back('\0');
                Cursor += Display.size() + 1;

                const FStringView Patterns(Cursor);
                Out.append(Patterns.data(), Patterns.size());
                Out.push_back('\0');
                Cursor += Patterns.size() + 1;
            }

            // The empty display name that ends the run, which is also what Win32 wants for the
            // terminating null pair.
            Out.push_back('\0');
            return Out;
        }
    }
}
