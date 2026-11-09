#include <gtest/gtest.h>

#include <array>
#include <future>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

import Graphics.RenderGraph;

using namespace ArtifactCore;

namespace {

// Runs tasks inline: deterministic, no threads. Satisfies the duck-typed
// launcher concept of RenderGraph::executeParallel.
struct InlineLauncher {
    template <typename F>
    auto async(F&& task) -> std::future<decltype(task())>
    {
        std::packaged_task<decltype(task())()> packaged(std::forward<F>(task));
        auto future = packaged.get_future();
        packaged();
        return future;
    }
};

struct TwoWayInlineLauncher {
    std::size_t concurrency() const noexcept { return 2; }

    template <typename F>
    auto async(F&& task) -> std::future<decltype(task())>
    {
        std::packaged_task<decltype(task())()> packaged(std::forward<F>(task));
        auto future = packaged.get_future();
        packaged();
        return future;
    }
};

struct ThrowOnSecondDispatchLauncher {
    std::size_t concurrency() const noexcept { return 2; }

    template <typename F>
    auto async(F&& task) -> std::future<decltype(task())>
    {
        if (dispatchCount++ == 1) {
            throw std::runtime_error("simulated dispatch failure");
        }
        return std::async(std::launch::deferred, std::forward<F>(task));
    }

    std::size_t dispatchCount = 0;
};

RenderResourceDescriptor transientTexture(const std::string& name)
{
    return {name, RenderResourceKind::Texture,
            RenderResourceLifetime::Transient, 320, 180, 1, 1, 320u * 180u * 4u};
}

RenderResourceDescriptor externalTexture(const std::string& name)
{
    return {name, RenderResourceKind::Texture,
            RenderResourceLifetime::External, 320, 180, 1, 1, 320u * 180u * 4u};
}

RenderPassDescriptor graphicsPass(const std::string& name)
{
    return {name, RenderPassQueue::Graphics, {}, {}, true};
}

std::vector<std::string> passNames(const RenderGraph& graph,
                                   const std::vector<RenderPassHandle>& order)
{
    std::vector<std::string> names;
    for (const auto handle : order) {
        const auto* descriptor = graph.pass(handle);
        names.push_back(descriptor ? descriptor->name : std::string());
    }
    return names;
}

} // namespace

TEST(RenderGraphCompileTest, LinearChainKeepsOrderAndLevels)
{
    RenderGraph graph;
    const auto begin = graph.addResource(externalTexture("Begin"));
    const auto middle = graph.addResource(transientTexture("Middle"));
    const auto end = graph.addResource(transientTexture("End"));

    auto first = graphicsPass("first");
    first.writes = {middle};
    graph.addPass(first);
    auto second = graphicsPass("second");
    second.reads = {middle};
    second.writes = {end};
    graph.addPass(second);
    (void)begin;

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    ASSERT_EQ(compiled.passOrder.size(), 2);
    EXPECT_EQ(passNames(graph, compiled.passOrder),
              (std::vector<std::string>{"first", "second"}));
    ASSERT_EQ(compiled.executionLevels.size(), 2);
    EXPECT_EQ(compiled.executionLevels[0].size(), 1);
    EXPECT_EQ(compiled.executionLevels[1].size(), 1);
}

TEST(RenderGraphCompileTest, DiamondConvergesIntoSharedLevel)
{
    RenderGraph graph;
    const auto root = graph.addResource(externalTexture("root"));
    const auto left = graph.addResource(transientTexture("left"));
    const auto right = graph.addResource(transientTexture("right"));
    const auto joined = graph.addResource(transientTexture("joined"));

    auto emit = graphicsPass("emit");
    emit.reads = {root};
    emit.writes = {left, right};
    graph.addPass(emit);
    // A second writer of the same resource serializes after the first one.
    auto forkLeft = graphicsPass("forkLeft");
    forkLeft.reads = {left};
    forkLeft.writes = {joined};
    graph.addPass(forkLeft);
    auto forkRight = graphicsPass("forkRight");
    forkRight.reads = {right};
    // Writes to a fresh resource so both forks stay in the same level.
    const auto rightOut = graph.addResource(transientTexture("rightOut"));
    forkRight.writes = {rightOut};
    graph.addPass(forkRight);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    ASSERT_EQ(compiled.passOrder.size(), 3);
    ASSERT_EQ(compiled.executionLevels.size(), 2);
    EXPECT_EQ(compiled.executionLevels[0].size(), 1);
    EXPECT_EQ(compiled.executionLevels[1].size(), 2);
    EXPECT_EQ(passNames(graph, compiled.passOrder).front(), std::string("emit"));
}

