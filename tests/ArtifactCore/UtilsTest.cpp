 #include <gtest/gtest.h>
 #include <QString>
 #include <string>
 #include <string_view>

 import Utils.String;
 import Utils.Path;
 import Utils.Convertor.String;
 import Utils.String.Like;
 import Core.Localization;

 using namespace ArtifactCore;

 // getExeDir() がプロダクションでは利用可能だが、
 // テスト環境ではカレントディレクトリを使用
 static QString getTestTranslationsDir() {
     return QCoreApplication::applicationDirPath() + "/translations";
 }

 TEST(UtilsTest, LevenshteinDistance) {
    // 完全一致
    EXPECT_EQ(calculateLevenshteinDistance("test", "test"), 0);
    
    // 1文字違い
    EXPECT_EQ(calculateLevenshteinDistance("test", "tesx"), 1);
    
    // 1文字削除
    EXPECT_EQ(calculateLevenshteinDistance("test", "tes"), 1);
    
    // 1文字追加
    EXPECT_EQ(calculateLevenshteinDistance("test", "testx"), 1);
    
    // 空文字列
    EXPECT_EQ(calculateLevenshteinDistance("", "test"), 4);
    EXPECT_EQ(calculateLevenshteinDistance("test", ""), 4);
    EXPECT_EQ(calculateLevenshteinDistance("", ""), 0);
    
    // 完全に異なる
    EXPECT_EQ(calculateLevenshteinDistance("abc", "def"), 3);
}

TEST(UtilsTest, StringConvertor) {
    EXPECT_EQ(toStdString(QString("test")), "test");
    EXPECT_EQ(toStdU16String(QString("test")), u"test");
}

TEST(UtilsTest, PathUtils) {
    EXPECT_FALSE(getAppPath().isEmpty());
    EXPECT_FALSE(getExeDir().isEmpty());
    EXPECT_FALSE(getCurrentDir().isEmpty());
    EXPECT_FALSE(getIconPath().isEmpty());
    EXPECT_FALSE(getIconResourceRoot().isEmpty());
    
    // 解像度テスト
    EXPECT_TRUE(resolveIconPath("visibility.svg").contains("Icon"));
    EXPECT_TRUE(resolveIconResourcePath("visibility.svg").startsWith(":/icons"));
}

 TEST(UtilsTest, StringLike) {
     EXPECT_EQ(toQString("test"), "test");
     EXPECT_EQ(toQString(std::string("test")), "test");
     EXPECT_EQ(toQString(std::string_view("test")), "test");
 }

 TEST(Localization, BasicTranslation) {
     auto& loc = LocalizationManager::instance();
     loc.resetStats();

     // 翻訳ファイルをロード
     QString testDir = QString::fromStdString(getExeDir().toStdString() + "/../tests/ArtifactCore/translations");
     EXPECT_TRUE(loc.loadFromDirectory(testDir));

     // 英語翻訳
     loc.setLanguage(LocaleLanguage::English);
     EXPECT_EQ(loc.translate("app.title"), "Artifact Studio");
     EXPECT_EQ(loc.translate("menu.file"), "File");

     // 日本語翻訳
     loc.setLanguage(LocaleLanguage::Japanese);
     EXPECT_EQ(loc.translate("app.title"), "アーティファクト スタジオ");
     EXPECT_EQ(loc.translate("menu.file"), "ファイル");

     // 英語に戻す
     loc.setLanguage(LocaleLanguage::English);
 }

 TEST(Localization, PlaceholderSubstitution) {
     auto& loc = LocalizationManager::instance();

     // プレースホルダー付き翻訳
     QString result = loc.translate("greeting", {"World"});
     EXPECT_EQ(result, "Hello World!");

     // 日本語
     loc.setLanguage(LocaleLanguage::Japanese);
     result = loc.translate("greeting", {"世界"});
     EXPECT_EQ(result, "こんにちは 世界！");

     loc.setLanguage(LocaleLanguage::English);
 }

 TEST(Localization, MissingKeyTracking) {
     auto& loc = LocalizationManager::instance();
     loc.resetStats();

     // 存在しないキー
     QString result = loc.translate("nonexistent.key");
     EXPECT_EQ(result, "nonexistent.key");

     // 統計で欠落キーが追跡されている
     LocalizationStats stats = loc.getStats();
     EXPECT_EQ(stats.missingKeyCount, 1);
     EXPECT_TRUE(stats.missingKeys.contains("nonexistent.key"));
     EXPECT_EQ(stats.missingKeys["nonexistent.key"], 1);
 }

 TEST(Localization, CachePerformance) {
     auto& loc = LocalizationManager::instance();
     loc.resetStats();
     loc.clearCache();

     // 同じキーを複数回呼び出し
     loc.translate("app.title");
     loc.translate("app.title");
     loc.translate("app.title");

     LocalizationStats stats = loc.getStats();
     // 2回のキャッシュヒットがある
     EXPECT_GE(stats.cacheHits, 2);
     EXPECT_LE(stats.cacheMisses, 1);
 }

 TEST(Localization, CacheClear) {
     auto& loc = LocalizationManager::instance();
     loc.resetStats();

     // キャッシュ生成
     loc.translate("app.title");
     EXPECT_GT(loc.cacheSize(), 0);

     // キャッシュクリア
     loc.clearCache();
     EXPECT_EQ(loc.cacheSize(), 0);
 }

