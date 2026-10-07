#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
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

TEST(LayerScriptComponentContractTest,
     ArrayLiteralBuildsTheExpectedValuesAcrossRepeatedHooks) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ArrayLiteralCounter : ArtifactBehaviour
{
    public Array values;
    void OnUpdate() { values = [10.0, 20.0, 30.0, 40.0]; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    for (int i = 0; i < 32; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

#if defined(_MSC_VER) && defined(_DEBUG)
    constexpr std::size_t allocationIterations = 1000;
    {
        ScriptAllocationCounter allocations;
        for (std::size_t i = 0; i < allocationIterations; ++i) {
            ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
                << instance.lastError();
        }
        const auto [allocationCount, allocatedBytes] = allocations.stop();
        std::cout << "ArtifactScript four-element array literal: "
                  << allocationCount / allocationIterations
                  << " allocations/hook, "
                  << allocatedBytes / allocationIterations
                  << " bytes/hook\n";
        EXPECT_EQ(allocationCount, allocationIterations * 3);
        EXPECT_EQ(allocatedBytes, allocationIterations * 256);
    }
#endif

    constexpr std::size_t timingIterations = 50000;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < timingIterations; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }
    const auto elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "ArtifactScript four-element array literal: "
              << elapsed / timingIterations << " us/hook ("
              << timingIterations << " calls)\n";

    const auto& array = std::get<ArtifactScriptArrayPtr>(
        instance.fields().at("values"));
    ASSERT_NE(array, nullptr);
    ASSERT_EQ(array->values.size(), 4u);
    EXPECT_DOUBLE_EQ(std::get<double>(array->values[0]), 10.0);
    EXPECT_DOUBLE_EQ(std::get<double>(array->values[1]), 20.0);
    EXPECT_DOUBLE_EQ(std::get<double>(array->values[2]), 30.0);
    EXPECT_DOUBLE_EQ(std::get<double>(array->values[3]), 40.0);
}

TEST(LayerScriptComponentContractTest,
     SplitBuiltinRunsInsideRepeatedLayerHooks) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class SplitLayerPath : ArtifactBehaviour
{
    public string path = "/assets//layer/";
    public string roundTrip;
    public string normalized;
    public bool hasRepeatedSeparator;
    public Array segments;
    void OnUpdate()
    {
        segments = split(path, "/");
        roundTrip = join(segments, "/");
        normalized = replace(path, "//", "/");
        hasRepeatedSeparator = contains(path, "//");
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));

    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    const auto& segments = std::get<ArtifactScriptArrayPtr>(
        instance.fields().at("segments"));
    ASSERT_NE(segments, nullptr);
    ASSERT_EQ(segments->values.size(), 5u);
    EXPECT_TRUE(std::get<std::string>(segments->values[0]).empty());
    EXPECT_EQ(std::get<std::string>(segments->values[1]), "assets");
    EXPECT_TRUE(std::get<std::string>(segments->values[2]).empty());
    EXPECT_EQ(std::get<std::string>(segments->values[3]), "layer");
    EXPECT_TRUE(std::get<std::string>(segments->values[4]).empty());
    EXPECT_EQ(std::get<std::string>(instance.fields().at("roundTrip")),
              "/assets//layer/");
    EXPECT_EQ(std::get<std::string>(instance.fields().at("normalized")),
              "/assets/layer/");
    EXPECT_TRUE(std::get<bool>(instance.fields().at("hasRepeatedSeparator")));
    EXPECT_TRUE(instance.lastError().empty());
}

TEST(LayerScriptComponentContractTest,
     StringSearchBuiltinsAvoidCopyingFieldAndLocalOperands) {
    ArtifactScriptParser parser;
    const auto makeDefinition = [&](std::string_view className,
                                    bool forceStringCopy) {
        std::string source = "class ";
        source.append(className);
        source += R"( : ArtifactBehaviour
{
    public string source;
    public string query;
    public ObjectRef target;
    public bool fieldMatch;
    public bool localMatch;
    public bool localPrefix;
    public bool localSuffix;
    public bool objectPrefix;
    public bool objectSuffix;
    public int fieldIndex;
    public int localIndex;
    public int localLastIndex;
    public int objectLastIndex;
    public int localCount;
    public int objectCount;
    void OnCreate()
    {
        source = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxneedle";
        query = "needle";
        target = new StringContainsTarget(source);
    }
)";
        const std::string sourceExpression = forceStringCopy
            ? "source + \"\""
            : "source";
        const std::string update =
            "\n    void OnUpdate()\n    {\n"
            "        string localQuery = \"needle\";\n"
            "        string localPrefixQuery = \"x\";\n"
            "        fieldMatch = target.matches(query);\n"
            "        localMatch = contains(" + sourceExpression + ", localQuery);\n"
            "        localPrefix = startsWith(" + sourceExpression + ", localPrefixQuery);\n"
            "        localSuffix = endsWith(" + sourceExpression + ", localQuery);\n"
            "        fieldIndex = target.find(query);\n"
            "        localIndex = indexOf(" + sourceExpression + ", localQuery);\n"
            "        localLastIndex = lastIndexOf(" + sourceExpression + ", localQuery);\n"
            "        localCount = count(" + sourceExpression + ", \"x\");\n"
            "        objectPrefix = target.matchesPrefix(localPrefixQuery);\n"
            "        objectSuffix = target.matchesSuffix(query);\n"
            "        objectLastIndex = target.findLast(query);\n"
            "        objectCount = target.countMatches(\"x\");\n"
            "    }\n}\n";
        source += update;
        const std::string objectFieldExpression = forceStringCopy
            ? "this.value + \"\""
            : "this.value";
        source += "\nclass StringContainsTarget : ArtifactBehaviour\n{\n"
                  "    public string value;\n"
                  "    void OnConstruct(string input) { this.value = input; }\n"
                  "    bool matches(string query) { return contains(";
        source += objectFieldExpression;
        source += ", query); }\n"
                  "    bool matchesPrefix(string query) { return startsWith(";
        source += objectFieldExpression;
        source += ", query); }\n"
                  "    bool matchesSuffix(string query) { return endsWith(";
        source += objectFieldExpression;
        source += ", query); }\n"
                  "    int findLast(string query) { return lastIndexOf(";
        source += objectFieldExpression;
        source += ", query); }\n"
                  "    int countMatches(string query) { return count(";
        source += objectFieldExpression;
        source += ", query); }\n"
                  "    int find(string query) { return indexOf(";
        source += objectFieldExpression;
        source += ", query); }\n}\n";
        return parser.parse(source);
    };

    auto directDefinition = makeDefinition("StringContainsDirectProbe", false);
    auto copyDefinition = makeDefinition("StringContainsCopyProbe", true);
    ASSERT_TRUE(directDefinition.diagnostics.empty());
    ASSERT_TRUE(copyDefinition.diagnostics.empty());
    ArtifactScriptInstance direct(std::move(directDefinition));
    ArtifactScriptInstance copied(std::move(copyDefinition));
    for (auto* instance : {&direct, &copied}) {
        ASSERT_TRUE(instance->invokeHook(ArtifactScriptHook::OnCreate))
            << instance->lastError();
        ASSERT_TRUE(instance->invokeHook(ArtifactScriptHook::OnUpdate))
            << instance->lastError();
        EXPECT_TRUE(std::get<bool>(instance->fields().at("fieldMatch")));
        EXPECT_TRUE(std::get<bool>(instance->fields().at("localMatch")));
        EXPECT_TRUE(std::get<bool>(instance->fields().at("localPrefix")));
        EXPECT_TRUE(std::get<bool>(instance->fields().at("localSuffix")));
        EXPECT_TRUE(std::get<bool>(instance->fields().at("objectPrefix")));
        EXPECT_TRUE(std::get<bool>(instance->fields().at("objectSuffix")));
        EXPECT_EQ(std::get<std::int64_t>(instance->fields().at("fieldIndex")),
                  128);
        EXPECT_EQ(std::get<std::int64_t>(instance->fields().at("localIndex")),
                  128);
        EXPECT_EQ(std::get<std::int64_t>(
                      instance->fields().at("localLastIndex")), 128);
        EXPECT_EQ(std::get<std::int64_t>(
                      instance->fields().at("objectLastIndex")), 128);
        EXPECT_EQ(std::get<std::int64_t>(
                      instance->fields().at("localCount")), 128);
        EXPECT_EQ(std::get<std::int64_t>(
                      instance->fields().at("objectCount")), 128);
    }

#if defined(_MSC_VER) && defined(_DEBUG)
    constexpr std::size_t allocationIterations = 1000;
    const auto measure = [&](ArtifactScriptInstance& instance) {
        ScriptAllocationCounter counter;
        std::string failure;
        for (std::size_t i = 0; i < allocationIterations; ++i) {
            if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                failure = instance.lastError();
                break;
            }
        }
        const auto allocations = counter.stop();
        EXPECT_TRUE(failure.empty()) << failure;
        return allocations;
    };
    const auto [directAllocations, directBytes] = measure(direct);
    const auto [copyAllocations, copyBytes] = measure(copied);
    constexpr std::size_t timingIterations = 3000;
    constexpr std::size_t timingRepetitions = 3;
    const auto measureMicros = [&](ArtifactScriptInstance& instance) {
        std::array<double, timingRepetitions> samples{};
        for (auto& sample : samples) {
            const auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < timingIterations; ++i) {
                if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                    ADD_FAILURE() << instance.lastError();
                    break;
                }
            }
            sample = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - start).count() /
                timingIterations;
        }
        std::sort(samples.begin(), samples.end());
        return samples[samples.size() / 2];
    };
    const double directMicros = measureMicros(direct);
    const double copyMicros = measureMicros(copied);
    std::cout << "ArtifactScript string search(field/local, 134-byte source): "
              << directAllocations / allocationIterations << " / "
              << copyAllocations / allocationIterations
              << " allocations/hook, "
              << directBytes / allocationIterations << " / "
              << copyBytes / allocationIterations << " bytes/hook, "
              << directMicros << " / " << copyMicros
              << " us/hook (median of " << timingRepetitions << " x "
              << timingIterations << ")\n";
    EXPECT_LT(directAllocations, copyAllocations);
    EXPECT_LT(directBytes, copyBytes);
#endif
}

