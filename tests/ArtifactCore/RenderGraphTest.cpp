#include <gtest/gtest.h>

#include <future>
#include <string>
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
