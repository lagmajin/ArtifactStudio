#include <gtest/gtest.h>

#include <string>

import Script.CSharp.Engine;

using namespace ArtifactCore;

TEST(CSharpScriptEngineTest, SingletonIsIdempotent)
{
    auto& engine = CSharpScriptEngine::instance();
    engine.clearError();
    EXPECT_FALSE(engine.hasError());
    EXPECT_EQ(&engine, &CSharpScriptEngine::instance());
}

TEST(CSharpScriptEngineTest, MissingScriptReportsError)
{
    auto& engine = CSharpScriptEngine::instance();
    engine.clearError();
    EXPECT_FALSE(engine.executeScriptFile("missing-artifact-script.csx"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.getLastError().empty());
}