TEST(LayerScriptComponentContractTest,
     ArrayCountBuiltinMatchesForeachAndAvoidsInterpreterLoopWork) {
    ArtifactScriptParser parser;
    constexpr std::size_t itemCount = 64;
    constexpr std::size_t expectedMatches = (itemCount + 2) / 3;
    const auto makeInstance = [&](std::string_view className, int mode) {
        std::string source = "class ";
        source.append(className);
        source += R"( : ArtifactBehaviour
{
    public Array values;
    public int total = 0;
    void OnCreate() { values = [)";
        for (std::size_t i = 0; i < itemCount; ++i) {
            if (i != 0) source += ", ";
            source += i % 3 == 0 ? "\"hit\"" : "\"miss\"";
        }
        source += "];}\n";
        if (mode == 1) {
            source += "    void OnUpdate() { total = count(values, \"hit\"); }\n";
        } else if (mode == 2) {
            source += "    void OnUpdate() { total = 0; foreach (item in values) { if (item == \"hit\") total++; } }\n";
        } else {
            source += "    void OnUpdate() { total = 21; }\n";
        }
        source += "}\n";
        auto definition = parser.parse(source);
        EXPECT_TRUE(definition.diagnostics.empty());
        return ArtifactScriptInstance(std::move(definition));
    };

    auto builtin = makeInstance("ArrayCountBuiltinProbe", 1);
    auto scriptLoop = makeInstance("ArrayCountLoopProbe", 2);
    auto assignmentBaseline = makeInstance("ArrayCountAssignmentBaseline", 0);
    for (auto* instance : {&builtin, &scriptLoop}) {
        ASSERT_TRUE(instance->invokeHook(ArtifactScriptHook::OnCreate))
            << instance->lastError();
        ASSERT_TRUE(instance->invokeHook(ArtifactScriptHook::OnUpdate))
            << instance->lastError();
        EXPECT_EQ(std::get<std::int64_t>(instance->fields().at("total")),
                  static_cast<std::int64_t>(expectedMatches));
    }
    ASSERT_TRUE(assignmentBaseline.invokeHook(ArtifactScriptHook::OnCreate))
        << assignmentBaseline.lastError();
    ASSERT_TRUE(assignmentBaseline.invokeHook(ArtifactScriptHook::OnUpdate))
        << assignmentBaseline.lastError();
    EXPECT_EQ(std::get<std::int64_t>(
                  assignmentBaseline.fields().at("total")), 21);

#if defined(_MSC_VER) && defined(_DEBUG)
    constexpr std::size_t allocationIterations = 1000;
    const auto measureAllocations = [&](ArtifactScriptInstance& instance) {
        ScriptAllocationCounter counter;
        for (std::size_t i = 0; i < allocationIterations; ++i) {
            if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                ADD_FAILURE() << instance.lastError();
                break;
            }
        }
        return counter.stop();
    };
    const auto [builtinAllocations, builtinBytes] = measureAllocations(builtin);
    const auto [loopAllocations, loopBytes] = measureAllocations(scriptLoop);
    const auto [baselineAllocations, baselineBytes] = measureAllocations(assignmentBaseline);
    constexpr std::size_t timingIterations = 1500;
    constexpr std::size_t timingRepetitions = 3;
    const auto measureMicros = [&](ArtifactScriptInstance& instance) {
        std::array<double, timingRepetitions> samples{};
        for (auto& sample : samples) {
            const auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < timingIterations; ++i) {
                if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                    ADD_FAILURE() << instance.lastError();
                    break;
                }
            }
            sample = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - start).count() /
                timingIterations;
        }
        std::sort(samples.begin(), samples.end());
        return samples[samples.size() / 2];
    };
    const double builtinMicros = measureMicros(builtin);
    const double loopMicros = measureMicros(scriptLoop);
    std::cout << "ArtifactScript count(Array[64], \"hit\") vs foreach: "
              << builtinAllocations / allocationIterations << " / "
              << loopAllocations / allocationIterations
              << " / " << baselineAllocations / allocationIterations
              << " allocations/hook, "
              << builtinBytes / allocationIterations << " / "
              << loopBytes / allocationIterations << " / "
              << baselineBytes / allocationIterations << " bytes/hook, "
              << builtinMicros << " / " << loopMicros
              << " us/hook (median of " << timingRepetitions << " x "
              << timingIterations << ")\n";
#endif
}

TEST(LayerScriptComponentContractTest,
     ArraySumBuiltinMatchesForeachAndAvoidsInterpreterLoopWork) {
    ArtifactScriptParser parser;
    constexpr std::size_t itemCount = 64;
    constexpr std::int64_t expectedSum =
        static_cast<std::int64_t>(itemCount * (itemCount + 1) / 2);
    const auto makeInstance = [&](std::string_view className, bool useBuiltin) {
        std::string source = "class ";
        source.append(className);
        source += R"( : ArtifactBehaviour
{
    public Array values;
    public int total = 0;
    void OnCreate() { values = [)";
        for (std::size_t i = 0; i < itemCount; ++i) {
            if (i != 0) source += ", ";
            source += std::to_string(i + 1);
        }
        source += "];}\n";
        if (useBuiltin) {
            source += "    void OnUpdate() { total = sum(values); }\n";
        } else {
            source += "    void OnUpdate() { total = 0; foreach (item in values) { total += item; } }\n";
        }
        source += "}\n";
        auto definition = parser.parse(source);
        EXPECT_TRUE(definition.diagnostics.empty());
        return ArtifactScriptInstance(std::move(definition));
    };

    auto builtin = makeInstance("ArraySumBuiltinProbe", true);
    auto scriptLoop = makeInstance("ArraySumLoopProbe", false);
    for (auto* instance : {&builtin, &scriptLoop}) {
        ASSERT_TRUE(instance->invokeHook(ArtifactScriptHook::OnCreate))
            << instance->lastError();
        ASSERT_TRUE(instance->invokeHook(ArtifactScriptHook::OnUpdate))
            << instance->lastError();
        EXPECT_EQ(std::get<std::int64_t>(instance->fields().at("total")),
                  expectedSum);
    }

#if defined(_MSC_VER) && defined(_DEBUG)
    constexpr std::size_t allocationIterations = 1000;
    const auto measureAllocations = [&](ArtifactScriptInstance& instance) {
        ScriptAllocationCounter counter;
        for (std::size_t i = 0; i < allocationIterations; ++i) {
            if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                ADD_FAILURE() << instance.lastError();
                break;
            }
        }
        return counter.stop();
    };
    const auto [builtinAllocations, builtinBytes] = measureAllocations(builtin);
    const auto [loopAllocations, loopBytes] = measureAllocations(scriptLoop);
    constexpr std::size_t timingIterations = 1500;
    constexpr std::size_t timingRepetitions = 3;
    const auto measureMicros = [&](ArtifactScriptInstance& instance) {
        std::array<double, timingRepetitions> samples{};
        for (auto& sample : samples) {
            const auto start = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < timingIterations; ++i) {
                if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                    ADD_FAILURE() << instance.lastError();
                    break;
                }
            }
            sample = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - start).count() /
                timingIterations;
        }
        std::sort(samples.begin(), samples.end());
        return samples[samples.size() / 2];
    };
    const double builtinMicros = measureMicros(builtin);
    const double loopMicros = measureMicros(scriptLoop);
    std::cout << "ArtifactScript sum(Array[64]) vs foreach: "
              << builtinAllocations / allocationIterations << " / "
              << loopAllocations / allocationIterations
              << " allocations/hook, "
              << builtinBytes / allocationIterations << " / "
              << loopBytes / allocationIterations << " bytes/hook, "
              << builtinMicros << " / " << loopMicros
              << " us/hook (median of " << timingRepetitions << " x "
              << timingIterations << ")\n";
#endif
}

TEST(LayerScriptComponentContractTest,
     JoinBuiltinReducesAllocationsComparedWithScriptLoop) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class JoinAllocationProbe : ArtifactBehaviour
{
    public Array parts;
    public string joined;
    void OnCreate()
    {
        parts = ["first segment has enough bytes", "second segment also has enough bytes", "third segment"];
    }
    void OnUpdate() { joined = join(parts, " :: "); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate))
        << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();

    const std::string expected =
        "first segment has enough bytes :: second segment also has enough bytes :: third segment";
    EXPECT_EQ(std::get<std::string>(instance.fields().at("joined")), expected);

#if defined(_MSC_VER) && defined(_DEBUG)
    auto baselineDefinition = parser.parse(R"(
class JoinLoopAllocationProbe : ArtifactBehaviour
{
    public Array parts;
    public string joined;
    void OnCreate()
    {
        parts = ["first segment has enough bytes", "second segment also has enough bytes", "third segment"];
    }
    void OnUpdate()
    {
        joined = "";
        for (int i = 0; i < size(parts); i++)
        {
            if (i > 0) joined += " :: ";
            joined += parts[i];
        }
    }
}
)");
    ASSERT_TRUE(baselineDefinition.diagnostics.empty());
    ArtifactScriptInstance baseline(std::move(baselineDefinition));
    ASSERT_TRUE(baseline.invokeHook(ArtifactScriptHook::OnCreate))
        << baseline.lastError();
    ASSERT_TRUE(baseline.invokeHook(ArtifactScriptHook::OnUpdate))
        << baseline.lastError();
    EXPECT_EQ(std::get<std::string>(baseline.fields().at("joined")), expected);

    constexpr std::size_t allocationIterations = 1000;
    const auto measureAllocations = [&](ArtifactScriptInstance& measured) {
        ScriptAllocationCounter counter;
        std::string failure;
        for (std::size_t i = 0; i < allocationIterations; ++i) {
            if (!measured.invokeHook(ArtifactScriptHook::OnUpdate)) {
                failure = measured.lastError();
                break;
            }
        }
        const auto allocations = counter.stop();
        EXPECT_TRUE(failure.empty()) << failure;
        return allocations;
    };
    const auto [joinAllocations, joinBytes] = measureAllocations(instance);
    const auto [loopAllocations, loopBytes] = measureAllocations(baseline);
    std::cout << "ArtifactScript join vs += loop (" << expected.size()
              << " output bytes): "
              << joinAllocations / allocationIterations << " / "
              << loopAllocations / allocationIterations << " allocations/hook, "
              << joinBytes / allocationIterations << " / "
              << loopBytes / allocationIterations << " bytes/hook\n";
    EXPECT_LT(joinAllocations, loopAllocations);
    EXPECT_LT(joinBytes, loopBytes);
#endif
}

