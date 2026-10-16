#pragma once

#include "Containers/HashTable.h"
#include "Containers/String.h"
#include "Core/Threading/Sync.h"
#include "Memory/SmartPtr.h"
#include "Core/Object/ObjectMacros.h"
#include "Platform/GenericPlatform.h"
#include "Localization.generated.h"

namespace Lumina
{
    // Language a UI presents itself in. The declaration order is the picker order, and the first
    // entry is what an installation that has never saved a choice starts on, i.e. Chinese by default.
    REFLECT()
    enum class ELanguage : uint8
    {
        ChineseSimplified,
        English,
    };

    struct FLocalizationTable;

    /** Owning immutable read snapshot. Safe across threads, fiber yields and language changes.
     * Borrowed translations live as long as this snapshot; fallback source views remain caller-owned.
     */
    class RUNTIME_API FLocalizationSnapshot
    {
    public:
        FLocalizationSnapshot() = default;
        ELanguage GetLanguage() const;
        const char* Translate(FStringView Source) const;
        const char* Translate(FStringView Source, FStringView Context) const;
        FStringView TranslateView(FStringView Source) const;
        FStringView TranslateView(FStringView Source, FStringView Context) const;
        // Identity includes a reload of the same language. Both snapshots own their tables.
        bool HasSameTable(const FLocalizationSnapshot& Other) const;

    private:
        friend class FLocalization;
        explicit FLocalizationSnapshot(TSharedPtr<const FLocalizationTable> InTable);
        TSharedPtr<const FLocalizationTable> Table;
    };

    /** Pins LOC lookups for one main-thread frame. Background/fiber jobs use explicit snapshots.
     * A language published during the frame becomes visible in the next frame.
     */
    class RUNTIME_API FLocalizationFrameScope
    {
    public:
        FLocalizationFrameScope();
        ~FLocalizationFrameScope();
        FLocalizationFrameScope(const FLocalizationFrameScope&) = delete;
        FLocalizationFrameScope& operator=(const FLocalizationFrameScope&) = delete;

    private:
        FLocalizationSnapshot Snapshot;
        const FLocalizationSnapshot* Previous = nullptr;
    };

    // Resolves English source text to the active language for the editor chrome and the game UI.
    //
    // Tables are one flat JSON object per language, keyed by the English source string, e.g.
    //     { "Save": "保存" }
    // The source is also the missing-key fallback, so a string that has not been translated yet
    // still reads as English instead of going blank. Keeping the source as the key is what lets a
    // call site stay a plain string literal and lets translation be added file by file.
    //
    // Lives in the runtime module because both editor and game code draw through it; the editor
    // drives the settings object that chooses the language.
    class RUNTIME_API FLocalization
    {
    public:

        static FLocalization& Get();

        // Reads CLocalizationSettings and loads its table. Safe to call more than once; the engine
        // calls it right after settings load so the saved language is live before any UI draws.
        void Initialize();

        // Switches the active language, loading its table. The language is applied even when the
        // table cannot be read -- every lookup then falls back to the English source.
        bool SetLanguage(ELanguage NewLanguage);

        ELanguage GetLanguage() const;

        /** Obtain a reader-owned table without holding a lock while formatting or running a task. */
        FLocalizationSnapshot Read() const;

        // UI caches must use the same table as LOC during a pinned frame. Outside a frame,
        // this is a normal owning read; no borrowed thread-local pointer escapes.
        FLocalizationSnapshot ReadForCurrentFrame() const;

        // Sets the active language from an in-memory table instead of the file system. Exists for
        // tests, tools and future cooked string tables that never touch disk.
        bool SetLanguageFromString(ELanguage InLanguage, FStringView JsonText);

        // Drops the loaded table; every lookup then resolves to its source string.
        void ClearTable();

        // Translated text for Source, or Source itself when the active language has no entry.
        // Source is expected to be null-terminated and to outlive the lookup (every call site
        // passes a literal). Inside FLocalizationFrameScope the result lives through the frame.
        // Outside it, the calling thread pins the result until its next unscoped lookup. Use Read()
        // for multiple retained results or fiber tasks; its pointers live as long as the snapshot.
        const char* Translate(FStringView Source) const;

