#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

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

TEST(ThreadPoolContractTest,
     ConcurrentProducersEnqueueEveryTaskExactlyOnceBeforeWaitAllReturns)
{
    constexpr std::size_t producerCount = 4;
    constexpr std::size_t tasksPerProducer = 128;
    constexpr std::size_t taskCount = producerCount * tasksPerProducer;

    ThreadPool pool(producerCount);
    std::array<std::atomic<int>, taskCount> executionCounts{};
    std::array<std::future<std::size_t>, taskCount> results{};
    for (auto& count : executionCounts) count.store(0, std::memory_order_relaxed);
    std::atomic<std::size_t> readyProducers{0};
    std::atomic<bool> beginSubmissions{false};
    std::thread producers[producerCount];

    for (std::size_t producer = 0; producer < producerCount; ++producer) {
        producers[producer] = std::thread([&, producer] {
            readyProducers.fetch_add(1, std::memory_order_release);
            while (!beginSubmissions.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (std::size_t offset = 0; offset < tasksPerProducer; ++offset) {
                const std::size_t taskId = producer * tasksPerProducer + offset;
                results[taskId] = pool.enqueue([&executionCounts, taskId] {
                    executionCounts[taskId].fetch_add(1, std::memory_order_relaxed);
                    return taskId;
                });
            }
        });
    }

    while (readyProducers.load(std::memory_order_acquire) != producerCount)
        std::this_thread::yield();
    beginSubmissions.store(true, std::memory_order_release);
    for (auto& producer : producers) producer.join();
    pool.waitAll();

    for (std::size_t taskId = 0; taskId < taskCount; ++taskId) {
        EXPECT_EQ(results[taskId].get(), taskId) << "taskId=" << taskId;
        EXPECT_EQ(executionCounts[taskId].load(std::memory_order_relaxed), 1)
            << "taskId=" << taskId;
    }
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

TEST(ThreadPoolContractTest, DestructorWaitsForAlreadyRunningTasksToFinish)
{
    auto pool = std::make_unique<ThreadPool>(1);
    std::promise<void> taskStartedPromise;
    auto taskStarted = taskStartedPromise.get_future();
    std::promise<void> releaseTaskPromise;
    const auto releaseTask = releaseTaskPromise.get_future().share();
    auto taskResult = pool->enqueue([&] {
        taskStartedPromise.set_value();
        releaseTask.wait();
        return 73;
    });

    if (taskStarted.wait_for(std::chrono::seconds(2)) !=
        std::future_status::ready) {
        releaseTaskPromise.set_value();
        EXPECT_TRUE(taskStarted.wait_for(std::chrono::seconds(2)) ==
                    std::future_status::ready);
        return;
    }

    std::promise<void> destructionStartedPromise;
    auto destructionStarted = destructionStartedPromise.get_future();
    auto destruction = std::async(std::launch::async, [&] {
        destructionStartedPromise.set_value();
        pool.reset();
    });
    if (destructionStarted.wait_for(std::chrono::seconds(2)) !=
        std::future_status::ready) {
        releaseTaskPromise.set_value();
        destruction.wait();
        FAIL() << "destruction worker did not start";
        return;
    }

    EXPECT_EQ(destruction.wait_for(std::chrono::milliseconds(20)),
              std::future_status::timeout);
    releaseTaskPromise.set_value();
    EXPECT_EQ(taskResult.get(), 73);
    EXPECT_EQ(destruction.wait_for(std::chrono::seconds(2)),
              std::future_status::ready);
}