TEST(LayerScriptComponentContractTest, JoinBuiltinCpuScalingBenchmark) {
#if defined(_MSC_VER) && defined(_DEBUG)
    ArtifactScriptParser parser;
    const auto makeDefinition = [&](std::string_view className,
                                    std::size_t partCount,
                                    std::size_t partLength,
                                    bool useBuiltin) {
        std::string arrayLiteral = "[";
        for (std::size_t i = 0; i < partCount; ++i) {
            if (i != 0) arrayLiteral += ", ";
            arrayLiteral += '"';
            arrayLiteral.append(partLength, 'x');
            arrayLiteral += '"';
        }
        arrayLiteral += ']';

        std::string source = "class ";
        source.append(className);
        source += " : ArtifactBehaviour\n{\n"
                  "    public Array parts;\n"
                  "    public string joined;\n"
                  "    void OnCreate() { parts = ";
        source += arrayLiteral;
        source += "; }\n    void OnUpdate()\n    {\n";
        if (useBuiltin) {
            source += "        joined = join(parts, \"|\");\n";
        } else {
            source += "        joined = \"\";\n"
                      "        int count = size(parts);\n"
                      "        for (int i = 0; i < count; i++)\n"
                      "        {\n"
                      "            if (i > 0) joined += \"|\";\n"
                      "            joined += parts[i];\n"
                      "        }\n";
        }
        source += "    }\n}\n";
        return parser.parse(source);
    };

    const std::array<std::pair<std::size_t, std::size_t>, 3> workloads{{
        {3, 16}, {8, 64}, {32, 256}}};
    constexpr std::size_t iterations = 3000;
    constexpr std::size_t repetitions = 3;
    for (std::size_t workloadIndex = 0;
         workloadIndex < workloads.size(); ++workloadIndex) {
        const auto [partCount, partLength] = workloads[workloadIndex];
        auto joinDefinition = makeDefinition(
            "JoinCpuBuiltin", partCount, partLength, true);
        auto loopDefinition = makeDefinition(
            "JoinCpuLoop", partCount, partLength, false);
        ASSERT_TRUE(joinDefinition.diagnostics.empty());
        ASSERT_TRUE(loopDefinition.diagnostics.empty());
        ArtifactScriptInstance joinInstance(std::move(joinDefinition));
        ArtifactScriptInstance loopInstance(std::move(loopDefinition));
        ASSERT_TRUE(joinInstance.invokeHook(ArtifactScriptHook::OnCreate))
            << joinInstance.lastError();
        ASSERT_TRUE(loopInstance.invokeHook(ArtifactScriptHook::OnCreate))
            << loopInstance.lastError();
        ASSERT_TRUE(joinInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << joinInstance.lastError();
        ASSERT_TRUE(loopInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << loopInstance.lastError();
        EXPECT_EQ(std::get<std::string>(joinInstance.fields().at("joined")),
                  std::get<std::string>(loopInstance.fields().at("joined")));

        const auto measure = [&](ArtifactScriptInstance& instance) {
            std::array<double, repetitions> samples{};
            for (auto& sample : samples) {
                const auto start = std::chrono::steady_clock::now();
                for (std::size_t i = 0; i < iterations; ++i) {
                    if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
                        ADD_FAILURE() << instance.lastError();
                        break;
                    }
                }
                sample = std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - start).count() /
                    iterations;
            }
            std::sort(samples.begin(), samples.end());
            return samples[samples.size() / 2];
        };
        const double joinMicros = measure(joinInstance);
        const double loopMicros = measure(loopInstance);
        std::cout << "ArtifactScript join CPU (" << partCount << " x "
                  << partLength << " chars, "
                  << partCount * partLength + (partCount - 1)
                  << " output bytes): " << joinMicros << " vs " << loopMicros
                  << " us/hook (median of " << repetitions << " x "
                  << iterations << ")\n";
    }
#endif
}

TEST(LayerScriptComponentContractTest,
     ReplaceBuiltinAvoidsSplitJoinIntermediates) {
#if defined(_MSC_VER) && defined(_DEBUG)
    ArtifactScriptParser parser;
    const auto makeDefinition = [&](std::string_view className,
                                    std::size_t segmentCount,
                                    std::size_t segmentLength,
                                    bool useReplace) {
        std::string sourceValue;
        std::string expected;
        for (std::size_t i = 0; i < segmentCount; ++i) {
            if (i != 0) {
                sourceValue += "--";
                expected += '|';
            }
            sourceValue.append(segmentLength, 'x');
            expected.append(segmentLength, 'x');
        }

        std::string script = "class ";
        script.append(className);
        script += " : ArtifactBehaviour\n{\n"
                  "    public string source;\n"
                  "    public string output;\n"
                  "    void OnCreate() { source = \"";
        script += sourceValue;
        script += "\"; }\n    void OnUpdate() { output = ";
        script += useReplace ? "replace(source, \"--\", \"|\")" :
                               "join(split(source, \"--\"), \"|\")";
        script += "; }\n}\n";
        return std::pair{parser.parse(script), std::move(expected)};
    };

    const std::array<std::pair<std::size_t, std::size_t>, 3> workloads{{
        {3, 16}, {8, 64}, {32, 256}}};
    constexpr std::size_t timingIterations = 3000;
    constexpr std::size_t timingRepetitions = 3;
    for (std::size_t workloadIndex = 0;
         workloadIndex < workloads.size(); ++workloadIndex) {
        const auto [segmentCount, segmentLength] = workloads[workloadIndex];
        auto [replaceDefinition, expected] = makeDefinition(
            "ReplaceBuiltinProbe", segmentCount, segmentLength, true);
        auto [splitJoinDefinition, splitJoinExpected] = makeDefinition(
            "ReplaceSplitJoinProbe", segmentCount, segmentLength, false);
        ASSERT_TRUE(replaceDefinition.diagnostics.empty());
        ASSERT_TRUE(splitJoinDefinition.diagnostics.empty());
        ASSERT_EQ(expected, splitJoinExpected);
        ArtifactScriptInstance replaceInstance(std::move(replaceDefinition));
        ArtifactScriptInstance splitJoinInstance(std::move(splitJoinDefinition));
        ASSERT_TRUE(replaceInstance.invokeHook(ArtifactScriptHook::OnCreate))
            << replaceInstance.lastError();
        ASSERT_TRUE(splitJoinInstance.invokeHook(ArtifactScriptHook::OnCreate))
            << splitJoinInstance.lastError();
        ASSERT_TRUE(replaceInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << replaceInstance.lastError();
        ASSERT_TRUE(splitJoinInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << splitJoinInstance.lastError();
        EXPECT_EQ(std::get<std::string>(replaceInstance.fields().at("output")),
                  expected);
        EXPECT_EQ(std::get<std::string>(splitJoinInstance.fields().at("output")),
                  expected);

        if (workloadIndex == 0) {
            constexpr std::size_t allocationIterations = 1000;
            const auto measureAllocations = [&](ArtifactScriptInstance& measured) {
                ScriptAllocationCounter counter;
                std::string failure;
                for (std::size_t i = 0; i < allocationIterations; ++i) {
                    if (!measured.invokeHook(ArtifactScriptHook::OnUpdate)) {
                        failure = measured.lastError();
                        break;
                    }
                }
                const auto allocations = counter.stop();
                EXPECT_TRUE(failure.empty()) << failure;
                return allocations;
            };
            const auto [replaceAllocations, replaceBytes] =
                measureAllocations(replaceInstance);
            const auto [splitJoinAllocations, splitJoinBytes] =
                measureAllocations(splitJoinInstance);
            std::cout << "ArtifactScript replace vs split+join: "
                      << replaceAllocations / allocationIterations << " / "
                      << splitJoinAllocations / allocationIterations
                      << " allocations/hook, "
                      << replaceBytes / allocationIterations << " / "
                      << splitJoinBytes / allocationIterations
                      << " bytes/hook\n";
            EXPECT_LT(replaceAllocations, splitJoinAllocations);
            EXPECT_LT(replaceBytes, splitJoinBytes);
        }

        const auto measureTime = [&](ArtifactScriptInstance& measured) {
            std::array<double, timingRepetitions> samples{};
            for (auto& sample : samples) {
                const auto start = std::chrono::steady_clock::now();
                for (std::size_t i = 0; i < timingIterations; ++i) {
                    if (!measured.invokeHook(ArtifactScriptHook::OnUpdate)) {
                        ADD_FAILURE() << measured.lastError();
                        break;
                    }
                }
                sample = std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - start).count() /
                    timingIterations;
            }
            std::sort(samples.begin(), samples.end());
            return samples[samples.size() / 2];
        };
        const double replaceMicros = measureTime(replaceInstance);
        const double splitJoinMicros = measureTime(splitJoinInstance);
        std::cout << "ArtifactScript replace CPU (" << segmentCount << " x "
                  << segmentLength << " chars, " << expected.size()
                  << " output bytes): " << replaceMicros << " vs "
                  << splitJoinMicros << " us/hook (median of "
                  << timingRepetitions << " x " << timingIterations << ")\n";
    }
#endif
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

TEST(LayerScriptComponentContractTest, ForeachEvaluatesArrayExpressionOnce) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ForeachExpression : ArtifactBehaviour
{
    public float total = 0.0;
    public int calls = 0;

    Array makeValues()
    {
        calls += 1;
        return [2.0, 3.0, 5.0];
    }

    void OnUpdate()
    {
        foreach (item in makeValues())
            total += item;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty())
        << (definition.diagnostics.empty() ? "" : definition.diagnostics.front().message);

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    const auto numericValue = [](const ArtifactScriptValue& value) {
        if (const auto* number = std::get_if<double>(&value)) return *number;
        if (const auto* integer = std::get_if<std::int64_t>(&value))
            return static_cast<double>(*integer);
        ADD_FAILURE() << "expected a numeric script field";
        return 0.0;
    };
    EXPECT_DOUBLE_EQ(numericValue(instance.fields().at("total")), 10.0);
    EXPECT_DOUBLE_EQ(numericValue(instance.fields().at("calls")), 1.0);
}

TEST(LayerScriptComponentContractTest,
     SharedLayerRuntimeDrivesLifecycleAndFrameHooks) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class LayerRuntimeCounter : ArtifactBehaviour
{
    public float lifecycle = 0.0;
    public float elapsed = 0.0;
    public float observedTime = 0.0;
    public int observedFrame = 0;

    void OnCreate() { lifecycle = lifecycle * 10.0 + 1.0; }
    void OnStart() { lifecycle = lifecycle * 10.0 + 2.0; }
    void OnEnable() { lifecycle = lifecycle * 10.0 + 3.0; }
    void OnUpdate() {
        elapsed += dt;
        observedTime = time;
        observedFrame = frame;
    }
    void OnDisable() { lifecycle = lifecycle * 10.0 + 5.0; }
    void OnDestroy() { lifecycle = lifecycle * 10.0 + 6.0; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty())
        << (definition.diagnostics.empty() ? "" : definition.diagnostics.front().message);

    ArtifactScriptLayerRuntime runtime;
    runtime.bind(std::move(definition));
    ASSERT_TRUE(runtime.hasInstance());
    ASSERT_TRUE(runtime.advanceLifecycle(ArtifactScriptLayerRunState::Enabled,
                                         true));
    auto* instance = runtime.instance();
    ASSERT_NE(instance, nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(instance->fields().at("lifecycle")), 123.0);

    EXPECT_TRUE(runtime.evaluateFrame(50, 2.0, 0.5)) << runtime.lastError();
    EXPECT_FALSE(runtime.evaluateFrame(50, 9.0, 9.0));
    EXPECT_DOUBLE_EQ(std::get<double>(instance->fields().at("elapsed")), 0.5);
    EXPECT_DOUBLE_EQ(std::get<double>(instance->fields().at("observedTime")), 2.0);
    EXPECT_EQ(std::get<std::int64_t>(instance->fields().at("observedFrame")), 50);

    EXPECT_TRUE(runtime.evaluateFrame(51, 2.25, 0.25)) << runtime.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance->fields().at("elapsed")), 0.75);

    auto reloadedDefinition = parser.parse(R"(
class LayerRuntimeCounter : ArtifactBehaviour
{
    public float lifecycle = 0.0;
    public float elapsed = 0.0;
    public float observedTime = 0.0;
    public int observedFrame = 0;

    void OnCreate() { lifecycle = lifecycle * 10.0 + 1.0; }
    void OnStart() { lifecycle = lifecycle * 10.0 + 2.0; }
    void OnEnable() { lifecycle = lifecycle * 10.0 + 3.0; }
    void OnUpdate() {
        elapsed += dt * 2.0;
        observedTime = time;
        observedFrame = frame;
    }
    void OnDisable() { lifecycle = lifecycle * 10.0 + 5.0; }
    void OnDestroy() { lifecycle = lifecycle * 10.0 + 6.0; }
}
)");
    ASSERT_TRUE(reloadedDefinition.diagnostics.empty())
        << (reloadedDefinition.diagnostics.empty()
                ? ""
                : reloadedDefinition.diagnostics.front().message);
    ArtifactScriptSerializedFields migrated = instance->fields();
    runtime.replaceDefinition(std::move(reloadedDefinition), std::move(migrated));
    instance = runtime.instance();
    ASSERT_NE(instance, nullptr);
    EXPECT_FALSE(runtime.evaluateFrame(51, 3.0, 1.0));
    EXPECT_TRUE(runtime.evaluateFrame(52, 2.5, 0.25)) << runtime.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(instance->fields().at("elapsed")), 1.25);
    EXPECT_DOUBLE_EQ(std::get<double>(instance->fields().at("lifecycle")), 123.0);

    EXPECT_TRUE(runtime.advanceLifecycle(ArtifactScriptLayerRunState::Unbound,
                                         true));
    EXPECT_DOUBLE_EQ(std::get<double>(instance->fields().at("lifecycle")),
                     12356.0);

    runtime.release();
    EXPECT_FALSE(runtime.hasInstance());
    EXPECT_EQ(runtime.instance(), nullptr);
    EXPECT_FALSE(runtime.advanceLifecycle(ArtifactScriptLayerRunState::Enabled,
                                          true));
}

