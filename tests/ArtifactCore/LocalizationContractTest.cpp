#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <limits>
#include <thread>
#include <utility>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

import Core.Localization;

using namespace ArtifactCore;

namespace {
bool writeLocale(const QTemporaryDir& directory, const QString& code,
                 const QJsonObject& translations)
{
    QFile file(QDir(directory.path()).filePath(code + QStringLiteral(".json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray bytes = QJsonDocument(translations).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size()) return false;
    return file.flush();
}

class LocalizationReset {
public:
    LocalizationReset()
    {
        LocalizationManager::instance().clearTranslations();
    }

    ~LocalizationReset()
    {
        LocalizationManager::instance().clearTranslations();
    }
};
}

TEST(LocalizationContractTest, DirectoryLoadFlattensNestedKeysAndUsesTraditionalSimplifiedEnglishFallback)
{
    LocalizationReset reset;
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(writeLocale(directory, QStringLiteral("en"), QJsonObject{
        {QStringLiteral("_meta"), QJsonObject{{QStringLiteral("version"), 1}}},
        {QStringLiteral("editor"), QJsonObject{
             {QStringLiteral("open"), QStringLiteral("Open")},
             {QStringLiteral("close"), QStringLiteral("Close")},
             {QStringLiteral("plural"), QJsonObject{
                  {QStringLiteral("one"), QStringLiteral("%1 file")},
                  {QStringLiteral("other"), QStringLiteral("%1 files")}}}}},
        {QStringLiteral("common"), QStringLiteral("Shared")},
    }));
    ASSERT_TRUE(writeLocale(directory, QStringLiteral("zh"), QJsonObject{
        {QStringLiteral("editor"), QJsonObject{{QStringLiteral("open"), QStringLiteral("打开")}}},
        {QStringLiteral("common"), QStringLiteral("共通")},
    }));
    ASSERT_TRUE(writeLocale(directory, QStringLiteral("zh-TW"), QJsonObject{
        {QStringLiteral("editor"), QJsonObject{{QStringLiteral("close"), QStringLiteral("關閉")}}},
        {QStringLiteral("common"), QStringLiteral("Shared")},
    }));

    auto& localization = LocalizationManager::instance();
    ASSERT_TRUE(localization.loadFromDirectory(directory.path()));
    EXPECT_TRUE(localization.availableLocales().contains(QStringLiteral("en")));
    EXPECT_TRUE(localization.availableLocales().contains(QStringLiteral("zh")));
    EXPECT_TRUE(localization.availableLocales().contains(QStringLiteral("zh-TW")));
    EXPECT_EQ(localization.translationCount(QStringLiteral("zh_TW")), 2);

    localization.setLanguage(LocaleLanguage::ChineseTraditional);
    EXPECT_EQ(localization.translate(QStringLiteral("editor.close")), QStringLiteral("關閉"));
    EXPECT_EQ(localization.translate(QStringLiteral("editor.open")), QStringLiteral("打开"));
    EXPECT_EQ(localization.translate(QStringLiteral("editor.plural.other")), QStringLiteral("%1 files"));
    EXPECT_EQ(localization.translate(QStringLiteral("common")), QStringLiteral("Shared"));
    EXPECT_EQ(localization.translate(QStringLiteral("_meta.version")), QStringLiteral("_meta.version"));
    EXPECT_EQ(localization.translate(QStringLiteral("missing.key")), QStringLiteral("missing.key"));
    EXPECT_TRUE(localization.missingKeys().contains(QStringLiteral("editor.open")));
    EXPECT_TRUE(localization.untranslatedKeys().contains(QStringLiteral("common")));
}

TEST(LocalizationContractTest, RussianAndArabicPluralRulesResolveCategoryKeysAndFallbacks)
{
    LocalizationReset reset;
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(writeLocale(directory, QStringLiteral("en"), QJsonObject{
        {QStringLiteral("items"), QJsonObject{
             {QStringLiteral("one"), QStringLiteral("%1 item")},
             {QStringLiteral("other"), QStringLiteral("%1 items")}}},
        {QStringLiteral("fallback"), QJsonObject{
             {QStringLiteral("other"), QStringLiteral("%1 fallback items")}}},
    }));
    ASSERT_TRUE(writeLocale(directory, QStringLiteral("ru"), QJsonObject{
        {QStringLiteral("items"), QJsonObject{
             {QStringLiteral("one"), QStringLiteral("%1 предмет")},
             {QStringLiteral("few"), QStringLiteral("%1 предмета")},
             {QStringLiteral("many"), QStringLiteral("%1 предметов")},
             {QStringLiteral("other"), QStringLiteral("%1 предмета (дробное)")}}},
    }));
    ASSERT_TRUE(writeLocale(directory, QStringLiteral("ar"), QJsonObject{
        {QStringLiteral("items"), QJsonObject{
             {QStringLiteral("zero"), QStringLiteral("لا عناصر")},
             {QStringLiteral("one"), QStringLiteral("عنصر واحد")},
             {QStringLiteral("two"), QStringLiteral("عنصران")},
             {QStringLiteral("few"), QStringLiteral("%1 عناصر")},
             {QStringLiteral("many"), QStringLiteral("%1 عنصرًا")},
             {QStringLiteral("other"), QStringLiteral("%1 عنصر")}}},
    }));

    auto& localization = LocalizationManager::instance();
    ASSERT_TRUE(localization.loadFromDirectory(directory.path()));
    localization.setLanguage(LocaleLanguage::Russian);
    EXPECT_EQ(localization.pluralCategory(1), PluralCategory::One);
    EXPECT_EQ(localization.pluralCategory(2), PluralCategory::Few);
    EXPECT_EQ(localization.pluralCategory(5), PluralCategory::Many);
    EXPECT_EQ(localization.pluralCategory(21), PluralCategory::One);
    EXPECT_EQ(localization.pluralCategory(1.5), PluralCategory::Other);
    EXPECT_EQ(localization.translatePlural(QStringLiteral("items"), 1, "one", "many"),
              QStringLiteral("1 предмет"));
    EXPECT_EQ(localization.translatePlural(QStringLiteral("items"), 22, "one", "many"),
              QStringLiteral("22 предмета"));
    EXPECT_EQ(localization.translatePlural(QStringLiteral("items"), 1.5, "one", "many"),
              QStringLiteral("1.5 предмета (дробное)"));
    EXPECT_EQ(localization.translatePlural(QStringLiteral("fallback"), 22, "one", "many"),
              QStringLiteral("22 fallback items"));
    EXPECT_EQ(localization.translatePlural(QStringLiteral("absent"), 2, "one", "many"),
              QStringLiteral("many"));

    localization.setLanguage(LocaleLanguage::Arabic);
    const std::array arabicCases{
        std::pair{0.0, PluralCategory::Zero},
        std::pair{1.0, PluralCategory::One},
        std::pair{2.0, PluralCategory::Two},
        std::pair{3.0, PluralCategory::Few},
        std::pair{11.0, PluralCategory::Many},
        std::pair{100.0, PluralCategory::Other},
    };
    for (const auto& [count, expected] : arabicCases) {
        EXPECT_EQ(localization.pluralCategory(count), expected);
    }
    EXPECT_EQ(localization.translatePlural(QStringLiteral("items"), 0, "one", "many"),
              QStringLiteral("لا عناصر"));
    EXPECT_EQ(localization.translatePlural(QStringLiteral("items"), 2, "one", "many"),
              QStringLiteral("عنصران"));
    EXPECT_EQ(localization.pluralCategory(-1), PluralCategory::One);
    EXPECT_EQ(localization.pluralCategory(-2), PluralCategory::Two);
    EXPECT_EQ(localization.pluralCategory(-3), PluralCategory::Few);
    EXPECT_EQ(localization.pluralCategory(-11), PluralCategory::Many);
    EXPECT_EQ(localization.pluralCategory(1.5), PluralCategory::Other);
    EXPECT_EQ(localization.pluralCategory(-2.5), PluralCategory::Other);
}

TEST(LocalizationContractTest,
     ReloadingDirectoryRemovesLocalesAndFallbackKeysMissingFromNewCatalog)
{
    LocalizationReset reset;
    QTemporaryDir firstDirectory;
    QTemporaryDir replacementDirectory;
    ASSERT_TRUE(firstDirectory.isValid());
    ASSERT_TRUE(replacementDirectory.isValid());
    ASSERT_TRUE(writeLocale(firstDirectory, QStringLiteral("en"), QJsonObject{
        {QStringLiteral("shared"), QStringLiteral("Old English")},
        {QStringLiteral("obsolete"), QStringLiteral("Old fallback")},
    }));
    ASSERT_TRUE(writeLocale(firstDirectory, QStringLiteral("ja"), QJsonObject{
        {QStringLiteral("obsolete"), QStringLiteral("古い翻訳")},
    }));
    ASSERT_TRUE(writeLocale(replacementDirectory, QStringLiteral("en"), QJsonObject{
        {QStringLiteral("shared"), QStringLiteral("New English")},
    }));

    auto& localization = LocalizationManager::instance();
    ASSERT_TRUE(localization.loadFromDirectory(firstDirectory.path()));
    ASSERT_TRUE(localization.availableLocales().contains(QStringLiteral("ja")));

    localization.setLanguage(LocaleLanguage::Japanese);
    EXPECT_EQ(localization.translate(QStringLiteral("obsolete")),
              QStringLiteral("古い翻訳"));

    ASSERT_TRUE(localization.loadFromDirectory(replacementDirectory.path()));

    EXPECT_EQ(localization.availableLocales(), QStringList{QStringLiteral("en")});
    EXPECT_EQ(localization.translate(QStringLiteral("shared")),
              QStringLiteral("New English"));
    EXPECT_EQ(localization.translate(QStringLiteral("obsolete")),
              QStringLiteral("obsolete"));
    EXPECT_EQ(localization.missingKeys(), QStringList{QStringLiteral("shared")});
    EXPECT_EQ(localization.loadedKeys(), QStringList{QStringLiteral("shared")});
    EXPECT_TRUE(localization.untranslatedKeys().isEmpty());

    localization.setLanguage(LocaleLanguage::English);
    EXPECT_TRUE(localization.missingKeys().isEmpty());
    EXPECT_EQ(localization.loadedKeys(), QStringList{QStringLiteral("shared")});
    EXPECT_TRUE(localization.untranslatedKeys().isEmpty());
}

TEST(LocalizationContractTest,
     DirectoryWithoutEnglishCatalogDropsPreviousFallbackAndLocale)
{
    LocalizationReset reset;
    QTemporaryDir firstDirectory;
    QTemporaryDir replacementDirectory;
    ASSERT_TRUE(firstDirectory.isValid());
    ASSERT_TRUE(replacementDirectory.isValid());
    ASSERT_TRUE(writeLocale(firstDirectory, QStringLiteral("en"), QJsonObject{
        {QStringLiteral("retired"), QStringLiteral("Old fallback text")},
        {QStringLiteral("shared"), QStringLiteral("Old English text")},
    }));
    ASSERT_TRUE(writeLocale(firstDirectory, QStringLiteral("ja"), QJsonObject{
        {QStringLiteral("retired"), QStringLiteral("古い翻訳")},
        {QStringLiteral("shared"), QStringLiteral("古い共有文言")},
    }));
    ASSERT_TRUE(writeLocale(replacementDirectory, QStringLiteral("ja"), QJsonObject{
        {QStringLiteral("shared"), QStringLiteral("新しい共有文言")},
    }));

    auto& localization = LocalizationManager::instance();
    ASSERT_TRUE(localization.loadFromDirectory(firstDirectory.path()));
    localization.setLanguage(LocaleLanguage::Japanese);
    ASSERT_EQ(localization.translate(QStringLiteral("retired")),
              QStringLiteral("古い翻訳"));

    ASSERT_TRUE(localization.loadFromDirectory(replacementDirectory.path()));

    EXPECT_EQ(localization.availableLocales(), QStringList{QStringLiteral("ja")});
    EXPECT_EQ(localization.translate(QStringLiteral("shared")),
              QStringLiteral("新しい共有文言"));
    EXPECT_EQ(localization.translate(QStringLiteral("retired")),
              QStringLiteral("retired"));
    EXPECT_TRUE(localization.missingKeys().isEmpty());
    EXPECT_EQ(localization.loadedKeys(), QStringList{QStringLiteral("shared")});
    EXPECT_TRUE(localization.untranslatedKeys().isEmpty());
}

TEST(LocalizationContractTest,
     ConcurrentReadersObserveOnlyCompleteCatalogsDuringDirectoryReload)
{
    LocalizationReset reset;
    QTemporaryDir firstDirectory;
    QTemporaryDir secondDirectory;
    ASSERT_TRUE(firstDirectory.isValid());
    ASSERT_TRUE(secondDirectory.isValid());
    ASSERT_TRUE(writeLocale(firstDirectory, QStringLiteral("en"), QJsonObject{
        {QStringLiteral("catalog.generation"), QStringLiteral("first-generation")},
        {QStringLiteral("catalog.required"), QStringLiteral("present")},
    }));
    ASSERT_TRUE(writeLocale(secondDirectory, QStringLiteral("en"), QJsonObject{
        {QStringLiteral("catalog.generation"), QStringLiteral("second-generation")},
        {QStringLiteral("catalog.required"), QStringLiteral("present")},
    }));

    auto& localization = LocalizationManager::instance();
    ASSERT_TRUE(localization.loadFromDirectory(firstDirectory.path()));
    localization.setLanguage(LocaleLanguage::English);

    std::atomic<bool> readerReady{false};
    std::atomic<bool> startReader{false};
    std::atomic<bool> finishReader{false};
    std::atomic<int> invalidReads{0};
    std::atomic<int> readCount{0};
    std::thread reader([&] {
        readerReady.store(true, std::memory_order_release);
        while (!startReader.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        do {
            const QString generation = localization.translate(
                QStringLiteral("catalog.generation"));
            const QString required = localization.translate(
                QStringLiteral("catalog.required"));
            if ((generation != QStringLiteral("first-generation") &&
                 generation != QStringLiteral("second-generation")) ||
                required != QStringLiteral("present")) {
                invalidReads.fetch_add(1, std::memory_order_relaxed);
            }
            readCount.fetch_add(1, std::memory_order_relaxed);
        } while (!finishReader.load(std::memory_order_acquire));
    });

    while (!readerReady.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    startReader.store(true, std::memory_order_release);
    bool allReloadsSucceeded = true;
    for (int reload = 0; reload < 250; ++reload) {
        const QString directory = reload % 2 == 0
            ? secondDirectory.path() : firstDirectory.path();
        if (!localization.loadFromDirectory(directory)) {
            allReloadsSucceeded = false;
            break;
        }
    }
    finishReader.store(true, std::memory_order_release);
    reader.join();

    EXPECT_TRUE(allReloadsSucceeded);
    EXPECT_GT(readCount.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(invalidReads.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(localization.translate(QStringLiteral("catalog.required")),
              QStringLiteral("present"));
}

TEST(LocalizationContractTest,
     PluralRulesHandleNonFiniteAndValuesBeyondIntRange)
{
    LocalizationReset reset;
    auto& localization = LocalizationManager::instance();
    const std::array languages{
        LocaleLanguage::English,
        LocaleLanguage::Russian,
        LocaleLanguage::Arabic,
    };
    const std::array nonFiniteCounts{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };

    for (const LocaleLanguage language : languages) {
        localization.setLanguage(language);
        for (const double count : nonFiniteCounts) {
            EXPECT_EQ(localization.pluralCategory(count), PluralCategory::Other)
                << "language=" << static_cast<int>(language)
                << " count=" << count;
        }
    }

    localization.setLanguage(LocaleLanguage::Russian);
    EXPECT_EQ(localization.pluralCategory(2147483648.0), PluralCategory::Many);
    EXPECT_EQ(localization.pluralCategory(2147483651.0), PluralCategory::One);
    EXPECT_EQ(localization.pluralCategory(-2147483648.0), PluralCategory::Many);

    localization.setLanguage(LocaleLanguage::Arabic);
    EXPECT_EQ(localization.pluralCategory(2147483648.0), PluralCategory::Many);
}
