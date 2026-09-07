#include <gtest/gtest.h>
#include <vector>
#include <atomic>
#include <algorithm>
#include <QString>
#include <taskflow/taskflow.hpp>

import Core.TaskSystem;
import Layer.Matte;
import FloatRGBA;
import Utils.Id;

using namespace ArtifactCore;

TEST(TaskSystemMattePoC, DagExecutesWithDependencies) {
    TaskSystem& sys = TaskSystem::globalInstance();
    std::atomic<int> counter{0};

    tf::Taskflow flow;
    auto A = flow.emplace([&](){ counter.fetch_add(1); });
    auto B = flow.emplace([&](){ counter.fetch_add(10); });
    auto C = flow.emplace([&](){ counter.fetch_add(100); });
    auto D = flow.emplace([&](){ EXPECT_GE(counter.load(), 111); });

    A.precede(B, C);
    D.succeed(B, C);

    sys.run(flow).wait();
    EXPECT_EQ(counter.load(), 111);
}

TEST(TaskSystemMattePoC, CorunAvoidsDeadlock) {
    TaskSystem& sys = TaskSystem::globalInstance();
    tf::Taskflow outer;
    std::atomic<int> done{0};

    for (int i = 0; i < 4; ++i) {
        outer.emplace([&sys, &done](){
            tf::Taskflow inner;
            inner.emplace([&done](){ done.fetch_add(1); });
            sys.corun(inner);
        });
    }
    sys.run(outer).wait();
    EXPECT_EQ(done.load(), 4);
}

TEST(TaskSystemMattePoC, ParallelMatteStackMatchesSequential) {
    // Simulate 3 matte sources (alpha masks) and combine via Add/Common/Subtract
    const int W = 64, H = 64;
    const size_t N = static_cast<size_t>(W*H);
    std::vector<float> src0(N, 0.3f), src1(N, 0.5f), src2(N, 0.2f);

    MatteStack stack;
    stack.setStackMode(MatteStackMode::Add);
    MatteNode n0; n0.setMode(MatteMode::Alpha); n0.setEnabled(true);
    MatteNode n1; n1.setMode(MatteMode::Alpha); n1.setEnabled(true);
    MatteNode n2; n2.setMode(MatteMode::Alpha); n2.setEnabled(true);
    n0.setSourceLayerId(Id(QStringLiteral("11111111-1111-1111-1111-111111111111")));
    n1.setSourceLayerId(Id(QStringLiteral("22222222-2222-2222-2222-222222222222")));
    n2.setSourceLayerId(Id(QStringLiteral("33333333-3333-3333-3333-333333333333")));
    stack.addNode(n0); stack.addNode(n1); stack.addNode(n2);

    std::vector<std::vector<float>> sources = {src0, src1, src2};
    auto seq = evaluateMatteStack(sources, stack, W, H);
    ASSERT_TRUE(seq.isValid());

    // Parallel version via TaskSystem: each source mask built in parallel task, then combine
    std::vector<std::vector<float>> parallelMasks(3, std::vector<float>(N));
    {
        tf::Taskflow flow;
        for (int i = 0; i < 3; ++i) {
            flow.emplace([&, i](){
                const auto& src = sources[i];
                auto& dst = parallelMasks[i];
                for (size_t p = 0; p < N; ++p) dst[p] = std::clamp(src[p], 0.0f, 1.0f);
            });
        }
        TaskSystem::globalInstance().run(flow).wait();
    }
    // Combine on main thread with same logic as evaluateMatteStack (Add)
    std::vector<float> combined(N, 0.0f);
    for (size_t p = 0; p < N; ++p) {
        float v = parallelMasks[0][p];
        v = MatteEvaluator::combine(v, parallelMasks[1][p], MatteStackMode::Add);
        v = MatteEvaluator::combine(v, parallelMasks[2][p], MatteStackMode::Add);
        combined[p] = v;
    }
    for (size_t p = 0; p < N; ++p) {
        EXPECT_FLOAT_EQ(combined[p], seq.alphaMask[p]);
    }
    // Expected: 0.3+0.5+0.2 = 1.0 clamped
    EXPECT_FLOAT_EQ(seq.alphaMask[0], 1.0f);
}

TEST(TaskSystemMattePoC, ForEachViaTaskflow) {
    std::atomic<int> sum{0};
    tf::Taskflow flow;
    for (int i = 0; i < 10; ++i) {
        flow.emplace([&sum](){ sum.fetch_add(100); });
    }
    TaskSystem::globalInstance().run(flow).wait();
    EXPECT_EQ(sum.load(), 1000);
}
