#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <variant>

import Script.ArtifactScript;

using namespace ArtifactCore;

TEST(LayerScriptComponentContractTest,
     RunsLifecycleHooksAndPreservesFieldsAcrossFrames) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class FrameCounter : ArtifactBehaviour
{
    public float order = 0.0;
    public float elapsed = 0.0;
    public float observedTime = 0.0;
    public int observedFrame = 0;

    void OnCreate() { order = order * 10.0 + 1.0; }
    void OnStart() { order = order * 10.0 + 2.0; }
    void OnEnable() { order = order * 10.0 + 3.0; }
    void OnUpdate(float dt)
    {
        order = order * 10.0 + 4.0;
        elapsed = elapsed + dt;
        observedTime = time;
        observedFrame = frame;
    }
    void OnDisable() { order = order * 10.0 + 5.0; }
    void OnDestroy() { order = order * 10.0 + 6.0; }
}
)");

    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));

    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate));
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnCreate));
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("order")), 1.0);

    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnStart));
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnStart));
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnEnable));
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnEnable));

    instance.fields()["dt"] = 0.25;
    instance.fields()["time"] = 2.5;
    instance.fields()["frame"] = std::int64_t{75};
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnUpdate));
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("elapsed")), 0.25);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("observedTime")), 2.5);
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("observedFrame")), 75);

    instance.fields()["dt"] = 0.5;
    instance.fields()["time"] = 3.0;
    instance.fields()["frame"] = std::int64_t{90};
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("elapsed")), 0.75);
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("observedTime")), 3.0);
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("observedFrame")), 90);

    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnDisable));
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnDisable));
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnDestroy));
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnDestroy));
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("order")), 1234456.0);
    EXPECT_TRUE(instance.lastError().empty());
}

TEST(LayerScriptComponentContractTest,
     AcceptsEmptyHooksAndCommentsInCustomScripts) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class CommentedCounter : ArtifactBehaviour
{
    public float value = 0.0;

    void OnCreate() { /* Empty hooks remain callable. */ }
    void OnUpdate()
    {
        // Script comments are ignored by the method-body parser.
        value = value + 1.0;
    }
}
)");

    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate));
    EXPECT_TRUE(instance.wasHookInvoked(ArtifactScriptHook::OnCreate));

    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("value")), 2.0);
    EXPECT_TRUE(instance.lastError().empty());
}
