#include <gtest/gtest.h>
#include <QString>
#include <QVariant>

#include <string>

import EnvironmentVariable;
import Script.Expression.Evaluator;

using namespace ArtifactCore;

namespace {

constexpr const char* kTestVar = "ARTIFACT_SCRIPT_ENV_TEST_VAR";

std::string expr(const char* source)
{
    return std::string(source);
}

void clearTestVar()
{
    EnvironmentVariableManager::instance()->unsetVariable(
        QString::fromStdString(kTestVar));
}

} // namespace

TEST(EnvManagerTest, UnsetRemovesVariable)
{
    auto* manager = EnvironmentVariableManager::instance();
    manager->setVariable(QStringLiteral("ARTIFACT_TMP_UNSET_ME"),
                         QStringLiteral("x"));
    EXPECT_TRUE(manager->hasVariable(QStringLiteral("ARTIFACT_TMP_UNSET_ME")));
    const quint64 before = manager->revision();
    EXPECT_TRUE(manager->unsetVariable(QStringLiteral("ARTIFACT_TMP_UNSET_ME")));
    EXPECT_FALSE(manager->hasVariable(QStringLiteral("ARTIFACT_TMP_UNSET_ME")));
    EXPECT_GT(manager->revision(), before);
}

TEST(EnvManagerTest, UnsetMissingReturnsFalseWithoutRevisionBump)
{
    auto* manager = EnvironmentVariableManager::instance();
    manager->unsetVariable(QStringLiteral("ARTIFACT_TMP_DEFINITELY_ABSENT"));
    const quint64 before = manager->revision();
    EXPECT_FALSE(manager->unsetVariable(QStringLiteral("ARTIFACT_TMP_DEFINITELY_ABSENT")));
    EXPECT_EQ(manager->revision(), before);
}

TEST(EnvScriptTest, HasEnvReflectsManager)
{
    clearTestVar();
    ExpressionEvaluator evaluator;
    EXPECT_DOUBLE_EQ(evaluator.evaluate(expr("hasEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\")")).asNumber(), 0.0);
    EnvironmentVariableManager::instance()->setVariable(
        QString::fromStdString(kTestVar), QStringLiteral("v"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(expr("hasEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\")")).asNumber(), 1.0);
    clearTestVar();
}

TEST(EnvScriptTest, GetEnvReturnsValueOrDefault)
{
    clearTestVar();
    ExpressionEvaluator evaluator;
    EnvironmentVariableManager::instance()->setVariable(
        QString::fromStdString(kTestVar), QStringLiteral("hello"));
    EXPECT_EQ(evaluator.evaluate(expr("getEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\")")).asZeroString(),
              ZeroString("hello"));
    EXPECT_EQ(evaluator.evaluate(expr("getEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\", \"fallback\")")).asZeroString(),
              ZeroString("hello"));
    clearTestVar();
    EXPECT_TRUE(evaluator.evaluate(expr("getEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\")")).isNull());
    EXPECT_EQ(evaluator.evaluate(expr("getEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\", \"fallback\")")).asZeroString(),
              ZeroString("fallback"));
}

TEST(EnvScriptTest, SetEnvWritesOverlayAndReadsBack)
{
    clearTestVar();
    ExpressionEvaluator evaluator;
    const ExpressionValue stored = evaluator.evaluate(expr("setEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\", \"from_script\")"));
    EXPECT_EQ(stored.asZeroString(), ZeroString("from_script"));
    EXPECT_TRUE(EnvironmentVariableManager::instance()->hasVariable(
        QString::fromStdString(kTestVar)));
    EXPECT_EQ(evaluator.evaluate(expr("getEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\")")).asZeroString(),
              ZeroString("from_script"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(expr("hasEnv(\"ARTIFACT_SCRIPT_ENV_TEST_VAR\")")).asNumber(), 1.0);
    clearTestVar();
}

TEST(EnvScriptTest, MissingNameYieldsNull)
{
    ExpressionEvaluator evaluator;
    EXPECT_TRUE(evaluator.evaluate(expr("getEnv()")).isNull());
    EXPECT_TRUE(evaluator.evaluate(expr("setEnv(\"only_name\")")).isNull());
    EXPECT_DOUBLE_EQ(evaluator.evaluate(expr("hasEnv()")).asNumber(), 0.0);
}
