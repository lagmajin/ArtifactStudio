module;

#include <QString>
#include <QStringList>

export module ArtifactPr.Translation;

export namespace ArtifactPr {

/**
 * Load the translation catalogs and select the active locale.
 *
 * @param catalogDirectory Directory that holds the <locale>.json catalogs.
 * @param languageCode     Locale code to activate ("ja", "en", "zh", ...).
 *                         An empty string follows the system locale.
 */
void initializeTranslations(const QString& catalogDirectory, const QString& languageCode);

/// Active locale code ("en", "ja", ...).
QString currentLanguageCode();

/// Locale codes whose catalog is loaded.
QStringList availableLocales();

/**
 * Look up @p key in the active catalog.
 *
 * Returns @p fallback when no catalog holds the key, so a missing translation
 * degrades to the inline English default instead of showing the raw key.
 */
QString trText(const char* key, const char* fallback);

} // namespace ArtifactPr
