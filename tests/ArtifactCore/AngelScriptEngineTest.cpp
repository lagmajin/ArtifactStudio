// AngelScript hello-world test.
// Only compiled when AngelScript SDK is available (ARTIFACT_HAS_ANGELSCRIPT=1).
#ifdef ARTIFACT_HAS_ANGELSCRIPT

#include <gtest/gtest.h>
#include <string>

import Script.AngelScript.Engine;

using namespace ArtifactCore;

// ============================================================================
// Helper: capture log output into a string
// ============================================================================
static std::string g_capturedOutput;

static void captureOutput(const std::string& text, bool isError) {
    g_capturedOutput += text;
    (void)isError;
}

// ============================================================================
// Tests
// ============================================================================

TEST(AngelScriptEngineTest, InitializeAndFinalize) {
    auto& engine = AngelScriptEngine::instance();

    // Should succeed on first call.
    ASSERT_TRUE(engine.initialize());
    EXPECT_TRUE(engine.isInitialized());

    // Second call is a no-op (idempotent).
    EXPECT_TRUE(engine.initialize());

    // Clean up after all tests in this suite.
    // Note: finalize() is also called by the singleton destructor, so this
    // is optional, but explicit here for clarity.
}

TEST(AngelScriptEngineTest, CompileAndRunHelloWorld) {
    auto& engine = AngelScriptEngine::instance();
    ASSERT_TRUE(engine.isInitialized());

    g_capturedOutput.clear();
    engine.setOutputCallback(captureOutput);

    const char* source = R"(
void main() {
    log("hello from AngelScript");
    print("world");
}
)";

    ASSERT_TRUE(engine.compileModule("test_hello", source));
    EXPECT_FALSE(engine.hasError()) << engine.getLastError();

    ASSERT_TRUE(engine.runMain("test_hello"));
    EXPECT_FALSE(engine.hasError()) << engine.getLastError();

    // Verify captured output (each call appends a newline).
    EXPECT_EQ(g_capturedOutput, "hello from AngelScript\nworld");

    // Clean up the module.
    engine.discardModule("test_hello");
}

TEST(AngelScriptEngineTest, CompileErrorCaptured) {
    auto& engine = AngelScriptEngine::instance();
    ASSERT_TRUE(engine.isInitialized());

    engine.clearError();

    // Syntax error: missing closing brace.
    const char* badSource = R"(
void main() {
    log("missing brace"
}
)";

    EXPECT_FALSE(engine.compileModule("test_error", badSource));
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.getLastError().empty());
}

TEST(AngelScriptEngineTest, RunNonexistentModule) {
    auto& engine = AngelScriptEngine::instance();
    ASSERT_TRUE(engine.isInitialized();

    engine.clearError();

    EXPECT_FALSE(engine.runMain("nonexistent_module"));
    EXPECT_TRUE(engine.hasError());
}

TEST(AngelScriptEngineTest, RunModuleWithoutMain) {
    auto& engine = AngelScriptEngine::instance();
    ASSERT_TRUE(engine.isInitialized());

    engine.clearError();

    const char* source = R"(
void somethingElse() {
    log("not main");
}
)";

    ASSERT_TRUE(engine.compileModule("test_no_main", source));
    EXPECT_FALSE(engine.runMain("test_no_main")); // no void main() declared
    EXPECT_TRUE(engine.hasError());

    engine.discardModule("test_no_main");
}

#endif // ARTIFACT_HAS_ANGELSCRIPT