        // Same lookup, but handed back as a view. For sources that are themselves views and are not
        // guaranteed to be terminated, so the caller can copy it with an explicit length.
        NODISCARD FStringView TranslateView(FStringView Source) const;

        // Disambiguated lookup for a word whose reading depends on where it appears. The table key
        // is "Source|Context", and resolution walks Source|Context, then Source, then Source itself,
        // so a context is a pure addition: every entry keyed by a bare source keeps working, and only
        // the handful of words that genuinely read two ways ever needs a contextual entry. Context is
        // a plain lowercase tag ("audio", "texture") that names the domain, not the other reading.
        const char* Translate(FStringView Source, FStringView Context) const;

        // TranslateView with the same context resolution.
        NODISCARD FStringView TranslateView(FStringView Source, FStringView Context) const;

        // Stable identifier used for the config value and the table filename, e.g. "zh-Hans".
        static const char* GetLanguageCode(ELanguage InLanguage);

        // Endonym shown in a language picker, so the list reads in each language's own script.
        static const char* GetLanguageDisplayName(ELanguage InLanguage);

        // Reflected languages in declaration order, for a picker.
        static size_t GetLanguageCount();
        static ELanguage GetLanguageByIndex(size_t Index);

    private:

        // VFS path of a language's table, or empty when the language has no shipped table.
        static FString GetTablePath(ELanguage InLanguage);

        FLocalization();
        void Publish(TSharedPtr<const FLocalizationTable> NewTable);
        const FLocalizationSnapshot& CurrentRead() const;

        mutable FMutex TableMutex;
        TSharedPtr<const FLocalizationTable> ActiveTable;
    };

    namespace Localization
    {
        // "<icon> <translated text>", the shape almost every menu entry and button takes. The icon is
        // kept out of the lookup so the table stays free of glyph codepoints. Writes into one of a few
        // rotating thread-local buffers: ImGui consumes a label inside the call that receives it, and
        // the rotation leaves several live at once for a line that draws more than one.
        RUNTIME_API const char* IconText(const char* Icon, const char* Source);

        // Same, with a context for a source whose reading depends on where it appears. See
        // FLocalization::Translate for how the context resolves.
        RUNTIME_API const char* IconText(const char* Icon, const char* Source, const char* Context);

        // Translated copy of a source view that may not be terminated, written into one of the same
        // rotating buffers. The result stays valid for the next few calls, which is all a label needs.
        RUNTIME_API const char* TranslateTemporary(FStringView Source);

        // TranslateTemporary with a context. Used where a source is a runtime string (a pin name, an
        // interned class name) and the domain it is drawn in disambiguates it.
        RUNTIME_API const char* TranslateTemporary(FStringView Source, FStringView Context);

        // "<translated>###<source>", the label shape a widget needs when its text is also its ImGui
        // ID. ImHashStr restarts its hash at "###", so the ID hashes the source while the visible
        // half stays translated: the widget keeps its identity (and ImGui keeps its stored state)
        // across a language switch, and two labels that happen to translate alike cannot collide.
        // ImGui displays nothing past the "###". Needed because a runtime translation cannot be
        // concatenated with a literal at compile time. Same rotating buffers as the rest.
        RUNTIME_API const char* LocalizedId(FStringView Source);
        RUNTIME_API const char* LocalizedId(FStringView Source, FStringView Context);

        // Same, for the "<icon> Label" form, for example a collapsing header that carries an icon:
        // "<icon> <translated>###<source>".
        RUNTIME_API const char* IconTextId(const char* Icon, FStringView Source);

        // Translated copy of a "Display\0Pattern\0Display\0Pattern\0\0" file-dialog filter, which is
        // the shape every platform's OpenFileDialogue takes. Only the display halves are translated;
        // the glob patterns are data and are copied through untouched. The platform dialogues call
        // this themselves, so a call site keeps passing a filter literal and gets a localized dialog
        // without doing anything. Returned by value because the copy has to outlive the dialogue.
        RUNTIME_API FString LocalizeFileDialogFilter(const char* Filter);
    }
}