TEST(LayerScriptComponentContractTest,
     SharedLayerRuntimeInitializesPrivateFieldsAndReloadDefaults) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class PrivateRuntimeSettings : ArtifactBehaviour
{
    public float result = 0.0;
    private float runtimeCache = 4.0;
    [SerializeField]
    private float persistedSeed = 7.0;
    void OnUpdate() {
        result = runtimeCache + persistedSeed;
        runtimeCache += dt;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptLayerRuntime runtime;
    runtime.bind(std::move(definition));
    ASSERT_NE(runtime.instance(), nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(runtime.instance()->fields().at("runtimeCache")),
                     4.0);
    EXPECT_DOUBLE_EQ(std::get<double>(runtime.instance()->fields().at("persistedSeed")),
                     7.0);
    ASSERT_TRUE(runtime.advanceLifecycle(ArtifactScriptLayerRunState::Enabled,
                                         true));
    ASSERT_TRUE(runtime.evaluateFrame(1, 0.5, 0.5)) << runtime.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(runtime.instance()->fields().at("result")),
                     11.0);

    auto reloadedDefinition = parser.parse(R"(
class PrivateRuntimeSettings : ArtifactBehaviour
{
    public float result = 0.0;
    private float runtimeCache = 20.0;
    [SerializeField]
    private float persistedSeed = 8.0;
    void OnUpdate() {
        result = runtimeCache + persistedSeed;
        runtimeCache += dt;
    }
}
)");
    ASSERT_TRUE(reloadedDefinition.diagnostics.empty());
    ArtifactScriptSerializedFields migrated;
    migrated.emplace("persistedSeed", ArtifactScriptValue(9.0));
    runtime.replaceDefinition(std::move(reloadedDefinition), std::move(migrated));
    EXPECT_DOUBLE_EQ(std::get<double>(runtime.instance()->fields().at("runtimeCache")),
                     20.0);
    EXPECT_DOUBLE_EQ(std::get<double>(runtime.instance()->fields().at("persistedSeed")),
                     9.0);
    EXPECT_DOUBLE_EQ(std::get<double>(runtime.instance()->fields().at("result")),
                     0.0);
    ASSERT_TRUE(runtime.evaluateFrame(2, 1.0, 0.25)) << runtime.lastError();
    EXPECT_DOUBLE_EQ(std::get<double>(runtime.instance()->fields().at("result")),
                     29.0);
}

