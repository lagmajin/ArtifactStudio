#include <gtest/gtest.h>

#include <vector>

import Graphics.GPUThreadPool;

using ArtifactCore::GPUJobPriority;
using ArtifactCore::GPUThreadPool;

TEST(GPUThreadPoolContractTest, RejectsEmptyJobsAndBoundsThePendingQueue)
{
    GPUThreadPool pool(2);
    EXPECT_FALSE(pool.enqueue({}).isValid());
    EXPECT_EQ(pool.snapshot().queued, 0u);

    int executions = 0;
    auto first = pool.enqueue([&] { ++executions; }, GPUJobPriority::Low);
    auto second = pool.enqueue([&] { ++executions; }, GPUJobPriority::High);
    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());
    EXPECT_FALSE(pool.enqueue([&] { ++executions; }).isValid());
    EXPECT_EQ(pool.snapshot().queued, 2u);
    EXPECT_EQ(pool.maxQueuedJobs(), 2u);
    EXPECT_FALSE(first.isSubmitted());
    EXPECT_FALSE(first.isCompleted());
    EXPECT_EQ(first.id(), 1u);
    EXPECT_EQ(second.id(), 2u);
}

TEST(GPUThreadPoolContractTest, DrainBudgetPreservesFifoOrderAndUpdatesHandlesAndStatistics)
{
    GPUThreadPool pool(4);
    std::vector<int> order;
    auto first = pool.enqueue([&] { order.push_back(1); }, GPUJobPriority::High);
    auto second = pool.enqueue([&] { order.push_back(2); }, GPUJobPriority::Low);
    auto third = pool.enqueue([&] { order.push_back(3); }, GPUJobPriority::Normal);

    int executorCalls = 0;
    const auto executor = [&](const GPUThreadPool::GPUJob& job) {
        ++executorCalls;
        job();
        return true;
    };

    EXPECT_EQ(pool.drain(executor, 0), 0u);
    EXPECT_EQ(pool.drain(executor, 2), 2u);
    EXPECT_EQ(executorCalls, 2);
    EXPECT_TRUE(first.isSubmitted());
    EXPECT_TRUE(first.isCompleted());
    EXPECT_TRUE(second.isCompleted());
    EXPECT_FALSE(third.isSubmitted());
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);

    const auto partial = pool.snapshot();
    EXPECT_EQ(partial.queued, 1u);
    EXPECT_EQ(partial.submitted, 2u);
    EXPECT_EQ(partial.completed, 2u);
    EXPECT_EQ(partial.failed, 0u);
    EXPECT_EQ(partial.cancelled, 0u);

    EXPECT_EQ(pool.drain(executor), 1u);
    EXPECT_TRUE(third.isCompleted());
    EXPECT_EQ(order[2], 3);
    const auto complete = pool.snapshot();
    EXPECT_EQ(complete.queued, 0u);
    EXPECT_EQ(complete.submitted, 3u);
    EXPECT_EQ(complete.completed, 3u);
}

TEST(GPUThreadPoolContractTest, ExecutorRejectionMarksFailedWithoutRunningQueuedJob)
{
    GPUThreadPool pool;
    int executions = 0;
    auto handle = pool.enqueue([&] { ++executions; });

    EXPECT_EQ(pool.drain([](const GPUThreadPool::GPUJob&) { return false; }), 1u);
    EXPECT_EQ(executions, 0);
    EXPECT_TRUE(handle.isSubmitted());
    EXPECT_TRUE(handle.isFailed());
    EXPECT_FALSE(handle.isCompleted());
    EXPECT_FALSE(handle.isCancelled());

    const auto stats = pool.snapshot();
    EXPECT_EQ(stats.queued, 0u);
    EXPECT_EQ(stats.submitted, 1u);
    EXPECT_EQ(stats.completed, 0u);
    EXPECT_EQ(stats.failed, 1u);
    EXPECT_EQ(stats.cancelled, 0u);
}

TEST(GPUThreadPoolContractTest, InvalidExecutorLeavesQueueUntouched)
{
    GPUThreadPool pool;
    auto handle = pool.enqueue([] {});

    EXPECT_EQ(pool.drain({}), 0u);
    EXPECT_TRUE(handle.isValid());
    EXPECT_FALSE(handle.isSubmitted());
    EXPECT_EQ(pool.snapshot().queued, 1u);
    EXPECT_EQ(pool.snapshot().submitted, 0u);
}

TEST(GPUThreadPoolContractTest, CancelPendingCancelsOnlyQueuedJobsAndClearResetsCounters)
{
    GPUThreadPool pool(3);
    auto first = pool.enqueue([] {});
    auto second = pool.enqueue([] {});
    auto completed = pool.enqueue([] {});
    ASSERT_EQ(pool.drain([](const GPUThreadPool::GPUJob& job) {
        job();
        return true;
    }, 1), 1u);
    ASSERT_TRUE(completed.isCompleted());

    pool.cancelPending();
    EXPECT_TRUE(first.isCancelled());
    EXPECT_TRUE(second.isCancelled());
    EXPECT_TRUE(completed.isCompleted());
    auto stats = pool.snapshot();
    EXPECT_EQ(stats.queued, 0u);
    EXPECT_EQ(stats.submitted, 1u);
    EXPECT_EQ(stats.completed, 1u);
    EXPECT_EQ(stats.cancelled, 2u);

    pool.clearCompleted();
    stats = pool.snapshot();
    EXPECT_EQ(stats.queued, 0u);
    EXPECT_EQ(stats.submitted, 0u);
    EXPECT_EQ(stats.completed, 0u);
    EXPECT_EQ(stats.failed, 0u);
    EXPECT_EQ(stats.cancelled, 0u);
    EXPECT_TRUE(completed.isCompleted());
}

TEST(GPUThreadPoolContractTest, NormalizesZeroCapacityToOne)
{
    GPUThreadPool pool(0);
    EXPECT_EQ(pool.maxQueuedJobs(), 1u);
    EXPECT_TRUE(pool.enqueue([] {}).isValid());
    EXPECT_FALSE(pool.enqueue([] {}).isValid());
}
