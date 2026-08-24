#include <gtest/gtest.h>
#include <QString>
#include <QVariant>

#include <functional>

import EnvironmentVariable;
import EnvironmentVariable.Expansion;

using namespace ArtifactCore;

namespace {

ExpansionContext makeContext(double frame = 0.0, double fps = 30.0)
{
    ExpansionContext ctx;
    ctx.frame = frame;
    ctx.fps = fps;
    return ctx;
}

class CustomResolverFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto* manager = EnvironmentVariableManager::instance();
        manager->setVariable(QStringLiteral("TEST_HOME"), QStringLiteral("/home/tester"));
        manager->setVariable(QStringLiteral("EMPTY_VAR"), QString());
    }
};

} // namespace

TEST(TokenExpansionTest, NoMarkerReturnsInputUnchanged)
{
    EXPECT_EQ(expandTokens(QStringLiteral("plain/path.png"),
                           makeContext()),
              QStringLiteral("plain/path.png"));
}

TEST(TokenExpansionTest, DollarNameExpandsFromManager)
{
    EXPECT_EQ(expandTokens(QStringLiteral("$TEST_HOME/img.png"),
                           makeContext()),
              QStringLiteral("/home/tester/img.png"));
}

TEST(TokenExpansionTest, BraceFormExpandsFromManager)
{
    EXPECT_EQ(expandTokens(QStringLiteral("${TEST_HOME}/img.png"),
                           makeContext()),
              QStringLiteral("/home/tester/img.png"));
}

TEST(TokenExpansionTest, UnresolvedTokenStaysLiteral)
{
    EXPECT_EQ(expandTokens(QStringLiteral("$DOES_NOT_EXIST_XYZ/img.png"),
                           makeContext()),
              QStringLiteral("$DOES_NOT_EXIST_XYZ/img.png"));
    EXPECT_EQ(expandTokens(QStringLiteral("${DOES_NOT_EXIST_XYZ}/img.png"),
                           makeContext()),
              QStringLiteral("${DOES_NOT_EXIST_XYZ}/img.png"));
}

TEST(TokenExpansionTest, DollarEscapeKeepsSingleDollar)
{
    EXPECT_EQ(expandTokens(QStringLiteral("$$home"), makeContext()),
              QStringLiteral("$home"));
}

TEST(TokenExpansionTest, FramePadding)
{
    EXPECT_EQ(expandTokens(QStringLiteral("seq.$F4.png"),
                           makeContext(7.0)),
              QStringLiteral("seq.0007.png"));
    EXPECT_EQ(expandTokens(QStringLiteral("seq.$F2.png"),
                           makeContext(123.0)),
              QStringLiteral("seq.23.png"));
    EXPECT_EQ(expandTokens(QStringLiteral("seq.$F.png"),
                           makeContext(5.0)),
              QStringLiteral("seq.5.png"));
}

TEST(TokenExpansionTest, FrameNamedVariableNotConfusedWithFrame)
{
    // $FRAME はフレームトークンではなく変数として扱う
    auto* manager = EnvironmentVariableManager::instance();
    manager->setVariable(QStringLiteral("FRAME"), QStringLiteral("VARIABLE_VALUE"));
    EXPECT_EQ(expandTokens(QStringLiteral("$FRAME"),
                           makeContext(9.0)),
              QStringLiteral("VARIABLE_VALUE"));
}

TEST(TokenExpansionTest, CustomResolverTakesPrecedence)
{
    ExpansionContext ctx = makeContext();
    ctx.customResolver = [](const QString& name) -> QString {
        if (name == QStringLiteral("LOCAL")) {
            return QStringLiteral("custom/value");
        }
        return QString(); // null = 未解決 → マネージャへフォールバック
    };

    EXPECT_EQ(expandTokens(QStringLiteral("$LOCAL"), ctx),
              QStringLiteral("custom/value"));
    // 未解決時はマネージャへフォールバックする
    EXPECT_EQ(expandTokens(QStringLiteral("$TEST_HOME"), ctx),
              QStringLiteral("/home/tester"));
}

TEST(TokenExpansionTest, EmptyVariableValueResolvesToEmptyString)
{
    // 空文字列は「解決されたが空」。未解決 (元のまま) とは区別される。
    EXPECT_EQ(expandTokens(QStringLiteral("a/${EMPTY_VAR}/b"), makeContext()),
              QStringLiteral("a//b"));
}

TEST(TokenExpansionTest, MixedTokensInOneString)
{
    EXPECT_EQ(expandTokens(QStringLiteral("$TEST_HOME/seq_${EMPTY_VAR}x_$F3.$$bak"),
                           makeContext(12.0)),
              QStringLiteral("/home/tester/seq/_x_012.$bak"));
}

TEST(TokenExpansionTest, TrailingDollarStaysLiteral)
{
    EXPECT_EQ(expandTokens(QStringLiteral("path$"), makeContext()),
              QStringLiteral("path$"));
}

TEST(TokenExpansionTest, UnclosedBraceStaysLiteral)
{
    EXPECT_EQ(expandTokens(QStringLiteral("path${OPEN"), makeContext()),
              QStringLiteral("path${OPEN"));
}