TEST(RenderGraphCompileTest, DisabledPassesAreExcluded)
{
    RenderGraph graph;
    const auto token = graph.addResource(transientTexture("token"));
    const auto out = graph.addResource(transientTexture("out"));

    auto active = graphicsPass("active");
    active.writes = {token};
    graph.addPass(active);
    auto skipped = graphicsPass("skipped");
    skipped.enabled = false;
    skipped.reads = {token};
    skipped.writes = {out};
    graph.addPass(skipped);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    EXPECT_EQ(passNames(graph, compiled.passOrder),
              (std::vector<std::string>{"active"}));
    ASSERT_EQ(compiled.executionLevels.size(), 1);
}

TEST(RenderGraphCompileTest, DisabledWriterDoesNotInitializeTransientResource)
{
    RenderGraph graph;
    const auto token = graph.addResource(transientTexture("token"));

    auto skippedWriter = graphicsPass("skippedWriter");
    skippedWriter.enabled = false;
    skippedWriter.writes = {token};
    graph.addPass(skippedWriter);

    auto reader = graphicsPass("reader");
    reader.reads = {token};
    graph.addPass(reader);

    const CompiledRenderGraph compiled = graph.compile();

    EXPECT_FALSE(compiled.valid);
    EXPECT_FALSE(compiled.error.empty());
}

TEST(RenderGraphCompileTest, TransientReadBeforeWriteFails)
{
    RenderGraph graph;
    const auto token = graph.addResource(transientTexture("token"));
    auto reader = graphicsPass("reader");
    reader.reads = {token};
    graph.addPass(reader);

    const CompiledRenderGraph compiled = graph.compile();
    EXPECT_FALSE(compiled.valid);
    EXPECT_FALSE(compiled.error.empty());
}

TEST(RenderGraphCompileTest, UnknownResourceFails)
{
    RenderGraph graph;
    auto writer = graphicsPass("writer");
    writer.writes = {RenderResourceHandle{999}};
    graph.addPass(writer);

    const CompiledRenderGraph compiled = graph.compile();
    EXPECT_FALSE(compiled.valid);
    EXPECT_FALSE(compiled.error.empty());
}

TEST(RenderGraphCompileTest, ExternalResourceCanBeReadAndWrittenByOnePass)
{
    RenderGraph graph;
    const auto external = graph.addResource(externalTexture("in-place"));
    auto inPlace = graphicsPass("in-place");
    inPlace.reads = {external};
    inPlace.writes = {external};
    graph.addPass(inPlace);

    const CompiledRenderGraph compiled = graph.compile();

    ASSERT_TRUE(compiled.valid) << compiled.error;
    ASSERT_EQ(compiled.passOrder.size(), 1);
    EXPECT_EQ(passNames(graph, compiled.passOrder),
              (std::vector<std::string>{"in-place"}));
    ASSERT_EQ(compiled.executionLevels.size(), 1);
    EXPECT_EQ(compiled.executionLevels.front().size(), 1);
}

TEST(RenderGraphCompileTest, ExternalReadIsOrderedBeforeItsLaterOverwrite)
{
    RenderGraph graph;
    const auto external = graph.addResource(externalTexture("external"));

    auto readExisting = graphicsPass("readExisting");
    readExisting.reads = {external};
    graph.addPass(readExisting);

    auto overwrite = graphicsPass("overwrite");
    overwrite.writes = {external};
    graph.addPass(overwrite);

    const CompiledRenderGraph compiled = graph.compile();

    ASSERT_TRUE(compiled.valid) << compiled.error;
    EXPECT_EQ(passNames(graph, compiled.passOrder),
              (std::vector<std::string>{"readExisting", "overwrite"}));
    ASSERT_EQ(compiled.executionLevels.size(), 2);
    EXPECT_EQ(compiled.executionLevels[0].size(), 1);
    EXPECT_EQ(compiled.executionLevels[1].size(), 1);
}

