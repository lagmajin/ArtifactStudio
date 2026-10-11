#include <gtest/gtest.h>

#include <future>
#include <QString>
#include <QStringView>

#include <string_view>
#include <thread>
#include <type_traits>

import Utils.String.View;

using namespace ArtifactCore;

TEST(StringViewContractTest, AliasAndQStringConversionPreserveText)
{
    static_assert(std::is_same_v<StringView, QStringView>);
    const QString source = QString::fromUtf8("café 東京");
    EXPECT_EQ(toQString(toStringView(source)), source);
}

TEST(StringViewContractTest, Utf8ConversionPreservesNonAsciiText)
{
    const QString source = QString::fromUtf8("naïve 東京");
    const std::string utf8 = toStdString(toStringView(source));
    EXPECT_EQ(utf8, std::string("naïve 東京"));
    EXPECT_EQ(toQString(std::string_view(utf8)), source);
}

TEST(StringViewContractTest, Utf8ConversionPreservesSupplementaryUnicodeCharacters)
{
    const QString source = QString::fromUtf8("layer 🎨 東京");
    const std::string utf8 = toStdString(toStringView(source));

    EXPECT_EQ(toQString(std::string_view(utf8)), source);
    EXPECT_NE(utf8.find("🎨"), std::string::npos);
}

TEST(StringViewContractTest, ConversionsRespectNonNullTerminatedSubstringBounds)
{
    const QString source = QStringLiteral("prefix-target-suffix");
    const QStringView slice = QStringView(source).sliced(7, 6);

    EXPECT_EQ(toQString(slice), QStringLiteral("target"));
    EXPECT_EQ(toStdString(slice), "target");
}

TEST(StringViewContractTest, SubstringConversionPreservesSupplementaryCharacterPair)
{
    const QString source = QStringLiteral("x🎨y");
    const QStringView emoji = QStringView(source).sliced(1, 2);

    EXPECT_EQ(toQString(emoji), QStringLiteral("🎨"));
    EXPECT_EQ(toStdString(emoji), "🎨");
}

TEST(StringViewContractTest, Utf8ViewConversionRetainsEmbeddedNull)
{
    const QString source = QString::fromUtf8("a\0b", 3);
    const std::string_view utf8 = toStdStringView(toStringView(source));
    EXPECT_EQ(utf8.size(), 3u);
    EXPECT_EQ(utf8[1], '\0');
    EXPECT_EQ(std::string(utf8), std::string("a\0b", 3));
}

TEST(StringViewContractTest, OwnedUtf8ConversionRetainsEmbeddedNull)
{
    const QString source = QString::fromUtf8("a\0b", 3);
    const std::string converted = toStdString(toStringView(source));

    EXPECT_EQ(converted.size(), 3u);
    EXPECT_EQ(converted, std::string("a\0b", 3));
}

TEST(StringViewContractTest, Utf8StringViewToQStringRetainsEmbeddedNull)
{
    const std::string_view utf8("a\0b", 3);
    const QString converted = toQString(utf8);

    ASSERT_EQ(converted.size(), 3);
    EXPECT_EQ(converted, QString::fromUtf8("a\0b", 3));
    EXPECT_EQ(converted.at(1).unicode(), QChar(u'\0').unicode());
}

TEST(StringViewContractTest, Utf8ConversionPreservesNullAndEmptyViewStates)
{
    const std::string_view nullView;
    const std::string_view emptyView("", 0);

    const QString convertedNull = toQString(nullView);
    const QString convertedEmpty = toQString(emptyView);

    EXPECT_TRUE(convertedNull.isNull());
    EXPECT_TRUE(convertedEmpty.isEmpty());
    EXPECT_FALSE(convertedEmpty.isNull());
}

TEST(StringViewContractTest, Utf8ViewStorageIsIsolatedPerThread)
{
    const std::string_view mainView =
        toStdStringView(toStringView(QStringLiteral("main")));
    std::promise<std::string> workerResult;
    auto result = workerResult.get_future();
    std::thread worker([&workerResult] {
        const std::string_view workerView =
            toStdStringView(toStringView(QStringLiteral("worker")));
        workerResult.set_value(std::string(workerView));
    });
    worker.join();

    EXPECT_EQ(result.get(), "worker");
    EXPECT_EQ(mainView, "main");
}

TEST(StringViewContractTest, PrefixSuffixAndContainmentUseQStringViewSemantics)
{
    const QString source = QStringLiteral("alpha-beta");
    const auto view = toStringView(source);
    EXPECT_TRUE(startsWith(view, QStringView(u"alpha")));
    EXPECT_TRUE(endsWith(view, QStringView(u"beta")));
    EXPECT_TRUE(contains(view, QStringView(u"ha-be")));
    EXPECT_FALSE(contains(view, QStringView(u"gamma")));
}

TEST(StringViewContractTest, EmptyPrefixSuffixAndNeedleFollowQStringViewSemantics)
{
    const QString source = QStringLiteral("value");
    const auto view = toStringView(source);
    const QStringView empty;

    EXPECT_TRUE(startsWith(view, empty));
    EXPECT_TRUE(endsWith(view, empty));
    EXPECT_TRUE(contains(view, empty));
    EXPECT_TRUE(startsWith(empty, empty));
    EXPECT_TRUE(endsWith(empty, empty));
    EXPECT_TRUE(contains(empty, empty));
}

TEST(StringViewContractTest, EmptyAndNullViewsHaveDistinctNullState)
{
    const QString empty = QStringLiteral("");
    const QString null;
    EXPECT_TRUE(isEmpty(toStringView(empty)));
    EXPECT_FALSE(isNullOrEmpty(toStringView(empty)));
    EXPECT_TRUE(isNullOrEmpty(toStringView(null)));
    EXPECT_TRUE(isNotEmpty(toStringView(QStringLiteral("x"))));
}

TEST(StringViewContractTest, QStringConversionPreservesNullAndEmptyViewStates)
{
    const QString nullString;
    const QString emptyString = QStringLiteral("");
    const QString convertedNull = toQString(toStringView(nullString));
    const QString convertedEmpty = toQString(toStringView(emptyString));

    EXPECT_TRUE(convertedNull.isNull());
    EXPECT_TRUE(convertedEmpty.isEmpty());
    EXPECT_FALSE(convertedEmpty.isNull());
}

TEST(StringViewContractTest, BlankChecksTrimUnicodeWhitespace)
{
    EXPECT_TRUE(isBlank(QStringView(u" \t\n")));
    EXPECT_FALSE(isBlank(QStringView(u" x ")));
    EXPECT_TRUE(isNotBlank(QStringView(u"x")));
}

TEST(StringViewContractTest, BlankChecksNonAsciiUnicodeSpaceCharacters)
{
    EXPECT_TRUE(isBlank(QStringView(u"\u00a0\u2003\u3000")));
    EXPECT_FALSE(isBlank(QStringView(u"\u3000x\u2003")));
}

