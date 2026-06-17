// AngelScript behaviour-style lifecycle test.
// Only compiled when AngelScript SDK is available (ARTIFACT_HAS_ANGELSCRIPT=1).
#ifdef ARTIFACT_HAS_ANGELSCRIPT

#include <gtest/gtest.h>
#include <string>

import Script.AngelScript.Behaviour;
import Script.AngelScript.Engine;

using namespace ArtifactCore;

static std::string g_behaviourOutput;

static void captureBehaviourOutput(const std::string& text, bool isError) {
    g_behaviourOutput += text;
    (void)isError;
}

TEST(AngelScriptBehaviourTest, StartUpdateDestroyFlow) {
    auto& engine = AngelScriptEngine::instance();
    ASSERT_TRUE(engine.initialize());
    engine.setOutputCallback(captureBehaviourOutput);

    AngelScriptBehaviour behaviour;
    const char* source = R"(
void Start() {
    log("start");
}

void Update(float dt) {
    if (dt >= 0.0f) {
    log("update");
    }
}

void OnDestroy() {
    log("destroy");
}
)";

    ASSERT_TRUE(behaviour.load("behaviour_flow", source));
    ASSERT_TRUE(behaviour.start());
    ASSERT_TRUE(behaviour.update(0.5f));
    ASSERT_TRUE(behaviour.destroy());

    EXPECT_EQ(g_behaviourOutput, "start\nupdate\ndestroy\n");
    engine.discardModule("behaviour_flow");
}

#endif // ARTIFACT_HAS_ANGELSCRIPT