TEST(RenderGraphCompileTest, EmptyGraphCompiles)
{
    const RenderGraph graph;
    const CompiledRenderGraph compiled = graph.compile();
    EXPECT_TRUE(compiled.valid);
    EXPECT_TRUE(compiled.passOrder.empty());
    EXPECT_TRUE(compiled.executionLevels.empty());
}

TEST(RenderGraphAliasTest, NonOverlappingTransientsShareSlot)
{
    RenderGraph graph;
    const auto first = graph.addResource(transientTexture("first"));
    const auto fence = graph.addResource(transientTexture("fence"));
    const auto second = graph.addResource(transientTexture("second"));

    auto produceFirst = graphicsPass("produceFirst");
    produceFirst.writes = {first};
    graph.addPass(produceFirst);
    auto consumeFirst = graphicsPass("consumeFirst");
    consumeFirst.reads = {first};
    consumeFirst.writes = {fence};
    graph.addPass(consumeFirst);
    auto produceSecond = graphicsPass("produceSecond");
    produceSecond.reads = {fence};
    produceSecond.writes = {second};
    graph.addPass(produceSecond);
    auto consumeSecond = graphicsPass("consumeSecond");
    consumeSecond.reads = {second};
    graph.addPass(consumeSecond);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    const auto* firstLifetime = compiled.lifetime(first);
    const auto* secondLifetime = compiled.lifetime(second);
    ASSERT_NE(firstLifetime, nullptr);
    ASSERT_NE(secondLifetime, nullptr);
    EXPECT_EQ(firstLifetime->allocationSlot, secondLifetime->allocationSlot);
    // The fence overlaps both chains, so exactly two slots are needed.
    EXPECT_EQ(compiled.allocationSlotCount, 2);
}

TEST(RenderGraphAliasTest, NonOverlappingTexturesWithDifferentDimensionsDoNotAlias)
{
    RenderGraph graph;
    const auto first = graph.addResource(transientTexture("first"));
    const auto fence = graph.addResource(transientTexture("fence"));
    auto largerDescriptor = transientTexture("second");
    largerDescriptor.width = 640;
    largerDescriptor.byteSize = 640u * 180u * 4u;
    const auto second = graph.addResource(largerDescriptor);

    auto produceFirst = graphicsPass("produceFirst");
    produceFirst.writes = {first};
    graph.addPass(produceFirst);
    auto consumeFirst = graphicsPass("consumeFirst");
    consumeFirst.reads = {first};
    consumeFirst.writes = {fence};
    graph.addPass(consumeFirst);
    auto produceSecond = graphicsPass("produceSecond");
    produceSecond.reads = {fence};
    produceSecond.writes = {second};
    graph.addPass(produceSecond);
    auto consumeSecond = graphicsPass("consumeSecond");
    consumeSecond.reads = {second};
    graph.addPass(consumeSecond);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    const auto* firstLifetime = compiled.lifetime(first);
    const auto* secondLifetime = compiled.lifetime(second);
    ASSERT_NE(firstLifetime, nullptr);
    ASSERT_NE(secondLifetime, nullptr);
    EXPECT_NE(firstLifetime->allocationSlot, secondLifetime->allocationSlot);
    EXPECT_EQ(compiled.allocationSlotCount, 3u)
        << "the overlapping fence needs one slot and incompatible textures need distinct slots";
}