TEST(LayerScriptComponentContractTest,
     SharedLayerRuntimeClearsExecutionErrorAfterDefinitionReplacement) {
    ArtifactScriptParser parser;
    auto failingDefinition = parser.parse(R"(
class RecoverableLayerScript : ArtifactBehaviour
{
    public float value = 0.0;
    void OnUpdate() { value = 1.0 / 0.0; }
}
)");
    ASSERT_TRUE(failingDefinition.diagnostics.empty());

    ArtifactScriptLayerRuntime runtime;
    runtime.bind(std::move(failingDefinition));
    ASSERT_TRUE(runtime.advanceLifecycle(ArtifactScriptLayerRunState::Enabled,
                                         true));
    ASSERT_FALSE(runtime.evaluateFrame(10, 0.5, 0.016));
    ASSERT_FALSE(runtime.lastError().empty());

    auto recoveredDefinition = parser.parse(R"(
class RecoverableLayerScript : ArtifactBehaviour
{
    public float value = 0.0;
    void OnUpdate() { value += dt; }
}
)");
    ASSERT_TRUE(recoveredDefinition.diagnostics.empty());
    ArtifactScriptSerializedFields migrated;
    migrated.emplace("value", ArtifactScriptValue(3.0));
    runtime.replaceDefinition(std::move(recoveredDefinition), std::move(migrated));

    EXPECT_TRUE(runtime.lastError().empty());
    EXPECT_FALSE(runtime.advanceLifecycle(ArtifactScriptLayerRunState::Enabled,
                                          true));
    EXPECT_FALSE(runtime.evaluateFrame(10, 9.0, 9.0));
    EXPECT_TRUE(runtime.lastError().empty());
    ASSERT_TRUE(runtime.evaluateFrame(11, 0.75, 0.25)) << runtime.lastError();
    ASSERT_NE(runtime.instance(), nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(runtime.instance()->fields().at("value")),
                     3.25);
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

    const std::string longFieldValue(128, 'x');
    const std::string stringFieldSource =
        "class BenchmarkStringFieldRead : ArtifactBehaviour\n{\n"
        "public string source = \"seed\";\n"
        "public string target = \"\";\n"
        "void OnUpdate()\n{\ntarget = source;\n}\n}\n";
    auto stringFieldDefinition = parser.parse(stringFieldSource);
    ASSERT_TRUE(stringFieldDefinition.diagnostics.empty());
    ArtifactScriptInstance stringFieldInstance(std::move(stringFieldDefinition));
    ASSERT_TRUE(stringFieldInstance.invokeHook(ArtifactScriptHook::OnUpdate))
        << stringFieldInstance.lastError();
    stringFieldInstance.fields()["source"] = longFieldValue;
    stringFieldInstance.fields()["target"] = std::string{};
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(stringFieldInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << stringFieldInstance.lastError();
    }
    const auto& copiedString =
        std::get<std::string>(stringFieldInstance.fields().at("target"));
    ASSERT_EQ(copiedString.size(), longFieldValue.size());
    ASSERT_EQ(copiedString, longFieldValue);
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(stringFieldInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << stringFieldInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript string field read/write(128 chars) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";

    const std::string stringReadOnlySource = R"(
class BenchmarkStringFieldReadOnly : ArtifactBehaviour
{
    public string source = "seed";
    void OnUpdate() { var snapshot = source; }
}
)";
    auto stringReadOnlyDefinition = parser.parse(stringReadOnlySource);
    ASSERT_TRUE(stringReadOnlyDefinition.diagnostics.empty());
    ArtifactScriptInstance stringReadOnlyInstance(std::move(stringReadOnlyDefinition));
    ASSERT_TRUE(stringReadOnlyInstance.invokeHook(ArtifactScriptHook::OnUpdate))
        << stringReadOnlyInstance.lastError();
    stringReadOnlyInstance.fields()["source"] = longFieldValue;
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(stringReadOnlyInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << stringReadOnlyInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(stringReadOnlyInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << stringReadOnlyInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript string field read-only(128 chars) benchmark: "
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

    auto fiveArgumentDefinition = parser.parse(R"(
class BenchmarkFiveArgumentCall : ArtifactBehaviour
{
    public float total = 0.0;
    float sum(float a, float b, float c, float d, float e)
    {
        return a + b + c + d + e;
    }
    void OnUpdate() { total = sum(1.0, 2.0, 3.0, 4.0, 5.0); }
}
)");
    ASSERT_TRUE(fiveArgumentDefinition.diagnostics.empty());
    ArtifactScriptInstance fiveArgumentInstance(std::move(fiveArgumentDefinition));
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(fiveArgumentInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << fiveArgumentInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(fiveArgumentInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << fiveArgumentInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript method(5 args) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";

    auto sixArgumentDefinition = parser.parse(R"(
class BenchmarkSixArgumentCall : ArtifactBehaviour
{
    public float total = 0.0;
    float sum(float a, float b, float c, float d, float e, float f)
    {
        return a + b + c + d + e + f;
    }
    void OnUpdate()
    {
        total = sum(1.0, 2.0, 3.0, 4.0, 5.0,
                    sum(1.0, 2.0, 3.0, 4.0, 5.0, 6.0));
    }
}
)");
    ASSERT_TRUE(sixArgumentDefinition.diagnostics.empty());
    ArtifactScriptInstance sixArgumentInstance(std::move(sixArgumentDefinition));
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(sixArgumentInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << sixArgumentInstance.lastError();
    }
    EXPECT_DOUBLE_EQ(std::get<double>(sixArgumentInstance.fields().at("total")), 36.0);
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(sixArgumentInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << sixArgumentInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript method(6 nested args) benchmark: "
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

    const auto sumArguments = [](std::span<const ArtifactScriptValue> args) {
        double total = 0.0;
        for (const auto& argument : args) {
            if (std::holds_alternative<double>(argument)) total += std::get<double>(argument);
        }
        return ArtifactScriptValue(total);
    };
    auto& scriptHost = ArtifactScriptHost::global();
    scriptHost.registerFunction("allocationProbe", sumArguments);
    scriptHost.registerMethod("ObjectRef", "allocationProbe",
        [sumArguments](const ArtifactScriptValue&, std::span<const ArtifactScriptValue> args) {
            return sumArguments(args);
        });
    auto hostFunctionDefinition = parser.parse(R"(
class BenchmarkHostFunction : ArtifactBehaviour
{
    public float result = 0.0;
    void OnUpdate() { result = allocationProbe(1.0, 2.0, 3.0, 4.0, 5.0); }
}
)");
    ASSERT_TRUE(hostFunctionDefinition.diagnostics.empty());
    ArtifactScriptInstance hostFunctionInstance(std::move(hostFunctionDefinition));
    auto hostMethodDefinition = parser.parse(R"(
class BenchmarkHostMethod : ArtifactBehaviour
{
    public ObjectRef target;
    public float result = 0.0;
    void OnUpdate() { result = target.allocationProbe(1.0, 2.0, 3.0, 4.0, 5.0); }
}
)");
    ASSERT_TRUE(hostMethodDefinition.diagnostics.empty());
    ArtifactScriptInstance hostMethodInstance(std::move(hostMethodDefinition));
    hostMethodInstance.fields()["target"] = ArtifactScriptRef{"benchmark"};
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(hostFunctionInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << hostFunctionInstance.lastError();
        ASSERT_TRUE(hostMethodInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << hostMethodInstance.lastError();
    }
    EXPECT_DOUBLE_EQ(std::get<double>(hostFunctionInstance.fields().at("result")), 15.0);
    EXPECT_DOUBLE_EQ(std::get<double>(hostMethodInstance.fields().at("result")), 15.0);

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
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(largeForeachInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << largeForeachInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript foreach(257, read-only array) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";

    auto stringForeachDefinition = parser.parse(R"(
class BenchmarkStringForeach : ArtifactBehaviour
{
    public Array values;
    public string target = "";
    public float matches = 0.0;
    void OnCreate()
    {
        push(values, "this string is longer than the small string buffer");
        push(values, "another long string to expose copy allocations");
    }
    void OnUpdate()
    {
        foreach (item in values) {
            if (item == target) matches += 1.0;
        }
    }
}
)");
    ASSERT_TRUE(stringForeachDefinition.diagnostics.empty());
    ArtifactScriptInstance stringForeachInstance(std::move(stringForeachDefinition));
    ASSERT_TRUE(stringForeachInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << stringForeachInstance.lastError();
    stringForeachInstance.fields()["target"] = longFieldValue;
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(stringForeachInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << stringForeachInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(stringForeachInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << stringForeachInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript foreach(2 long strings, read-only) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";
    const auto stringValues = std::get<ArtifactScriptArrayPtr>(
        stringForeachInstance.fields().at("values"));
    ASSERT_TRUE(stringValues);
    stringValues->values.clear();
    for (int i = 0; i < 257; ++i) stringValues->values.push_back(longFieldValue);
    stringForeachInstance.fields()["matches"] = 0.0;
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(stringForeachInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << stringForeachInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(stringForeachInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << stringForeachInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript foreach(257 long strings, read-only) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(
                         stringForeachInstance.fields().at("matches")),
                     static_cast<double>(
                         (warmupIterations + repetitions * iterations) * 257));

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
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(wideLocalsInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << wideLocalsInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript locals(12) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";

    std::string overflowLocalsSource =
        "class BenchmarkOverflowLocals : ArtifactBehaviour {\n"
        "public float total = 0.0;\nvoid OnUpdate() {\n";
    std::string overflowLocalsSum;
    for (int i = 0; i < 20; ++i) {
        const auto name = "local" + std::to_string(i);
        overflowLocalsSource += "float " + name + " = " +
                                std::to_string(i + 1) + ".0;\n";
        if (i != 0) overflowLocalsSum += " + ";
        overflowLocalsSum += name;
    }
    overflowLocalsSource += "total = " + overflowLocalsSum + ";\n}\n}";
    auto overflowLocalsDefinition = parser.parse(overflowLocalsSource);
    ASSERT_TRUE(overflowLocalsDefinition.diagnostics.empty());
    ArtifactScriptInstance overflowLocalsInstance(std::move(overflowLocalsDefinition));
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(overflowLocalsInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << overflowLocalsInstance.lastError();
    }
    EXPECT_DOUBLE_EQ(std::get<double>(overflowLocalsInstance.fields().at("total")), 210.0);
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(overflowLocalsInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << overflowLocalsInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript locals(20, overflow workspace) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";

    std::string methodLookupSource = R"(
class BenchmarkMethodLookup : ArtifactBehaviour
{
    public float total = 0.0;
)";
    for (int i = 0; i < 32; ++i) {
        methodLookupSource += "    float filler" + std::to_string(i) +
            "(float value) { return value; }\n";
    }
    methodLookupSource += R"(
    float increment(float value) { return value + 1.0; }
    void OnUpdate()
    {
        for (int index = 0; index < 16; index += 1) total = increment(total);
    }
}
)";
    auto methodLookupDefinition = parser.parse(methodLookupSource);
    ASSERT_TRUE(methodLookupDefinition.diagnostics.empty());
    ArtifactScriptInstance methodLookupInstance(std::move(methodLookupDefinition));
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(methodLookupInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << methodLookupInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(methodLookupInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << methodLookupInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript method lookup(32 methods, 16 calls) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(methodLookupInstance.fields().at("total")),
                     (100.0 + repetitions * iterations) * 16.0);

    std::string objectMethodLookupSource = R"(
class BenchmarkObjectMethodLookup : ArtifactBehaviour
{
    public float total = 0.0;
    void OnCreate() { counter = new Counter(); }
    void OnUpdate()
    {
        for (int index = 0; index < 16; index += 1) total = counter.increment(total);
    }
}
class Counter : ArtifactBehaviour
{
)";
    for (int i = 0; i < 32; ++i) {
        objectMethodLookupSource += "    float filler" + std::to_string(i) +
            "(float value) { return value; }\n";
    }
    objectMethodLookupSource += R"(
    float increment(float value) { return value + 1.0; }
}
)";
    auto objectMethodLookupDefinition = parser.parse(objectMethodLookupSource);
    ASSERT_TRUE(objectMethodLookupDefinition.diagnostics.empty());
    ArtifactScriptInstance objectMethodLookupInstance(std::move(objectMethodLookupDefinition));
    ASSERT_TRUE(objectMethodLookupInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << objectMethodLookupInstance.lastError();
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(objectMethodLookupInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << objectMethodLookupInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(objectMethodLookupInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << objectMethodLookupInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript object method lookup(32 methods, 16 calls) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(objectMethodLookupInstance.fields().at("total")),
                     (100.0 + repetitions * iterations) * 16.0);

    auto polymorphicObjectMethodDefinition = parser.parse(R"(
class BenchmarkPolymorphicObjectMethodLookup : ArtifactBehaviour
{
    public float total = 0.0;
    public ObjectRef target;
    void OnUpdate()
    {
        for (int index = 0; index < 16; index += 1) {
            if (index % 3 == 0) { target = second; }
            else if (index % 3 == 1) { target = first; }
            else { target = third; }
            total = total + target.who();
        }
    }
    public ObjectRef third;
    void OnCreate()
    {
        first = new Base();
        second = new Child();
        third = new Sibling();
    }
}
class Base : ArtifactBehaviour
{
    float who() { return 1.0; }
}
class Child : Base
{
    float who() { return 2.0; }
}
class Sibling : Base
{
    float who() { return 3.0; }
}
)");
    ASSERT_TRUE(polymorphicObjectMethodDefinition.diagnostics.empty());
    ArtifactScriptInstance polymorphicObjectMethodInstance(
        std::move(polymorphicObjectMethodDefinition));
    ASSERT_TRUE(polymorphicObjectMethodInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << polymorphicObjectMethodInstance.lastError();
    constexpr int polymorphicRepetitions = 3;
    constexpr int polymorphicIterations = 1000;
    constexpr int polymorphicWarmupIterations = 100;
    for (int i = 0; i < polymorphicWarmupIterations; ++i) {
        ASSERT_TRUE(polymorphicObjectMethodInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << polymorphicObjectMethodInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < polymorphicRepetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < polymorphicIterations; ++i) {
            ASSERT_TRUE(polymorphicObjectMethodInstance.invokeHook(
                ArtifactScriptHook::OnUpdate))
                << polymorphicObjectMethodInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript polymorphic object method (3 classes, 16 calls) benchmark: "
              << totalMicroseconds / (polymorphicRepetitions * polymorphicIterations)
              << " us/hook (" << polymorphicIterations * polymorphicRepetitions
              << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(polymorphicObjectMethodInstance.fields().at("total")),
                     (polymorphicWarmupIterations +
                      polymorphicRepetitions * polymorphicIterations) * 32.0);

    auto fourClassObjectMethodDefinition = parser.parse(R"(
class BenchmarkFourClassObjectMethodLookup : ArtifactBehaviour
{
    public float total = 0.0;
    public ObjectRef target;
    public ObjectRef third;
    public ObjectRef fourth;
    void OnCreate()
    {
        first = new Base();
        second = new Child();
        third = new Sibling();
        fourth = new Other();
    }
    void OnUpdate()
    {
        for (int index = 0; index < 16; index += 1) {
            if (index % 4 == 0) { target = second; }
            else if (index % 4 == 1) { target = first; }
            else if (index % 4 == 2) { target = third; }
            else { target = fourth; }
            total = total + target.who();
        }
    }
}
class Base : ArtifactBehaviour
{
    float who() { return 1.0; }
}
class Child : Base
{
    float who() { return 2.0; }
}
class Sibling : Base
{
    float who() { return 3.0; }
}
class Other : Base
{
    float who() { return 4.0; }
}
)");
    ASSERT_TRUE(fourClassObjectMethodDefinition.diagnostics.empty());
    ArtifactScriptInstance fourClassObjectMethodInstance(
        std::move(fourClassObjectMethodDefinition));
    ASSERT_TRUE(fourClassObjectMethodInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << fourClassObjectMethodInstance.lastError();
    for (int i = 0; i < polymorphicWarmupIterations; ++i) {
        ASSERT_TRUE(fourClassObjectMethodInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << fourClassObjectMethodInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < polymorphicRepetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < polymorphicIterations; ++i) {
            ASSERT_TRUE(fourClassObjectMethodInstance.invokeHook(
                ArtifactScriptHook::OnUpdate))
                << fourClassObjectMethodInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript polymorphic object method (4 classes, 16 calls) benchmark: "
              << totalMicroseconds / (polymorphicRepetitions * polymorphicIterations)
              << " us/hook (" << polymorphicIterations * polymorphicRepetitions
              << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(fourClassObjectMethodInstance.fields().at("total")),
                     (polymorphicWarmupIterations +
                      polymorphicRepetitions * polymorphicIterations) * 40.0);

    auto wideObjectMethodDefinition = parser.parse(R"(
class BenchmarkWideObjectMethod : ArtifactBehaviour
{
    public float total = 0.0;
    void OnCreate() { counter = new Counter(); }
    void OnUpdate()
    {
        for (int index = 0; index < 16; index += 1) {
            counter.update(total);
            total += 1.0;
        }
    }
}
class Counter : ArtifactBehaviour
{
    public float a = 0.0;
    public float b = 0.0;
    public float c = 0.0;
    public float d = 0.0;
    public float e = 0.0;
    void update(float value)
    {
        this.a = value;
        this.b = value;
        this.c = value;
        this.d = value;
        this.e = value;
    }
}
)");
    ASSERT_TRUE(wideObjectMethodDefinition.diagnostics.empty());
    ArtifactScriptInstance wideObjectMethodInstance(std::move(wideObjectMethodDefinition));
    ASSERT_TRUE(wideObjectMethodInstance.invokeHook(ArtifactScriptHook::OnCreate))
        << wideObjectMethodInstance.lastError();
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(wideObjectMethodInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << wideObjectMethodInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(wideObjectMethodInstance.invokeHook(ArtifactScriptHook::OnUpdate))
                << wideObjectMethodInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript object method(5 fields, 16 calls) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(wideObjectMethodInstance.fields().at("total")),
                     (100.0 + repetitions * iterations) * 16.0);

    auto objectConstructionDefinition = parser.parse(R"(
class BenchmarkObjectConstruction : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate()
    {
        var value = new Thing();
        total += value.f0 + value.f1 + value.f2 + value.f3 +
                 value.f4 + value.f5 + value.f6 + value.f7;
    }
}
class Thing : ArtifactBehaviour
{
    public float f0 = 1.0;
    public float f1 = 2.0;
    public float f2 = 3.0;
    public float f3 = 4.0;
    public float f4 = 5.0;
    public float f5 = 6.0;
    public float f6 = 7.0;
    public float f7 = 8.0;
}
)");
    ASSERT_TRUE(objectConstructionDefinition.diagnostics.empty());
    ArtifactScriptInstance objectConstructionInstance(
        std::move(objectConstructionDefinition));
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(objectConstructionInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << objectConstructionInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(objectConstructionInstance.invokeHook(
                ArtifactScriptHook::OnUpdate))
                << objectConstructionInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript object construction (8 defaults) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(objectConstructionInstance.fields().at("total")),
                     (warmupIterations + repetitions * iterations) * 36.0);

    std::string deepClassLookupSource = R"(
class BenchmarkDeepClassLookup : ArtifactBehaviour
{
    public ObjectRef target;
    public float total = 0.0;
    void OnUpdate()
    {
        target = new Level29();
        total += 1.0;
    }
}
)";
    for (int i = 0; i < 30; ++i) {
        deepClassLookupSource += "class Level" + std::to_string(i) + " : ";
        deepClassLookupSource += i == 0 ? "ArtifactBehaviour" :
            "Level" + std::to_string(i - 1);
        deepClassLookupSource += "\n{\n}\n";
    }
    auto deepClassLookupDefinition = parser.parse(deepClassLookupSource);
    ASSERT_TRUE(deepClassLookupDefinition.diagnostics.empty());
    ArtifactScriptInstance deepClassLookupInstance(
        std::move(deepClassLookupDefinition));
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(deepClassLookupInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << deepClassLookupInstance.lastError();
    }
    totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(deepClassLookupInstance.invokeHook(
                ArtifactScriptHook::OnUpdate))
                << deepClassLookupInstance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript object construction (30-class chain) benchmark: "
              << totalMicroseconds / (repetitions * iterations)
              << " us/hook (" << iterations * repetitions << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(deepClassLookupInstance.fields().at("total")),
                     (warmupIterations + repetitions * iterations) * 1.0);

    std::string inheritedHookSource = R"(
class BenchmarkInheritedHook : Level29
{
    public float total = 0.0;
}
)";
    for (int i = 0; i < 30; ++i) {
        inheritedHookSource += "class Level" + std::to_string(i) + " : ";
        inheritedHookSource += i == 0 ? "ArtifactBehaviour" :
            "Level" + std::to_string(i - 1);
        inheritedHookSource += "\n{\n";
        if (i == 0) inheritedHookSource += "    void OnUpdate() { total += 1.0; }\n";
        inheritedHookSource += "}\n";
    }
    auto inheritedHookDefinition = parser.parse(inheritedHookSource);
    ASSERT_TRUE(inheritedHookDefinition.diagnostics.empty());
    ArtifactScriptInstance inheritedHookInstance(std::move(inheritedHookDefinition));
    constexpr int inheritedHookWarmup = 20;
    constexpr int inheritedHookIterations = 3000;
    for (int i = 0; i < inheritedHookWarmup; ++i) {
        ASSERT_TRUE(inheritedHookInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << inheritedHookInstance.lastError();
    }
    const auto inheritedHookStart = std::chrono::steady_clock::now();
    for (int i = 0; i < inheritedHookIterations; ++i) {
        ASSERT_TRUE(inheritedHookInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << inheritedHookInstance.lastError();
    }
    const auto inheritedHookMicroseconds = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - inheritedHookStart).count() /
        inheritedHookIterations;
    std::cout << "ArtifactScript inherited hook lookup (30 classes) benchmark: "
              << inheritedHookMicroseconds << " us/hook ("
              << inheritedHookIterations << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(inheritedHookInstance.fields().at("total")),
                     inheritedHookWarmup + inheritedHookIterations);

    std::string rootHookSource = R"(
class BenchmarkRootHook : ArtifactBehaviour
{
    public float total = 0.0;
    void OnUpdate() { total += 1.0; }
}
)";
    for (int i = 0; i < 30; ++i) {
        rootHookSource += "class Extra" + std::to_string(i) +
            " : ArtifactBehaviour\n{\n    public float value = 0.0;\n}\n";
    }
    auto rootHookDefinition = parser.parse(rootHookSource);
    ASSERT_TRUE(rootHookDefinition.diagnostics.empty());
    ArtifactScriptInstance rootHookInstance(std::move(rootHookDefinition));
    for (int i = 0; i < inheritedHookWarmup; ++i) {
        ASSERT_TRUE(rootHookInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << rootHookInstance.lastError();
    }
    const auto rootHookStart = std::chrono::steady_clock::now();
    for (int i = 0; i < inheritedHookIterations; ++i) {
        ASSERT_TRUE(rootHookInstance.invokeHook(ArtifactScriptHook::OnUpdate))
            << rootHookInstance.lastError();
    }
    const auto rootHookMicroseconds = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - rootHookStart).count() /
        inheritedHookIterations;
    std::cout << "ArtifactScript root hook lookup (31 classes) benchmark: "
              << rootHookMicroseconds << " us/hook ("
              << inheritedHookIterations << " calls)\n";
    EXPECT_DOUBLE_EQ(std::get<double>(rootHookInstance.fields().at("total")),
                     inheritedHookWarmup + inheritedHookIterations);
#endif
}

TEST(LayerScriptComponentContractTest,
     EmptyForeachMutationAnalysisBenchmark) {
    ArtifactScriptParser parser;
    std::string source = R"(
class EmptyForeachMutationAnalysisProbe : ArtifactBehaviour
{
    public Array values;
    void OnUpdate()
    {
        foreach (item in values) {
)";
    for (int i = 0; i < 256; ++i) {
        source += "            float unused" + std::to_string(i) + " = " +
                  std::to_string(i) + ".0;\n";
    }
    source += R"(
        }
    }
}
)";
    auto definition = parser.parse(source);
    ASSERT_TRUE(definition.diagnostics.empty());
    ArtifactScriptInstance instance(std::move(definition));
    instance.fields()["values"] = makeShared<ArtifactScriptArray>();

    constexpr int warmupIterations = 100;
    constexpr int repetitions = 3;
    constexpr int iterations = 10000;
    for (int i = 0; i < warmupIterations; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }
    std::array<double, repetitions> microsecondsPerHook{};
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
                << instance.lastError();
        }
        microsecondsPerHook[repetition] =
            std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - start).count() / iterations;
    }
    std::cout << "ArtifactScript empty foreach with 256-statement mutation scan: "
              << microsecondsPerHook[0] << ", "
              << microsecondsPerHook[1] << ", "
              << microsecondsPerHook[2] << " us/hook\n";
}

#if defined(_MSC_VER) && defined(_DEBUG)
TEST(LayerScriptComponentContractTest,
     ArrayIndexStringComparisonAvoidsSteadyStateCopies) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ArrayStringComparisonProbe : ArtifactBehaviour
{
    public Array values;
    public string target = "";
    public float matches = 0.0;
    void OnCreate()
    {
        push(values, "this is a long string stored inside the script array");
    }
    void OnUpdate()
    {
        if (values[0] == target) matches += 1.0;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate))
        << instance.lastError();
    instance.fields()["target"] =
        std::string("this is a long string stored inside the script array");
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("matches")), 100.0);

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, 0)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, 0)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("matches")), 1100.0);
}

