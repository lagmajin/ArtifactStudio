#include <gtest/gtest.h>

#include <QDate>
#include <QLocale>
#include <QString>

import Localization.LocaleFormatting;

using ArtifactCore::LocaleFormatting;

TEST(LocaleFormattingContractTest, NumberAndPercentageUseExplicitLocaleAndPrecision)
{
    const QLocale german(QLocale::German, QLocale::Germany);
    EXPECT_EQ(LocaleFormatting::formatNumber(1234.5, 2, german), QStringLiteral("1.234,50"));
    EXPECT_EQ(LocaleFormatting::formatNumber(1.0 / 3.0, 3, QLocale::c()),
              QStringLiteral("0.333"));
    EXPECT_EQ(LocaleFormatting::formatPercentage(0.125, 1, german),
              QStringLiteral("12,5 %"));
}

TEST(LocaleFormattingContractTest, FileSizeUsesBinaryUnitsAndLocaleDecimalSeparator)
{
    const QLocale german(QLocale::German, QLocale::Germany);
    EXPECT_EQ(LocaleFormatting::formatFileSize(0, german), QStringLiteral("0 B"));
    EXPECT_EQ(LocaleFormatting::formatFileSize(1023, german), QStringLiteral("1.023 B"));
    EXPECT_EQ(LocaleFormatting::formatFileSize(1024, german), QStringLiteral("1,00 KiB"));
    EXPECT_EQ(LocaleFormatting::formatFileSize(10 * 1024, german), QStringLiteral("10,0 KiB"));
    EXPECT_EQ(LocaleFormatting::formatFileSize(1024LL * 1024, german), QStringLiteral("1,00 MiB"));
}

TEST(LocaleFormattingContractTest, FileSizeContinuesThroughTebibytesAndClampsAtLargestUnit)
{
    EXPECT_EQ(LocaleFormatting::formatFileSize(1024LL * 1024 * 1024 * 1024, QLocale::c()),
              QStringLiteral("1.00 TiB"));
    EXPECT_EQ(LocaleFormatting::formatFileSize(1024LL * 1024 * 1024 * 1024 * 1024, QLocale::c()),
              QStringLiteral("1024.0 TiB"));
    EXPECT_EQ(LocaleFormatting::formatFileSize(-1, QLocale::c()), QStringLiteral("-1 B"));
}

TEST(LocaleFormattingContractTest, FrameFormattingIsStableAndPreservesSignedFrameNumber)
{
    const QLocale arabic(QLocale::Arabic, QLocale::Egypt);
    EXPECT_EQ(LocaleFormatting::formatFrame(123456, arabic), QStringLiteral("123456"));
    EXPECT_EQ(LocaleFormatting::formatFrame(-42, arabic), QStringLiteral("-42"));
}

TEST(LocaleFormattingContractTest, TimecodeRoundsFramesAndCarriesAcrossClockBoundaries)
{
    EXPECT_EQ(LocaleFormatting::formatTimecode(59.99, 30.0), QStringLiteral("00:01:00:00"));
    EXPECT_EQ(LocaleFormatting::formatTimecode(3599.99, 30.0), QStringLiteral("01:00:00:00"));
    EXPECT_EQ(LocaleFormatting::formatTimecode(1.0 + 15.0 / 30.0, 30.0),
              QStringLiteral("00:00:01:15"));
}

TEST(LocaleFormattingContractTest, TimecodeClampsNegativeTimeAndRejectsNonpositiveRates)
{
    EXPECT_EQ(LocaleFormatting::formatTimecode(-5.0, 24.0), QStringLiteral("00:00:00:00"));
    EXPECT_EQ(LocaleFormatting::formatTimecode(10.0, 0.0), QStringLiteral("00:00:00:00"));
    EXPECT_EQ(LocaleFormatting::formatTimecode(10.0, -24.0), QStringLiteral("00:00:00:00"));
}

TEST(LocaleFormattingContractTest, DurationFloorsToWholeSecondsAndClampsNegativeInput)
{
    EXPECT_EQ(LocaleFormatting::formatDuration(0), QStringLiteral("00:00:00"));
    EXPECT_EQ(LocaleFormatting::formatDuration(1999), QStringLiteral("00:00:01"));
    EXPECT_EQ(LocaleFormatting::formatDuration(3'661'000), QStringLiteral("01:01:01"));
    EXPECT_EQ(LocaleFormatting::formatDuration(-1), QStringLiteral("00:00:00"));
}

TEST(LocaleFormattingContractTest, DateFormattingUsesRequestedLocale)
{
    const QDate date(2026, 10, 29);
    EXPECT_EQ(LocaleFormatting::formatDate(date, QLocale::c()),
              QLocale::c().toString(date, QLocale::ShortFormat));
    EXPECT_NE(LocaleFormatting::formatDate(date, QLocale(QLocale::German, QLocale::Germany)),
              LocaleFormatting::formatDate(date, QLocale(QLocale::English, QLocale::UnitedStates)));
}