TEST(RenderGraphAliasTest, UnknownFormatTexturesDoNotAliasAcrossNonOverlappingLifetimes)
{
    RenderGraph graph;
    auto firstDescriptor = transientTexture("first-unknown-format");
    firstDescriptor.format = 0;
    const auto first = graph.addResource(firstDescriptor);
    const auto fence = graph.addResource(transientTexture("fence"));
    auto secondDescriptor = transientTexture("second-unknown-format");
    secondDescriptor.format = 0;
    const auto second = graph.addResource(secondDescriptor);

    auto produceFirst = graphicsPass("produceFirst");
    produceFirst.writes = {first};
    graph.addPass(produceFirst);
    auto consumeFirst = graphicsPass("consumeFirst");
    consumeFirst.reads = {first};
    consumeFirst.writes = {fence};
    graph.addPass(consumeFirst);
    auto produceSecond = graphicsPass("produceSecond");
    produceSecond.reads = {fence};
    produceSecond.writes = {second};
    graph.addPass(produceSecond);
    auto consumeSecond = graphicsPass("consumeSecond");
    consumeSecond.reads = {second};
    graph.addPass(consumeSecond);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    const auto* firstLifetime = compiled.lifetime(first);
    const auto* secondLifetime = compiled.lifetime(second);
    ASSERT_NE(firstLifetime, nullptr);
    ASSERT_NE(secondLifetime, nullptr);
    EXPECT_NE(firstLifetime->allocationSlot, secondLifetime->allocationSlot);
    EXPECT_EQ(compiled.allocationSlotCount, 3u)
        << "unknown texture formats are not proven compatible for aliasing";
}

TEST(RenderGraphAliasTest, OverlappingTransientsUseSeparateSlots)
{
    RenderGraph graph;
    const auto first = graph.addResource(transientTexture("first"));
    const auto second = graph.addResource(transientTexture("second"));

    auto produceBoth = graphicsPass("produceBoth");
    produceBoth.writes = {first, second};
    graph.addPass(produceBoth);
    auto consumeBoth = graphicsPass("consumeBoth");
    consumeBoth.reads = {first, second};
    graph.addPass(consumeBoth);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    const auto* firstLifetime = compiled.lifetime(first);
    const auto* secondLifetime = compiled.lifetime(second);
    ASSERT_NE(firstLifetime, nullptr);
    ASSERT_NE(secondLifetime, nullptr);
    EXPECT_NE(firstLifetime->allocationSlot, secondLifetime->allocationSlot);
    EXPECT_EQ(compiled.allocationSlotCount, 2);
}

TEST(RenderGraphAliasTest,
     ResourcesUsedOnOppositeSidesOfSamePassCannotShareAllocation)
{
    RenderGraph graph;
    const auto first = graph.addResource(transientTexture("first"));
    const auto second = graph.addResource(transientTexture("second"));

    auto produceFirst = graphicsPass("produceFirst");
    produceFirst.writes = {first};
    graph.addPass(produceFirst);
    auto handoff = graphicsPass("readFirstWriteSecond");
    handoff.reads = {first};
    handoff.writes = {second};
    graph.addPass(handoff);
    auto consumeSecond = graphicsPass("consumeSecond");
    consumeSecond.reads = {second};
    graph.addPass(consumeSecond);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    const auto* firstLifetime = compiled.lifetime(first);
    const auto* secondLifetime = compiled.lifetime(second);
    ASSERT_NE(firstLifetime, nullptr);
    ASSERT_NE(secondLifetime, nullptr);
    EXPECT_EQ(firstLifetime->lastPass, secondLifetime->firstPass);
    EXPECT_NE(firstLifetime->allocationSlot, secondLifetime->allocationSlot)
        << "the handoff pass reads the first resource while writing the second";
}

TEST(RenderGraphAliasTest, ExternalResourcesKeepDedicatedSlots)
{
    RenderGraph graph;
    const auto first = graph.addResource(externalTexture("first"));
    const auto second = graph.addResource(externalTexture("second"));

    auto produceFirst = graphicsPass("produceFirst");
    produceFirst.writes = {first};
    graph.addPass(produceFirst);
    auto produceSecond = graphicsPass("produceSecond");
    produceSecond.writes = {second};
    graph.addPass(produceSecond);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    EXPECT_EQ(compiled.allocationSlotCount, 2);
    const auto* slot = compiled.allocationSlot(0);
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->kind, RenderResourceKind::Texture);
}