TEST(LayerScriptComponentContractTest,
     ObjectFieldStringComparisonAvoidsSteadyStateCopies) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ObjectFieldStringComparisonProbe : ArtifactBehaviour
{
    public ObjectRef target;
    public string expected = "";
    public float matches = 0.0;
    void OnCreate()
    {
        target = new ObjectFieldStringComparisonTarget();
        target.value = "this is a long string stored inside an object field";
    }
    void OnUpdate()
    {
        if (target.value == expected) matches += 1.0;
    }
}
class ObjectFieldStringComparisonTarget : ArtifactBehaviour
{
    public string value = "";
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate))
        << instance.lastError();
    instance.fields()["expected"] =
        std::string("this is a long string stored inside an object field");
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("matches")), 100.0);

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, 0)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, 0)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("matches")), 1100.0);
}

TEST(LayerScriptComponentContractTest,
     ParserMethodBodyParsingAvoidsTemporaryBodyCopyAllocations) {
    ArtifactScriptParser parser;
    const std::string longLiteral(512, 'x');
    const std::string source =
        "class ParserBodyProbe : ArtifactBehaviour {\n"
        " public string value = \"\";\n"
        " void OnUpdate() { value = \"" + longLiteral + "\"; }\n"
        "}\n";

    ScriptAllocationCounter counter;
    constexpr std::size_t parseIterations = 100;
    std::size_t parsedDefinitions = 0;
    for (std::size_t i = 0; i < parseIterations; ++i) {
        auto definition = parser.parse(source);
        if (definition.diagnostics.empty() && definition.rootClass.methods.size() == 1) {
            ++parsedDefinitions;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_EQ(parsedDefinitions, parseIterations);
    EXPECT_LT(allocations.first / parseIterations, 87u);
    EXPECT_LT(allocations.second / parseIterations, 5000u);
    std::cout << "ArtifactScript long method-body parse allocations: "
              << allocations.first / parseIterations << " allocations/parse, "
              << allocations.second / parseIterations << " bytes/parse ("
              << parseIterations << " parses)\n";
}

TEST(LayerScriptComponentContractTest,
     NumericLiteralParsingAvoidsTokenCopyAllocations) {
    constexpr std::string_view shortToken = "1.0";
    constexpr std::string_view scientificToken = "1.234567890123456e-12";
    constexpr std::size_t parseIterations = 200;
    ArtifactScriptParser parser;

    const auto makeSource = [](std::string_view token,
                               std::size_t targetTokenLength) {
        const std::string prefix =
            "class NumericParseProbe : ArtifactBehaviour {\n"
            " public float value = ";
        const std::string suffix = ";\n}\n";
        std::string source = prefix;
        source.append(token);
        source.append(targetTokenLength - token.size(), ' ');
        source.append(suffix);
        return source;
    };
    const std::string shortSource = makeSource(shortToken, scientificToken.size());
    const std::string scientificSource =
        makeSource(scientificToken, scientificToken.size());
    ASSERT_EQ(shortSource.size(), scientificSource.size());

    const auto countAllocations = [&](const std::string& source) {
        ScriptAllocationCounter counter;
        std::size_t parsedDefinitions = 0;
        for (std::size_t i = 0; i < parseIterations; ++i) {
            auto definition = parser.parse(source);
            if (definition.diagnostics.empty()) ++parsedDefinitions;
        }
        return std::pair{parsedDefinitions, counter.stop()};
    };
    const auto shortResult = countAllocations(shortSource);
    const auto scientificResult = countAllocations(scientificSource);

    ASSERT_EQ(shortResult.first, parseIterations);
    ASSERT_EQ(scientificResult.first, parseIterations);
    EXPECT_EQ(scientificResult.second.first, shortResult.second.first);
    EXPECT_EQ(scientificResult.second.second, shortResult.second.second);

    const auto measureParseTime = [&](const std::string& source) {
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < parseIterations; ++i) {
            const auto definition = parser.parse(source);
            if (!definition.diagnostics.empty()) return -1.0;
        }
        return std::chrono::duration<double, std::micro>(
                   std::chrono::steady_clock::now() - start)
                   .count() /
               parseIterations;
    };
    const double shortMicroseconds = measureParseTime(shortSource);
    const double scientificMicroseconds = measureParseTime(scientificSource);
    ASSERT_GE(shortMicroseconds, 0.0);
    ASSERT_GE(scientificMicroseconds, 0.0);
    std::cout << "ArtifactScript numeric literal parse allocations: short "
              << shortResult.second.first / parseIterations << " / "
              << shortResult.second.second / parseIterations
              << " bytes, scientific "
              << scientificResult.second.first / parseIterations << " / "
              << scientificResult.second.second / parseIterations
              << " bytes per parse; time " << shortMicroseconds << " vs "
              << scientificMicroseconds << " us/parse\n";
}

TEST(LayerScriptComponentContractTest, ParserStringScanBenchmark) {
    constexpr std::size_t stringLength = 4096;
    constexpr std::size_t scanIterations = 20000;
    constexpr std::size_t parseIterations = 200;
    const std::string scanSource(stringLength, 'x');
    const std::string scanInput = scanSource + '"';
    ArtifactScriptParser parser;
    const std::string script =
        "class ParserStringScanProbe : ArtifactBehaviour {\n"
        " public string value = \"\";\n"
        " void OnUpdate() { value = \"" + scanSource + "\"; }\n"
        "}\n";
    const std::string escapedScript =
        "class ParserEscapedStringScanProbe : ArtifactBehaviour {\n"
        " public string value = \"\";\n"
        " void OnUpdate() { value = \"" + scanSource + "\\n\"; }\n"
        "}\n";

    for (int warmup = 0; warmup < 5; ++warmup) {
        const auto definition = parser.parse(script);
        ASSERT_TRUE(definition.diagnostics.empty());
    }

    std::size_t linearChecksum = 0;
    const auto linearStart = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < scanIterations; ++iteration) {
        std::size_t position = 0;
        while (position < scanInput.size() && scanInput[position] != '"' &&
               scanInput[position] != '\\') {
            ++position;
        }
        linearChecksum += position;
    }
    const auto linearElapsed = std::chrono::steady_clock::now() - linearStart;

    std::size_t findChecksum = 0;
    const auto findStart = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < scanIterations; ++iteration) {
        const auto marker = scanInput.find_first_of("\\\"");
        findChecksum += marker == std::string::npos ? scanInput.size() : marker;
    }
    const auto findElapsed = std::chrono::steady_clock::now() - findStart;

    std::size_t parsedDefinitions = 0;
    const auto parseStart = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < parseIterations; ++iteration) {
        const auto definition = parser.parse(script);
        if (definition.diagnostics.empty()) ++parsedDefinitions;
    }
    const auto parseElapsed = std::chrono::steady_clock::now() - parseStart;

    std::size_t parsedEscapedDefinitions = 0;
    const auto escapedParseStart = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < parseIterations; ++iteration) {
        const auto definition = parser.parse(escapedScript);
        if (definition.diagnostics.empty()) ++parsedEscapedDefinitions;
    }
    const auto escapedParseElapsed =
        std::chrono::steady_clock::now() - escapedParseStart;

    EXPECT_EQ(linearChecksum, findChecksum);
    EXPECT_EQ(parsedDefinitions, parseIterations);
    EXPECT_EQ(parsedEscapedDefinitions, parseIterations);
    const auto linearMicroseconds =
        std::chrono::duration<double, std::micro>(linearElapsed).count() /
        scanIterations;
    const auto findMicroseconds =
        std::chrono::duration<double, std::micro>(findElapsed).count() /
        scanIterations;
    const auto parseMicroseconds =
        std::chrono::duration<double, std::micro>(parseElapsed).count() /
        parseIterations;
    const auto escapedParseMicroseconds =
        std::chrono::duration<double, std::micro>(escapedParseElapsed).count() /
        parseIterations;
    std::cout << "ArtifactScript 4 KiB unescaped string: linear scan "
              << linearMicroseconds << " us, find_first_of " << findMicroseconds
              << " us, full parse " << parseMicroseconds << " us/parse; escaped "
              << escapedParseMicroseconds << " us/parse\n";
}

