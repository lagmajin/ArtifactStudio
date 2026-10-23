module;

#include <QString>
#include <QStringList>

module ArtifactPr.Translation;

import Core.Localization;

namespace ArtifactPr {

void initializeTranslations(const QString& catalogDirectory, const QString& languageCode)
{
    auto& manager = ArtifactCore::LocalizationManager::instance();
    manager.loadFromDirectory(catalogDirectory);
    if (languageCode.isEmpty()) {
        manager.setLanguage(ArtifactCore::LocaleLanguage::Auto);
    } else {
        manager.setLanguageCode(languageCode);
    }
}

QString currentLanguageCode()
{
    return ArtifactCore::LocalizationManager::instance().languageCode();
}

QStringList availableLocales()
{
    return ArtifactCore::LocalizationManager::instance().availableLocales();
}

QString trText(const char* key, const char* fallback)
{
    const QString lookupKey = QString::fromUtf8(key);
    const QString value = ArtifactCore::LocalizationManager::instance().translate(lookupKey);
    if (value == lookupKey) {
        return QString::fromUtf8(fallback);
    }
    return value;
}

} // namespace ArtifactPr