TEST(RenderGraphExecuteTest, RunsInCompiledOrder)
{
    RenderGraph graph;
    const auto token = graph.addResource(transientTexture("token"));
    for (const char* name : {"one", "two", "three"}) {
        auto pass = graphicsPass(name);
        pass.writes = {token};
        graph.addPass(pass);
    }

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;

    std::vector<std::string> calls;
    std::string error;
    EXPECT_TRUE(graph.execute(compiled,
                              [&](const RenderGraphExecutionContext& context) {
                                  calls.push_back(context.descriptor.name);
                                  return true;
                              },
                              &error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(calls, (std::vector<std::string>{"one", "two", "three"}));
}

TEST(RenderGraphExecuteTest, FailureStopsAndReports)
{
    RenderGraph graph;
    const auto token = graph.addResource(transientTexture("token"));
    for (const char* name : {"one", "two"}) {
        auto pass = graphicsPass(name);
        pass.writes = {token};
        graph.addPass(pass);
    }
    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;

    int calls = 0;
    std::string error;
    EXPECT_FALSE(graph.execute(compiled,
                               [&](const RenderGraphExecutionContext&) {
                                   return ++calls < 2;
                               },
                               &error));
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(error, "render pass executor failed");
}

TEST(RenderGraphExecuteTest, InvalidGraphAndEmptyExecutorFail)
{
    const RenderGraph graph;
    CompiledRenderGraph compiled;
    compiled.valid = false;
    compiled.error = "boom";
    std::string error;
    EXPECT_FALSE(graph.execute(compiled,
                               [](const RenderGraphExecutionContext&) {
                                   return true;
                               },
                               &error));
    EXPECT_EQ(error, "boom");

    const CompiledRenderGraph empty = graph.compile();
    ASSERT_TRUE(empty.valid);
    EXPECT_FALSE(graph.execute(empty, RenderPassExecutor{}, &error));
    EXPECT_EQ(error, "render graph executor is empty");
}

TEST(RenderGraphExecuteTest, MutationAfterCompileRejectsStalePlanBeforeExecution)
{
    RenderGraph graph;
    graph.addPass(graphicsPass("compiled-pass"));
    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;

    graph.addPass(graphicsPass("late-pass"));

    int calls = 0;
    std::string error;
    EXPECT_FALSE(graph.execute(compiled,
                               [&](const RenderGraphExecutionContext&) {
                                   ++calls;
                                   return true;
                               },
                               &error));
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(error, "compiled render graph is stale");
}

TEST(RenderGraphExecuteTest,
     CompiledPlanIdentityFollowsMoveButNotCopy)
{
    RenderGraph original;
    original.addPass(graphicsPass("identity-bound-pass"));
    const CompiledRenderGraph compiled = original.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;

    RenderGraph copied(original);
    int copiedExecutorCalls = 0;
    std::string error;
    EXPECT_FALSE(copied.execute(
        compiled,
        [&](const RenderGraphExecutionContext&) {
            ++copiedExecutorCalls;
            return true;
        },
        &error));
    EXPECT_EQ(copiedExecutorCalls, 0);
    EXPECT_EQ(error, "compiled render graph is stale");

    RenderGraph copyAssigned;
    copyAssigned = original;
    int copyAssignedExecutorCalls = 0;
    error.clear();
    EXPECT_FALSE(copyAssigned.execute(
        compiled,
        [&](const RenderGraphExecutionContext&) {
            ++copyAssignedExecutorCalls;
            return true;
        },
        &error));
    EXPECT_EQ(copyAssignedExecutorCalls, 0);
    EXPECT_EQ(error, "compiled render graph is stale");

    RenderGraph moved(std::move(original));
    int movedExecutorCalls = 0;
    error.clear();
    EXPECT_TRUE(moved.execute(
        compiled,
        [&](const RenderGraphExecutionContext& context) {
            ++movedExecutorCalls;
            return context.descriptor.name == "identity-bound-pass";
        },
        &error));
    EXPECT_EQ(movedExecutorCalls, 1);
    EXPECT_TRUE(error.empty());

    RenderGraph moveAssigned;
    moveAssigned = std::move(moved);
    int moveAssignedExecutorCalls = 0;
    error.clear();
    EXPECT_TRUE(moveAssigned.execute(
        compiled,
        [&](const RenderGraphExecutionContext& context) {
            ++moveAssignedExecutorCalls;
            return context.descriptor.name == "identity-bound-pass";
        },
        &error));
    EXPECT_EQ(moveAssignedExecutorCalls, 1);
    EXPECT_TRUE(error.empty());

    int moveAssignedFromExecutorCalls = 0;
    error.clear();
    EXPECT_FALSE(moved.execute(
        compiled,
        [&](const RenderGraphExecutionContext&) {
            ++moveAssignedFromExecutorCalls;
            return true;
        },
        &error));
    EXPECT_EQ(moveAssignedFromExecutorCalls, 0);
    EXPECT_EQ(error, "compiled render graph is stale");

    int movedFromExecutorCalls = 0;
    error.clear();
    EXPECT_FALSE(original.execute(
        compiled,
        [&](const RenderGraphExecutionContext&) {
            ++movedFromExecutorCalls;
            return true;
        },
        &error));
    EXPECT_EQ(movedFromExecutorCalls, 0);
    EXPECT_EQ(error, "compiled render graph is stale");
}

TEST(RenderGraphParallelTest, DiamondRunsWithInlineLauncher)
{
    RenderGraph graph;
    const auto root = graph.addResource(externalTexture("root"));
    const auto left = graph.addResource(transientTexture("left"));
    const auto right = graph.addResource(transientTexture("right"));

    auto emit = graphicsPass("emit");
    emit.reads = {root};
    emit.writes = {left, right};
    graph.addPass(emit);
    auto forkLeft = graphicsPass("forkLeft");
    forkLeft.reads = {left};
    graph.addPass(forkLeft);
    auto forkRight = graphicsPass("forkRight");
    forkRight.reads = {right};
    graph.addPass(forkRight);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    ASSERT_EQ(compiled.executionLevels.size(), 2);
    ASSERT_EQ(compiled.executionLevels[1].size(), 2);

    InlineLauncher launcher;
    std::vector<std::string> calls;
    std::string error;
    EXPECT_TRUE(graph.executeParallel(compiled,
                                      [&](const RenderGraphExecutionContext& context) {
                                          calls.push_back(context.descriptor.name);
                                          return true;
                                      },
                                      launcher, &error));
    EXPECT_TRUE(error.empty());
    ASSERT_EQ(calls.size(), 3);
    EXPECT_EQ(calls.front(), std::string("emit"));
}

TEST(RenderGraphParallelTest, FailurePropagates)
{
    RenderGraph graph;
    const auto root = graph.addResource(externalTexture("root"));
    const auto left = graph.addResource(transientTexture("left"));

    auto emit = graphicsPass("emit");
    emit.reads = {root};
    emit.writes = {left};
    graph.addPass(emit);
    auto consumer = graphicsPass("consumer");
    consumer.reads = {left};
    graph.addPass(consumer);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;

    InlineLauncher launcher;
    std::string error;
    EXPECT_FALSE(graph.executeParallel(
        compiled,
        [](const RenderGraphExecutionContext& context) {
            return context.descriptor.name != "consumer";
        },
        launcher, &error));
    EXPECT_EQ(error, "render pass executor failed");

    EXPECT_FALSE(graph.executeParallel(
        CompiledRenderGraph{},
        [](const RenderGraphExecutionContext&) { return true; }, launcher,
        &error));
}

TEST(RenderGraphParallelTest, DispatchFailureJoinsPendingWorkThenFallsBackToSerial)
{
    RenderGraph graph;
    const auto input = graph.addResource(externalTexture("input"));
    const auto outputA = graph.addResource(transientTexture("outputA"));
    const auto outputB = graph.addResource(transientTexture("outputB"));
    const auto outputC = graph.addResource(transientTexture("outputC"));

    auto produce = graphicsPass("produce");
    produce.reads = {input};
    produce.writes = {outputA, outputB, outputC};
    graph.addPass(produce);
    for (const auto& [name, output] : std::array{
             std::pair{"consumeA", outputA},
             std::pair{"consumeB", outputB},
             std::pair{"consumeC", outputC}}) {
        auto consumer = graphicsPass(name);
        consumer.reads = {output};
        consumer.parallelSafe = true;
        graph.addPass(consumer);
    }

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    ASSERT_EQ(compiled.executionLevels.size(), 2u);
    ASSERT_EQ(compiled.executionLevels[1].size(), 3u);

    ThrowOnSecondDispatchLauncher launcher;
    std::vector<std::string> calls;
    std::string error;
    EXPECT_TRUE(graph.executeParallel(
        compiled,
        [&](const RenderGraphExecutionContext& context) {
            calls.push_back(context.descriptor.name);
            return true;
        },
        launcher, &error));

    EXPECT_EQ(launcher.dispatchCount, 2u);
    EXPECT_EQ(calls, (std::vector<std::string>{
        "produce", "consumeA", "consumeB", "consumeC"}));
    EXPECT_TRUE(error.empty());
}

TEST(RenderGraphParallelTest, RejectsMutatedExecutionLevelWithResourceDependency)
{
    RenderGraph graph;
    const auto resource = graph.addResource(transientTexture("dependent-resource"));
    auto producer = graphicsPass("producer");
    producer.writes = {resource};
    const auto producerHandle = graph.addPass(producer);
    auto consumer = graphicsPass("consumer");
    consumer.reads = {resource};
    const auto consumerHandle = graph.addPass(consumer);

    CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    ASSERT_EQ(compiled.passOrder,
              (std::vector<RenderPassHandle>{producerHandle, consumerHandle}));

    // Preserve the flattened pass order while corrupting only the level
    // partition: the producer and its consumer are now declared concurrent.
    compiled.executionLevels = {{producerHandle, consumerHandle}};
    compiled.passOrderLevels = {0, 0};

    TwoWayInlineLauncher launcher;
    int executorCalls = 0;
    std::string error;
    EXPECT_FALSE(graph.executeParallel(
        compiled,
        [&](const RenderGraphExecutionContext&) {
            ++executorCalls;
            return true;
        },
        launcher, &error));
    EXPECT_EQ(executorCalls, 0);
    EXPECT_EQ(error,
              "compiled render graph execution level contains dependent passes");
}

TEST(RenderGraphParallelTest, FailedParallelLevelJoinsPeersAndSkipsDependents)
{
    RenderGraph graph;
    const auto input = graph.addResource(externalTexture("input"));
    const auto leftOutput = graph.addResource(transientTexture("leftOutput"));
    const auto rightOutput = graph.addResource(transientTexture("rightOutput"));

    auto emit = graphicsPass("emit");
    emit.reads = {input};
    emit.writes = {leftOutput, rightOutput};
    graph.addPass(emit);
    auto readLeft = graphicsPass("readLeft");
    readLeft.reads = {leftOutput};
    readLeft.parallelSafe = true;
    graph.addPass(readLeft);
    auto readRight = graphicsPass("readRight");
    readRight.reads = {rightOutput};
    readRight.parallelSafe = true;
    graph.addPass(readRight);
    auto dependent = graphicsPass("dependent");
    dependent.reads = {leftOutput, rightOutput};
    graph.addPass(dependent);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    ASSERT_EQ(compiled.executionLevels.size(), 3u);
    ASSERT_EQ(compiled.executionLevels[1].size(), 2u);

    TwoWayInlineLauncher launcher;
    std::vector<std::string> calls;
    std::string error;
    EXPECT_FALSE(graph.executeParallel(
        compiled,
        [&](const RenderGraphExecutionContext& context) {
            calls.push_back(context.descriptor.name);
            return context.descriptor.name != "readLeft";
        },
        launcher, &error));

    EXPECT_EQ(calls.size(), 3u);
    EXPECT_NE(std::find(calls.begin(), calls.end(), "emit"), calls.end());
    EXPECT_NE(std::find(calls.begin(), calls.end(), "readLeft"), calls.end());
    EXPECT_NE(std::find(calls.begin(), calls.end(), "readRight"), calls.end());
    EXPECT_EQ(std::find(calls.begin(), calls.end(), "dependent"), calls.end());
    EXPECT_EQ(error, "render pass executor failed");
}

TEST(RenderGraphParallelTest, ThrownParallelExecutorJoinsPeersAndSkipsDependents)
{
    RenderGraph graph;
    const auto input = graph.addResource(externalTexture("input"));
    const auto leftOutput = graph.addResource(transientTexture("leftOutput"));
    const auto rightOutput = graph.addResource(transientTexture("rightOutput"));

    auto emit = graphicsPass("emit");
    emit.reads = {input};
    emit.writes = {leftOutput, rightOutput};
    graph.addPass(emit);
    auto readLeft = graphicsPass("readLeft");
    readLeft.reads = {leftOutput};
    readLeft.parallelSafe = true;
    graph.addPass(readLeft);
    auto readRight = graphicsPass("readRight");
    readRight.reads = {rightOutput};
    readRight.parallelSafe = true;
    graph.addPass(readRight);
    auto dependent = graphicsPass("dependent");
    dependent.reads = {leftOutput, rightOutput};
    graph.addPass(dependent);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    ASSERT_EQ(compiled.executionLevels.size(), 3u);
    ASSERT_EQ(compiled.executionLevels[1].size(), 2u);

    TwoWayInlineLauncher launcher;
    std::vector<std::string> calls;
    std::string error;
    EXPECT_FALSE(graph.executeParallel(
        compiled,
        [&](const RenderGraphExecutionContext& context) {
            calls.push_back(context.descriptor.name);
            if (context.descriptor.name == "readLeft") {
                throw std::runtime_error("simulated pass failure");
            }
            return true;
        },
        launcher, &error));

    EXPECT_EQ(calls.size(), 3u);
    EXPECT_NE(std::find(calls.begin(), calls.end(), "emit"), calls.end());
    EXPECT_NE(std::find(calls.begin(), calls.end(), "readLeft"), calls.end());
    EXPECT_NE(std::find(calls.begin(), calls.end(), "readRight"), calls.end());
    EXPECT_EQ(std::find(calls.begin(), calls.end(), "dependent"), calls.end());
    EXPECT_EQ(error, "render pass executor failed");
}

TEST(RenderGraphDiagnosticTest, SnapshotMarksScheduledAndDisabled)
{
    RenderGraph graph;
    const auto token = graph.addResource(transientTexture("token"));
    auto active = graphicsPass("active");
    active.writes = {token};
    graph.addPass(active);
    auto skipped = graphicsPass("skipped");
    skipped.enabled = false;
    graph.addPass(skipped);

    const CompiledRenderGraph compiled = graph.compile();
    ASSERT_TRUE(compiled.valid) << compiled.error;
    const RenderGraphDiagnosticSnapshot snapshot =
        graph.diagnosticSnapshot(compiled, 7);
    EXPECT_EQ(snapshot.executionId, 7u);
    EXPECT_TRUE(snapshot.valid);
    ASSERT_EQ(snapshot.passes.size(), 2);

    const RenderDiagnosticPassRecord* activeRecord = nullptr;
    const RenderDiagnosticPassRecord* skippedRecord = nullptr;
    for (const auto& record : snapshot.passes) {
        if (record.descriptor.name == "active") activeRecord = &record;
        if (record.descriptor.name == "skipped") skippedRecord = &record;
    }
    ASSERT_NE(activeRecord, nullptr);
    ASSERT_NE(skippedRecord, nullptr);
    EXPECT_EQ(activeRecord->state, RenderDiagnosticPassState::Scheduled);
    EXPECT_EQ(activeRecord->executionOrder, 0u);
    EXPECT_EQ(skippedRecord->state, RenderDiagnosticPassState::Disabled);
    EXPECT_FALSE(skippedRecord->stateReason.empty());
}