TEST(LayerScriptComponentContractTest, ScriptObjectHostMethodAvoidsSteadyStateAllocations) {
    auto& host = ArtifactScriptHost::global();
    host.registerMethod("LongNamedHostTarget", "hostPing",
        [](const ArtifactScriptValue&, std::span<const ArtifactScriptValue>) {
            return ArtifactScriptValue(1.0);
        });
    host.setLastError({});

    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptObjectHostMethodProbe : ArtifactBehaviour
{
    public ObjectRef target;
    public float result = 0.0;
    void OnCreate() { target = new LongNamedHostTarget(); }
    void OnUpdate() { result = target.hostPing(); }
}
class LongNamedHostTarget : ArtifactBehaviour
{
    public float value = 1.0;
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    }
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 1.0);

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, 0)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, 0)
        << "bytes/hook=" << allocations.second / allocationIterations;
}

TEST(LayerScriptComponentContractTest, ScriptHostFunctionAvoidsSteadyStateAllocations) {
    auto& host = ArtifactScriptHost::global();
    host.registerFunction("hostPing",
        [](std::span<const ArtifactScriptValue>) {
            return ArtifactScriptValue(1.0);
        });

    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptHostFunctionProbe : ArtifactBehaviour
{
    public float result = 0.0;
    void OnUpdate() { result = hostPing(); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, 0)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, 0)
        << "bytes/hook=" << allocations.second / allocationIterations;
}

TEST(LayerScriptComponentContractTest,
     ScriptPushMovesEvaluatedStringIntoRetainedArrayStorage) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptPushStringAllocationProbe : ArtifactBehaviour
{
    public Array values;
    public string source = "seed";
    void OnUpdate()
    {
        clear(values);
        push(values, source);
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 's');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }
    const auto initialValues = std::get<ArtifactScriptArrayPtr>(
        instance.fields().at("values"));
    ASSERT_TRUE(initialValues);
    ASSERT_EQ(initialValues->values.size(), 1u);
    EXPECT_EQ(std::get<std::string>(initialValues->values.front()).size(), 128u);

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 4)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, allocationIterations * 192)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")).size(), 128u);
    const auto values = std::get<ArtifactScriptArrayPtr>(
        instance.fields().at("values"));
    ASSERT_TRUE(values);
    ASSERT_EQ(values->values.size(), 1u);
    EXPECT_EQ(std::get<std::string>(values->values.front()).size(), 128u);
}

TEST(LayerScriptComponentContractTest,
     ScriptLocalDeclarationMovesEvaluatedStringIntoBinding) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptLocalStringDeclarationProbe : ArtifactBehaviour
{
    public string source = "seed";
    public string observed = "";
    void OnUpdate()
    {
        string snapshot = source;
        observed = snapshot;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 's');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 6)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, allocationIterations * 352)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")),
              std::string(128, 's'));
    EXPECT_EQ(std::get<std::string>(instance.fields().at("observed")),
              std::string(128, 's'));
    std::cout << "ArtifactScript long-string local declaration: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest,
     ScriptMethodReturnsMoveLongStringsOutOfReturnSlots) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptStringReturnProbe : ArtifactBehaviour
{
    public ObjectRef target;
    public string source = "seed";
    public string observed = "";
    void OnCreate() { target = new ScriptStringReturnSink(); }
    string copyValue() { return source; }
    void OnUpdate()
    {
        observed = copyValue();
        observed = target.copyValue(source);
    }
}
class ScriptStringReturnSink : ArtifactBehaviour
{
    string copyValue(string input) { return input; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate))
        << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 'r');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 13)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, allocationIterations * 592)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")),
              std::string(128, 'r'));
    EXPECT_EQ(std::get<std::string>(instance.fields().at("observed")),
              std::string(128, 'r'));
    std::cout << "ArtifactScript long-string method returns: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest,
     ScriptStringCompoundAssignmentReportsAllocations) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptStringCompoundAssignmentProbe : ArtifactBehaviour
{
    public string source = "seed";
    public string suffix = "suffix";
    public string result = "";
    void OnUpdate()
    {
        result = source;
        result += suffix;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 'a');
    instance.fields()["suffix"] = std::string(128, 'b');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    constexpr int timingRepetitions = 3;
    constexpr int timingIterations = 10000;
    double totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < timingRepetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < timingIterations; ++i) {
            ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
                << instance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript long-string += benchmark: "
              << totalMicroseconds / (timingRepetitions * timingIterations)
              << " us/hook\n";

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 5)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, allocationIterations * 592)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")),
              std::string(128, 'a'));
    EXPECT_EQ(std::get<std::string>(instance.fields().at("suffix")),
              std::string(128, 'b'));
    EXPECT_EQ(std::get<std::string>(instance.fields().at("result")),
              std::string(128, 'a') + std::string(128, 'b'));
    std::cout << "ArtifactScript long-string +=: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest,
     ScriptStringAssignmentReportsAllocations) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptStringAssignmentProbe : ArtifactBehaviour
{
    public string source = "seed";
    public string observed = "";
    void OnUpdate() { observed = source; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 'a');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 2)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, allocationIterations * 160)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")),
              std::string(128, 'a'));
    EXPECT_EQ(std::get<std::string>(instance.fields().at("observed")),
              std::string(128, 'a'));
    std::cout << "ArtifactScript long-string assignment: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest,
     ScriptStringAdditionReportsAllocations) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptStringAdditionProbe : ArtifactBehaviour
{
    public string source = "seed";
    public string suffix = "tail";
    public string observed = "";
    void OnUpdate() { observed = source + suffix; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 'a');
    instance.fields()["suffix"] = std::string(128, 'b');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 3)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, allocationIterations * 304)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")),
              std::string(128, 'a'));
    EXPECT_EQ(std::get<std::string>(instance.fields().at("suffix")),
              std::string(128, 'b'));
    EXPECT_EQ(std::get<std::string>(instance.fields().at("observed")),
              std::string(128, 'a') + std::string(128, 'b'));
    std::cout << "ArtifactScript long-string +: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest,
     ScriptMultiStringAdditionReportsAllocations) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptMultiStringAdditionProbe : ArtifactBehaviour
{
    public string first = "";
    public string second = "";
    public string third = "";
    public string fourth = "";
    public string observed = "";
    void OnUpdate() { observed = first + second + third + fourth; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["first"] = std::string(128, 'a');
    instance.fields()["second"] = std::string(128, 'b');
    instance.fields()["third"] = std::string(128, 'c');
    instance.fields()["fourth"] = std::string(128, 'd');
    const std::string expected = std::string(128, 'a') + std::string(128, 'b') +
                                 std::string(128, 'c') + std::string(128, 'd');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 3)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, allocationIterations * 560)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("observed")), expected);
    std::cout << "ArtifactScript four-string +: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest,
     ScriptStringCompoundNumericAdditionReportsAllocations) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptStringNumericAdditionProbe : ArtifactBehaviour
{
    public string source = "seed";
    public int suffix = 7;
    public string observed = "";
    void OnUpdate()
    {
        observed = source;
        observed += suffix;
    }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 'a');
    instance.fields()["suffix"] = std::int64_t{7};
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 3)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, allocationIterations * 176)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")),
              std::string(128, 'a'));
    EXPECT_EQ(std::get<std::int64_t>(instance.fields().at("suffix")), 7);
    EXPECT_EQ(std::get<std::string>(instance.fields().at("observed")),
              std::string(128, 'a') + "7");
    std::cout << "ArtifactScript long-string += integer: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest, ScriptMethodCallAvoidsSteadyStateAllocations) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptMethodAllocationProbe : ArtifactBehaviour
{
    public float result = 0.0;
    float addOne(float value) { return value + 1.0; }
    void OnUpdate() { result = addOne(result); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, 0)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, 0)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 1100.0);
}