// Translated form of a source string, for example ImGui::MenuItem(LOC("Save")). The argument has to
// be a string literal because it doubles as the English fallback.
#define LOC(Source) (::Lumina::FLocalization::Get().Translate(FStringView(Source)))

// Same, for the "<icon> Label" form the menus use, for example LOC_ICON(LE_ICON_COG, "Settings").
#define LOC_ICON(Icon, Source) (::Lumina::Localization::IconText(Icon, Source))

// Same as LOC, for a source that is not a literal -- an interned class name coming back as a view,
// for instance. Still falls back to the source, and the result must be consumed before the next few
// calls, so it is for a drawn label rather than something stored.
#define LOC_VIEW(Source) (::Lumina::Localization::TranslateTemporary(Source))

// Same as LOC, for a word that reads differently depending on where it appears. The table key is
// "Source|Context", and a missing contextual entry falls back to the plain source, so a context is
// only worth adding for a word that is genuinely ambiguous. The context is a domain tag, never shown:
//     LOC_CTX("Usage", "texture")   // 用途 for a texture's usage flags
//     LOC("Usage")                  // 使用量 for the plain memory figure
#define LOC_CTX(Source, Context) (::Lumina::FLocalization::Get().Translate(FStringView(Source), FStringView(Context)))

// Same, for the "<icon> Label" form, for example LOC_ICON_CTX(LE_ICON_COG, "Format", "rmlui").
#define LOC_ICON_CTX(Icon, Source, Context) (::Lumina::Localization::IconText(Icon, Source, Context))

// Same as LOC_VIEW, for a source that is not a literal and needs disambiguating.
#define LOC_VIEW_CTX(Source, Context) (::Lumina::Localization::TranslateTemporary(FStringView(Source), FStringView(Context)))

// Translated "label###identity", for a widget whose label is also its ImGui ID: checkboxes,
// collapsing headers, table columns, sliders, combo and selectable rows. Without it the ID moves
// with the language, which drops whatever ImGui keyed on it -- table column widths and order, a
// section's collapsed state, the item being dragged. The argument has to be the English source,
// because that is what the ID is built from and what a future translation is keyed by.
#define LOC_ID(Source) (::Lumina::Localization::LocalizedId(FStringView(Source)))

// Same, for a source that is not a literal and needs disambiguating.
#define LOC_ID_CTX(Source, Context) (::Lumina::Localization::LocalizedId(FStringView(Source), FStringView(Context)))

// Same, for the "<icon> Label" form.
#define LOC_ICON_ID(Icon, Source) (::Lumina::Localization::IconTextId(Icon, FStringView(Source)))

// Translated format string for the engine's own formatter, for example ImGuiX::Text(LOCF("{} assets"), N).
// The literal carries the placeholders, so a translation has to keep them.
//
// Use this for EVERY argument that lands in a format-string parameter, including a message with no
// placeholders at all -- ImGuiX::Text, the Notify* helpers and Dialogs take a compile-time checked
// format string there, so a runtime LOC() does not compile. Inside a lambda it is worse than a
// compile error at the call site: C++ promotes a lambda that mentions such a non-constant consteval
// call to an immediate function, and the error then surfaces wherever the lambda is invoked.
#define LOCF(Source) (::Lumina::Fmt::RuntimeFormat(LOC(Source)))

// The English plural suffix, as its own key so a format string can inflect without splitting into a
// singular/plural pair: LOC("%d row%s") with LOC_S. Chinese does not inflect for number, so the
// table maps "s" to an empty string on purpose -- that empty entry IS the translation, and losing it
// would leak a stray "s" into the UI. A message that does not have to compose should use two keys
// instead, which is what the rest of the editor does.
#define LOC_S (LOC("s"))

// Explicit reader ownership for asynchronous/fiber work; do not install thread-local frame scopes there.
#define LOC_IN(Snapshot, Source) ((Snapshot).Translate(::Lumina::FStringView(Source)))
#define LOCF_IN(Snapshot, Source) (::Lumina::Fmt::RuntimeFormat(LOC_IN(Snapshot, Source)))
