#pragma once

#include "Config/DeveloperSettings.h"
#include "Containers/String.h"
#include "Localization/Localization.h"
#include "LocalizationSettings.generated.h"

namespace Lumina
{
    // Which language the editor chrome and the game UI present. Persisted beside the other editor
    // preferences; the member initializer is the out-of-the-box language, so a project that has
    // never saved this section starts in Chinese.
    REFLECT(MinimalAPI, ConfigFile = "/Editor/Config/EditorPreferences.json", DisplayName = "Localization", Category = "Editor")
    class CLocalizationSettings : public CDeveloperSettings
    {
        GENERATED_BODY()

    public:

        /** Language every localized string is resolved to. Changing this reloads the string table. */
        PROPERTY(Editable, Category = "Language")
        ELanguage Language = ELanguage::ChineseSimplified;

        /** VFS directory holding one flat <code>.json table per language. */
        PROPERTY(Editable, Category = "Language")
        FString TableDirectory = "/Engine/Resources/Localization";
    };
}
