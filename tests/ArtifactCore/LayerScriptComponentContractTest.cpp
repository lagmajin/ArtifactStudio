#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <atomic>
#include <crtdbg.h>
#endif

import Script.ArtifactScript;

using namespace ArtifactCore;

#if defined(_MSC_VER) && defined(_DEBUG)
namespace {

std::atomic_size_t g_scriptAllocationCount{0};
std::atomic_size_t g_scriptAllocatedBytes{0};

int __cdecl countScriptAllocation(int allocationType, void*, std::size_t size,
                                  int, long, unsigned char const*, int) {
    if (allocationType == _HOOK_ALLOC || allocationType == _HOOK_REALLOC) {
        g_scriptAllocationCount.fetch_add(1, std::memory_order_relaxed);
        g_scriptAllocatedBytes.fetch_add(size, std::memory_order_relaxed);
    }
    return 1;
}

class ScriptAllocationCounter {
public:
    ScriptAllocationCounter() {
        g_scriptAllocationCount.store(0, std::memory_order_relaxed);
        g_scriptAllocatedBytes.store(0, std::memory_order_relaxed);
        previousHook_ = _CrtSetAllocHook(countScriptAllocation);
    }

    ~ScriptAllocationCounter() { stop(); }

    std::pair<std::size_t, std::size_t> stop() {
        if (active_) {
            _CrtSetAllocHook(previousHook_);
            active_ = false;
        }
        return {
            g_scriptAllocationCount.load(std::memory_order_relaxed),
            g_scriptAllocatedBytes.load(std::memory_order_relaxed)};
    }

private:
    _CRT_ALLOC_HOOK previousHook_ = nullptr;
    bool active_ = true;
};

}  // namespace
#endif

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

    auto foreachDefinition = parser.parse(R"(
class BenchmarkForeachCounter : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
    void OnCreate()
    {
        push(values, 1.0); push(values, 2.0); push(values, 3.0); push(values, 4.0);
        push(values, 5.0); push(values, 6.0); push(values, 7.0); push(values, 8.0);
    }
    void OnUpdate()
    {
        total = 0.0;
        foreach (item in values) { total += item; }
    }
}
)");
    ASSERT_TRUE(foreachDefinition.diagnostics.empty());
    ArtifactScriptInstance foreachInstance(std::move(foreachDefinition));
    ASSERT_TRUE(foreachInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << foreachInstance.lastError();
    for (int i = 0; i < warmupIterations; ++i) {
        if (!foreachInstance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            FAIL() << foreachInstance.lastError();
        }
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            if (!foreachInstance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                FAIL() << foreachInstance.lastError();
            }
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript foreach(8) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";

    std::string wideFieldsSource = R"(
class BenchmarkWideForeachCounter : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
)";
    for (int i = 0; i < 48; ++i) {
        wideFieldsSource += "    public float unused" + std::to_string(i) + " = 0.0;\n";
    }
    wideFieldsSource += R"(
    void OnCreate()
    {
        push(values, 1.0); push(values, 2.0); push(values, 3.0); push(values, 4.0);
        push(values, 5.0); push(values, 6.0); push(values, 7.0); push(values, 8.0);
    }
    void OnUpdate()
    {
        total = 0.0;
        foreach (item in values) { total += item; }
    }
}
)";
    auto wideFieldsDefinition = parser.parse(wideFieldsSource);
    ASSERT_TRUE(wideFieldsDefinition.diagnostics.empty());
    ArtifactScriptInstance wideFieldsInstance(std::move(wideFieldsDefinition));
    ASSERT_TRUE(wideFieldsInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << wideFieldsInstance.lastError();
    for (int i = 0; i < warmupIterations; ++i) {
        if (!wideFieldsInstance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            FAIL() << wideFieldsInstance.lastError();
        }
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            if (!wideFieldsInstance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                FAIL() << wideFieldsInstance.lastError();
            }
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript foreach(8, 50 fields) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";

#if defined(_MSC_VER) && defined(_DEBUG)
    auto noOpDefinition = parser.parse(R"(
class BenchmarkNoOp : ArtifactBehaviour
{
    public float value = 0.0;
    void OnUpdate() { }
}
)");
    ASSERT_TRUE(noOpDefinition.diagnostics.empty());
    ArtifactScriptInstance noOpInstance(std::move(noOpDefinition));
    noOpInstance.fields()["value"] = 0.0;
    const ArtifactScriptMethod* noOpMethod = nullptr;
    for (const auto& method : noOpInstance.definition().rootClass.methods) {
        if (method.name == "OnUpdate") noOpMethod = &method;
    }
    ASSERT_NE(noOpMethod, nullptr);

    auto largeForeachDefinition = parser.parse(R"(
class BenchmarkLargeForeach : ArtifactBehaviour
{
    public Array values;
    public float total = 0.0;
    void OnCreate()
    {
        for (int index = 0; index < 257; index += 1) push(values, index);
    }
    void OnUpdate()
    {
        total = 0.0;
        foreach (item in values) total += item;
    }
}
)");
    ASSERT_TRUE(largeForeachDefinition.diagnostics.empty());
    ArtifactScriptInstance largeForeachInstance(std::move(largeForeachDefinition));
    ASSERT_TRUE(largeForeachInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << largeForeachInstance.lastError();
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(largeForeachInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << largeForeachInstance.lastError();
    }

    auto stringForeachDefinition = parser.parse(R"(
class BenchmarkStringForeach : ArtifactBehaviour
{
    public Array values;
    void OnCreate()
    {
        push(values, "this string is longer than the small string buffer");
        push(values, "another long string to expose copy allocations");
    }
    void OnUpdate()
    {
        foreach (item in values) { }
    }
}
)");
    ASSERT_TRUE(stringForeachDefinition.diagnostics.empty());
    ArtifactScriptInstance stringForeachInstance(std::move(stringForeachDefinition));
    ASSERT_TRUE(stringForeachInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << stringForeachInstance.lastError();
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(stringForeachInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << stringForeachInstance.lastError();
    }

    auto wideLocalsDefinition = parser.parse(R"(
class BenchmarkWideLocals : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        float a0 = 0.0; float a1 = 1.0; float a2 = 2.0; float a3 = 3.0;
        float a4 = 4.0; float a5 = 5.0; float a6 = 6.0; float a7 = 7.0;
        float a8 = 8.0; float a9 = 9.0; float a10 = 10.0; float a11 = 11.0;
        total = a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11;
    }
}
)");
    ASSERT_TRUE(wideLocalsDefinition.diagnostics.empty());
    ArtifactScriptInstance wideLocalsInstance(std::move(wideLocalsDefinition));
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(wideLocalsInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << wideLocalsInstance.lastError();
    }
    const std::vector<ArtifactScriptValue> noArguments;
    constexpr int allocationIterations = 2000;
    const auto countCalls = [&](auto&& call) {
        ScriptAllocationCounter counter;
        bool succeeded = true;
        for (int i = 0; i < allocationIterations; ++i) {
            if (!call()) {
                succeeded = false;
                break;
            }
        }
        const auto totals = counter.stop();
        EXPECT_TRUE(succeeded);
        return std::pair<double, double>{
            static_cast<double>(totals.first) / allocationIterations,
            static_cast<double>(totals.second) / allocationIterations};
    };
    const auto lookupAllocationRate = countCalls([&] {
        return noOpInstance.hasHook(ArtifactScriptHook::OnUpdate);
    });
    std::cout << "ArtifactScript allocation split (count, bytes/hook): lookup="
              << lookupAllocationRate.first << ", " << lookupAllocationRate.second
              << "; direct-body=";
    ArtifactScriptEvaluator directEvaluator;
    ArtifactScriptSerializedFields directFields;
    directFields["value"] = 0.0;
    const auto bodyAllocationRate = countCalls([&] {
        return directEvaluator.execute(*noOpMethod->body, noArguments, directFields);
    });
    std::cout << bodyAllocationRate.first << ", " << bodyAllocationRate.second
              << '\n';

    const auto countAllocations = [&](ArtifactScriptInstance& target) {
        std::string error;
        const auto result = countCalls([&] {
            const bool succeeded = target.invokeHook(ArtifactScriptHook::OnUpdate);
            if (!succeeded) error = target.lastError();
            return succeeded;
        });
        EXPECT_TRUE(error.empty()) << error;
        return result;
    };
    const auto noOpAllocationRate = countAllocations(noOpInstance);
    const auto simpleAllocationRate = countAllocations(instance);
    const auto methodAllocationRate = countAllocations(callInstance);
    const auto foreachAllocationRate = countAllocations(foreachInstance);
    const auto wideAllocationRate = countAllocations(wideFieldsInstance);
    const auto largeForeachAllocationRate = countAllocations(largeForeachInstance);
    const auto stringForeachAllocationRate = countAllocations(stringForeachInstance);
    const auto wideLocalsAllocationRate = countAllocations(wideLocalsInstance);
    const auto expectNoSteadyStateAllocations = [](const auto& rate) {
        EXPECT_DOUBLE_EQ(rate.first, 0.0);
        EXPECT_DOUBLE_EQ(rate.second, 0.0);
    };
    expectNoSteadyStateAllocations(noOpAllocationRate);
    expectNoSteadyStateAllocations(simpleAllocationRate);
    expectNoSteadyStateAllocations(methodAllocationRate);
    expectNoSteadyStateAllocations(foreachAllocationRate);
    expectNoSteadyStateAllocations(wideAllocationRate);
    expectNoSteadyStateAllocations(largeForeachAllocationRate);
    expectNoSteadyStateAllocations(wideLocalsAllocationRate);
    EXPECT_LE(stringForeachAllocationRate.first, 3.0);
    EXPECT_LE(stringForeachAllocationRate.second, 96.0);
    std::cout << "ArtifactScript allocations/hook (count, bytes): no-op="
              << noOpAllocationRate.first << ", " << noOpAllocationRate.second
              << "; simple="
              << simpleAllocationRate.first << ", " << simpleAllocationRate.second
              << "; method=" << methodAllocationRate.first << ", "
              << methodAllocationRate.second << "; foreach="
              << foreachAllocationRate.first << ", " << foreachAllocationRate.second
              << "; wide-foreach=" << wideAllocationRate.first << ", "
              << wideAllocationRate.second << "; large-foreach="
              << largeForeachAllocationRate.first << ", "
              << largeForeachAllocationRate.second << "; string-foreach="
              << stringForeachAllocationRate.first << ", "
              << stringForeachAllocationRate.second << "; wide-locals="
              << wideLocalsAllocationRate.first << ", "
              << wideLocalsAllocationRate.second << '\n';
#endif
}
