#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <iostream>
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

TEST(LayerScriptComponentContractTest, ReusedEvaluatorClearsPriorHookError) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class RecoveringScript : ArtifactBehaviour
{
    public float value = 0.0;
    void OnUpdate() { value = lateBound; }
}
)");

    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    EXPECT_FALSE(instance.invokeHook(ArtifactScriptHook::OnUpdate));
    EXPECT_FALSE(instance.lastError().empty());

    instance.fields()["lateBound"] = 1.0;
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_TRUE(instance.lastError().empty());
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("value")), 1.0);
}

TEST(LayerScriptComponentContractTest, DerivedLifecycleHookOverridesBaseHook) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class DerivedCounter : BaseCounter
{
    public float value = 0.0;
    void OnUpdate() { value += 1.0; }
}
class BaseCounter : ArtifactBehaviour
{
    void OnUpdate() { value += 10.0; }
}
)");

    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.hasHook(ArtifactScriptHook::OnUpdate));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("value")), 1.0);
}

TEST(LayerScriptComponentContractTest,
     RestoresSerializedComponentRunsHooksAndSerializesRuntimeState) {
    constexpr auto source = R"(
class PersistedCounter : ArtifactBehaviour
{
    public float count = 1.0;
    public int observedFrame = 0;
    [SerializeField]
    private float savedCount = 3.0;
    private float transient = 99.0;

    void OnCreate() { count += 1.0; }
    void OnUpdate()
    {
        count += dt;
        savedCount += 1.0;
        observedFrame = frame;
    }
}
)";

    ArtifactScriptParser parser;
    auto componentDefinition = parser.parse(source);
    ASSERT_TRUE(componentDefinition.diagnostics.empty());

    ArtifactScriptSerializedComponent saved;
    saved.className = "PersistedCounter";
    saved.values["count"] = 5.5;
    saved.values["observedFrame"] = std::int64_t{0};
    saved.values["savedCount"] = 8.5;
    saved.unknown["futureField"] = std::string("preserve");

    const auto encoded = serializeScriptComponent(saved);
    ArtifactScriptSerializedComponent decoded;
    std::string error;
    ASSERT_TRUE(deserializeScriptComponent(encoded, decoded, error)) << error;

    ArtifactScriptComponent component;
    component.applySerializedComponent(componentDefinition, decoded);
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("count")), 5.5);
    EXPECT_DOUBLE_EQ(std::get<double>(component.publicFields().at("savedCount")), 8.5);
    EXPECT_EQ(std::get<std::string>(component.publicFields().at("futureField")), "preserve");

    auto runtimeDefinition = parser.parse(source);
    ASSERT_TRUE(runtimeDefinition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(runtimeDefinition));
    instance.fields() = component.publicFields();
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    instance.fields()["dt"] = 0.75;
    instance.fields()["frame"] = std::int64_t{42};
    EXPECT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();

    component.publicFields() = instance.fields();
    ArtifactScriptSerializedComponent runtimeState;
    runtimeState.className = component.scriptClass();
    runtimeState.values = component.serializedFields(componentDefinition);
    const auto runtimeJson = serializeScriptComponent(runtimeState);
    ArtifactScriptSerializedComponent roundTripped;
    ASSERT_TRUE(deserializeScriptComponent(runtimeJson, roundTripped, error)) << error;
    EXPECT_DOUBLE_EQ(std::get<double>(roundTripped.values.at("count")), 7.25);
    EXPECT_EQ(std::get<std::int64_t>(roundTripped.values.at("observedFrame")), 42);
    EXPECT_DOUBLE_EQ(std::get<double>(roundTripped.values.at("savedCount")), 9.5);
    EXPECT_EQ(roundTripped.values.find("transient"), roundTripped.values.end());
}

TEST(LayerScriptComponentContractTest, HookExecutionMicrobenchmark) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class BenchmarkCounter : ArtifactBehaviour
{
    public float value = 0.0;
    void OnUpdate() { value += dt; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    instance.fields()["dt"] = 0.016;

    constexpr int repetitions = 3;
    constexpr int iterations = 20000;
    constexpr int warmupIterations = 2000;
    for (int i = 0; i < warmupIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            FAIL() << instance.lastError();
        }
    }
    double totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                FAIL() << instance.lastError();
            }
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript OnUpdate benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";

    auto callDefinition = parser.parse(R"(
class BenchmarkMethodCounter : ArtifactBehaviour
{
    public float value = 0.0;
    float combine(float a, float b, float c, float d)
    {
        float intermediate = a * 2.0;
        return intermediate + b + c + d;
    }
    void OnUpdate() { value = combine(value, dt, 1.0, 2.0); }
}
)");
    ASSERT_TRUE(callDefinition.diagnostics.empty());
    ArtifactScriptInstance callInstance(std::move(callDefinition));
    callInstance.fields()["value"] = 0.0;
    callInstance.fields()["dt"] = 0.016;
    for (int i = 0; i < warmupIterations; ++i) {
        if (!callInstance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            FAIL() << callInstance.lastError();
        }
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            if (!callInstance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                FAIL() << callInstance.lastError();
            }
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript method/local benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";
}