TEST(LayerScriptComponentContractTest,
     ScriptObjectMethodMovesLongStringArgumentsIntoParameters) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptStringArgumentProbe : ArtifactBehaviour
{
    public ObjectRef target;
    public string source = "seed";
    void OnCreate() { target = new ScriptStringSink(); }
    void OnUpdate() { target.consume(source); }
}
class ScriptStringSink : ArtifactBehaviour
{
    void consume(string value) { }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate))
        << instance.lastError();
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 'x');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 4)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_GT(allocations.second, 0u)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")).size(), 128u);
}

TEST(LayerScriptComponentContractTest,
     ScriptDirectMethodMovesLongStringArgumentsIntoParameters) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptDirectStringArgumentProbe : ArtifactBehaviour
{
    public string source = "seed";
    void consume(string value) { }
    void OnUpdate() { consume(source); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 'x');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    constexpr int timingRepetitions = 3;
    constexpr int timingIterations = 10000;
    double totalMicroseconds = 0.0;
    for (int repetition = 0; repetition < timingRepetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < timingIterations; ++i) {
            ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
                << instance.lastError();
        }
        totalMicroseconds += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
    }
    std::cout << "ArtifactScript direct method long-string arguments: "
              << totalMicroseconds / (timingRepetitions * timingIterations)
              << " us/hook\n";

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 4)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")).size(), 128u);
    std::cout << "ArtifactScript direct method long-string arguments: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest,
     ScriptConstructorMovesLongStringArgumentsIntoParameters) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptStringConstructorProbe : ArtifactBehaviour
{
    public ObjectRef target;
    public string source = "seed";
    void OnUpdate() { target = new ScriptStringConstructorSink(source); }
}
class ScriptStringConstructorSink : ArtifactBehaviour
{
    public string value = "";
    void OnConstruct(string input) { this.value = input; }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["source"] = std::string(128, 'x');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }
    const auto target = std::get<ArtifactScriptObjectInstancePtr>(
        instance.fields().at("target"));
    ASSERT_TRUE(target);
    ASSERT_EQ(std::get<std::string>(target->fields.at("value")).size(), 128u);

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 22)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_GT(allocations.second, 0u)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("source")).size(), 128u);
    std::cout << "ArtifactScript constructor long-string arguments: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest,
     ScriptDirectMethodMovesMultipleLongStringArgumentsIntoParameters) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptMultipleStringArgumentProbe : ArtifactBehaviour
{
    public string first = "seed";
    public string second = "seed";
    void consume(string a, string b) { }
    void OnUpdate() { consume(first, second); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
        << instance.lastError();
    instance.fields()["first"] = std::string(128, 'a');
    instance.fields()["second"] = std::string(128, 'b');
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 8)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_GT(allocations.second, 0u)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_EQ(std::get<std::string>(instance.fields().at("first")).size(), 128u);
    EXPECT_EQ(std::get<std::string>(instance.fields().at("second")).size(), 128u);
    std::cout << "ArtifactScript multiple direct string arguments: "
              << allocations.first / allocationIterations << " allocations/hook, "
              << allocations.second / allocationIterations << " bytes/hook\n";
}

TEST(LayerScriptComponentContractTest, ScriptIsOperatorAvoidsSteadyStateAllocations) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptIsAllocationProbe : ArtifactBehaviour
{
    public ObjectRef target;
    public bool matchesBase = false;
    void OnCreate() { target = new LongNamedScriptChildClass(); }
    void OnUpdate() { matchesBase = target is LongNamedScriptBaseClass; }
}
class LongNamedScriptBaseClass : ArtifactBehaviour
{
}
class LongNamedScriptChildClass : LongNamedScriptBaseClass
{
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnCreate)) << instance.lastError();
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    }
    EXPECT_TRUE(std::get<bool>(instance.fields().at("matchesBase")));

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, 0)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, 0)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_TRUE(std::get<bool>(instance.fields().at("matchesBase")));
}

TEST(LayerScriptComponentContractTest,
     ScriptObjectConstructionReportsSteadyStateAllocations) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class ScriptObjectConstructionProbe : ArtifactBehaviour
{
    public ObjectRef target;
    void OnUpdate() { target = new Thing(); }
}
class Thing : ArtifactBehaviour
{
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, allocationIterations * 7)
        << "allocations/hook=" << allocations.first / allocationIterations;
}

TEST(LayerScriptComponentContractTest, DeepRecursiveCallsReuseOverflowWorkspaces) {
    ArtifactScriptParser parser;
    auto definition = parser.parse(R"(
class DeepRecursiveAllocationProbe : ArtifactBehaviour
{
    public float result = 0.0;
    float recurse(float depth, float a, float b, float c, float d, float e, float f)
    {
        if (depth <= 0.0) { return a + b + c + d + e + f; }
        return recurse(depth - 1.0, a, b, c, d, e, f) + 1.0;
    }
    void OnUpdate() { result = recurse(8.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0); }
}
)");
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    }

    ScriptAllocationCounter counter;
    constexpr std::size_t allocationIterations = 1000;
    bool succeeded = true;
    for (std::size_t i = 0; i < allocationIterations; ++i) {
        if (!instance.invokeHook(ArtifactScriptHook::OnUpdate)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, 0)
        << "allocations/hook=" << allocations.first / allocationIterations;
    EXPECT_EQ(allocations.second, 0)
        << "bytes/hook=" << allocations.second / allocationIterations;
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 29.0);
}

TEST(LayerScriptComponentContractTest, DeepCallChainReusesLocalOverflowWorkspace) {
    constexpr std::array<std::string_view, 13> parameterNames{
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m"};
    std::string source = R"(
class DeepCallChainAllocationProbe : ArtifactBehaviour
{
    public float result = 0.0;
)";
    const auto appendArguments = [&](bool useParameterNames) {
        for (std::size_t i = 0; i < parameterNames.size(); ++i) {
            if (i != 0) source += ", ";
            if (useParameterNames) source += parameterNames[i];
            else source += std::to_string(i + 1) + ".0";
        }
    };
    for (int methodIndex = 0; methodIndex < 9; ++methodIndex) {
        source += "    float step" + std::to_string(methodIndex) + "(";
        appendArguments(true);
        source += ") { return ";
        if (methodIndex < 8) {
            source += "step" + std::to_string(methodIndex + 1) + "(";
            appendArguments(true);
            source += "); }\n";
        } else {
            for (std::size_t i = 0; i < parameterNames.size(); ++i) {
                if (i != 0) source += " + ";
                source += parameterNames[i];
            }
            source += "; }\n";
        }
    }
    source += "    void OnUpdate() { result = step0(";
    appendArguments(false);
    source += "); }\n}\n";

    ArtifactScriptParser parser;
    auto definition = parser.parse(source);
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate)) << instance.lastError();
    }

    ScriptAllocationCounter counter;
    const bool succeeded = instance.invokeHook(ArtifactScriptHook::OnUpdate);
    const auto allocations = counter.stop();
    EXPECT_TRUE(succeeded) << instance.lastError();
    EXPECT_EQ(allocations.first, 0) << "allocations/hook=" << allocations.first;
    EXPECT_EQ(allocations.second, 0) << "bytes/hook=" << allocations.second;
    EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 91.0);
}

#endif

TEST(LayerScriptComponentContractTest,
     LocalsOverflowIndexAndFallbackPreserveVariableLookup) {
    std::string source = R"(
class WideLocalsLookupProbe : ArtifactBehaviour
{
    public float result = 0.0;
    float accumulate()
    {
)";
    for (int i = 0; i < 48; ++i) {
        source += "        float value" + std::to_string(i) + " = " +
                  std::to_string(i + 1) + ".0;\n";
    }
    source += R"(
        int iteration = 0;
        float total = 0.0;
        while (iteration < 10)
        {
            total += value0 + value1 + value2 + value3 + value4 + value5 +
                     value6 + value7 + value8 + value9 + value10 + value11 +
                     value12 + value13 + value14 + value15 + value16 + value17 +
                     value18 + value19 + value20 + value21 + value22 + value23 +
                     value24 + value25 + value26 + value27 + value28 + value29 +
                     value30 + value31 + value32 + value33 + value34 + value35 +
                     value36 + value37 + value38 + value39 + value40 + value41 +
                     value42 + value43 + value44 + value45 + value46 + value47;
            iteration += 1;
        }
        return total;
    }
    void OnUpdate() { result = accumulate(); }
}
)";

    ArtifactScriptParser parser;
    auto definition = parser.parse(source);
    ASSERT_TRUE(definition.diagnostics.empty());

    ArtifactScriptInstance instance(std::move(definition));
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(instance.invokeHook(ArtifactScriptHook::OnUpdate))
            << instance.lastError();
        EXPECT_DOUBLE_EQ(std::get<double>(instance.fields().at("result")), 11760.0);
    }
}
