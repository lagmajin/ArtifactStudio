#include <gtest/gtest.h>

#include <atomic>
#include <stdexcept>

import Core.ThreadPool;

using ArtifactCore::ThreadPool;

TEST(ThreadPoolContractTest, EnqueueReturnsFutureWithTaskResult)
{
    ThreadPool pool(2);
    auto result = pool.enqueue([] { return 42; });
    EXPECT_EQ(result.get(), 42);
}

TEST(ThreadPoolContractTest, EnqueueForwardsCallableArguments)
{
    ThreadPool pool(2);
    auto result = pool.enqueue(
        [](int left, int right) { return left * 10 + right; }, 4, 7);
    EXPECT_EQ(result.get(), 47);
}

TEST(ThreadPoolContractTest, WaitAllWaitsForVoidTasks)
{
    ThreadPool pool(2);
    std::atomic<int> completed{0};
    for (int index = 0; index < 16; ++index) {
        pool.enqueueTask([&completed] {
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.waitAll();
    EXPECT_EQ(completed.load(std::memory_order_relaxed), 16);
}

TEST(ThreadPoolContractTest, FutureRethrowsTaskException)
{
    ThreadPool pool(1);
    auto result = pool.enqueue([]() -> int {
        throw std::runtime_error("task failure");
    });

    EXPECT_THROW(result.get(), std::runtime_error);
}

TEST(ThreadPoolContractTest, ConcurrencyIsNeverZero)
{
    ThreadPool zeroRequested(0);
    ThreadPool oneRequested(1);
    EXPECT_EQ(zeroRequested.concurrency(), 1u);
    EXPECT_EQ(oneRequested.concurrency(), 1u);
}
