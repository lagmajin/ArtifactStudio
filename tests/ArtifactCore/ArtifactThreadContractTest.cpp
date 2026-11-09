#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

import Core.ArtifactThread;

using ArtifactCore::Thread;
using ArtifactCore::Lock;
using ArtifactCore::Mutex;
using ArtifactCore::Cond;

static_assert(!std::is_copy_constructible_v<Thread>);
static_assert(!std::is_copy_assignable_v<Thread>);
static_assert(!std::is_move_constructible_v<Thread>);
static_assert(!std::is_copy_constructible_v<Mutex>);
static_assert(!std::is_copy_constructible_v<Lock>);
static_assert(!std::is_move_constructible_v<Lock>);
static_assert(!std::is_copy_constructible_v<Cond>);
static_assert(!std::is_move_constructible_v<Cond>);

TEST(ArtifactThreadContractTest, NewThreadStartsStopped)
{
    const Thread thread;
    EXPECT_FALSE(thread.isRunning());
}

TEST(ArtifactThreadContractTest, StartRunsCallbackAndJoinWaitsForCompletion)
{
    Thread thread;
    std::promise<void> completed;
    auto completion = completed.get_future();
    thread.start([&completed] { completed.set_value(); });
    thread.join();
    EXPECT_EQ(completion.wait_for(std::chrono::seconds(0)), std::future_status::ready);
    EXPECT_FALSE(thread.isRunning());
}

TEST(ArtifactThreadContractTest, JoinWithoutWorkerIsSafeAndIdempotent)
{
    Thread thread;
    thread.join();
    thread.join();
    EXPECT_FALSE(thread.isRunning());
}

TEST(ArtifactThreadContractTest, JoinCanBeFollowedByAnotherStart)
{
    Thread thread;
    std::atomic<int> calls{0};
    thread.start([&calls] { calls.fetch_add(1, std::memory_order_relaxed); });
    thread.join();
    thread.start([&calls] { calls.fetch_add(10, std::memory_order_relaxed); });
    thread.join();
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 11);
}

TEST(ArtifactThreadContractTest, StartAfterWorkerFinishesReapsPreviousThread)
{
    Thread thread;
    std::promise<void> completed;
    auto completion = completed.get_future();
    thread.start([&completed] { completed.set_value(); });
    if (completion.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        thread.join();
        FAIL() << "worker did not complete";
        return;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (thread.isRunning() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    EXPECT_FALSE(thread.isRunning());

    std::atomic<int> secondCalls{0};
    thread.start([&secondCalls] { secondCalls.fetch_add(1, std::memory_order_relaxed); });
    thread.join();

    EXPECT_FALSE(thread.isRunning());
    EXPECT_EQ(secondCalls.load(std::memory_order_relaxed), 1);
}

TEST(ArtifactThreadContractTest, DetachReturnsWithoutCancelingWorker)
{
    auto thread = std::make_shared<Thread>();
    std::promise<void> started;
    std::promise<void> release;
    auto releaseSignal = release.get_future().share();
    std::promise<void> completed;
    auto completion = completed.get_future();

    thread->start([&, keepAlive = thread] {
        (void)keepAlive;
        started.set_value();
        releaseSignal.wait();
        completed.set_value();
    });
    started.get_future().wait();

    thread->detach();
    EXPECT_FALSE(thread->isRunning());
    release.set_value();
    EXPECT_EQ(completion.wait_for(std::chrono::seconds(5)), std::future_status::ready);
}

TEST(ArtifactThreadContractTest, StartWhileWorkerIsBlockedDoesNotReplaceCallback)
{
    Thread thread;
    std::promise<void> started;
    std::promise<void> release;
    auto releaseSignal = release.get_future().share();
    std::atomic<int> calls{0};
    thread.start([&] {
        started.set_value();
        releaseSignal.wait();
        calls.fetch_add(1, std::memory_order_relaxed);
    });
    started.get_future().wait();
    ASSERT_TRUE(thread.isRunning());
    thread.start([&calls] { calls.fetch_add(100, std::memory_order_relaxed); });
    release.set_value();
    thread.join();
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 1);
}

TEST(ArtifactThreadContractTest, StartAcceptsMoveOnlyCallableCapture)
{
    Thread thread;
    auto value = std::make_unique<int>(37);
    std::promise<int> result;
    auto resultFuture = result.get_future();
    thread.start([owned = std::move(value), result = std::move(result)]() mutable {
        result.set_value(*owned);
    });
    thread.join();
    EXPECT_EQ(resultFuture.get(), 37);
}

TEST(ArtifactThreadContractTest, MutexTryLockAcquiresUnlockedMutexAndReleases)
{
    Mutex mutex;
    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
}

TEST(ArtifactThreadContractTest, TryLockFailsWhileOwnedAndSucceedsAfterUnlock)
{
    Mutex mutex;
    mutex.lock();
    EXPECT_FALSE(mutex.tryLock());
    mutex.unlock();
    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
}

TEST(ArtifactThreadContractTest, LockGuardReleasesMutexAtScopeExit)
{
    Mutex mutex;
    {
        Lock lock(mutex);
        EXPECT_FALSE(mutex.tryLock());
    }
    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
}

TEST(ArtifactThreadContractTest, LockGuardReleasesMutexDuringExceptionUnwinding)
{
    Mutex mutex;

    EXPECT_THROW(
        {
            Lock lock(mutex);
            throw std::runtime_error("scope exit");
        },
        std::runtime_error);

    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
}

TEST(ArtifactThreadContractTest, MutexSupportsRecursiveLockAndBalancesUnlocks)
{
    Mutex mutex;
    {
        Lock outer(mutex);
        {
            Lock inner(mutex);
            EXPECT_FALSE(mutex.tryLock());
        }
        EXPECT_FALSE(mutex.tryLock());
    }
    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
}

TEST(ArtifactThreadContractTest, TryLockRejectsOwnershipByAnotherThread)
{
    Mutex mutex;
    std::promise<void> firstOwnsMutex;
    std::promise<void> releaseFirst;
    const auto releaseSignal = releaseFirst.get_future().share();
    std::promise<bool> secondTryResult;
    Thread first;
    Thread second;

    first.start([&] {
        Lock lock(mutex);
        firstOwnsMutex.set_value();
        releaseSignal.wait();
    });
    firstOwnsMutex.get_future().wait();

    second.start([&] {
        const bool acquired = mutex.tryLock();
        if (acquired) mutex.unlock();
        secondTryResult.set_value(acquired);
    });

    EXPECT_FALSE(secondTryResult.get_future().get());
    releaseFirst.set_value();
    first.join();
    second.join();
    EXPECT_TRUE(mutex.tryLock());
    mutex.unlock();
}

TEST(ArtifactThreadContractTest, ConditionNotificationsRemainAvailableUntilConsumed)
{
    Mutex mutex;
    Lock lock(mutex);
    Cond one;
    Cond all;

    one.wakeOne();
    EXPECT_NO_THROW(one.wait(lock));
    all.wakeAll();
    EXPECT_NO_THROW(all.wait(lock));
}

TEST(ArtifactThreadContractTest, ConditionPredicateWaitReturnsWhenAlreadySatisfied)
{
    Mutex mutex;
    Lock lock(mutex);
    Cond condition;
    int predicateCalls = 0;

    condition.wait(lock, [&predicateCalls] {
        ++predicateCalls;
        return true;
    });

    EXPECT_EQ(predicateCalls, 1);
}

TEST(ArtifactThreadContractTest, ConditionPredicateWaitRechecksAfterNotification)
{
    Mutex mutex;
    Lock lock(mutex);
    Cond condition;
    std::atomic<bool> ready{false};
    std::promise<void> predicateChecked;
    auto checked = predicateChecked.get_future();
    std::atomic<bool> firstCheck{true};
    Thread notifier;

    notifier.start([&] {
        checked.wait();
        ready.store(true, std::memory_order_release);
        condition.wakeOne();
    });

    condition.wait(lock, [&] {
        if (firstCheck.exchange(false, std::memory_order_acq_rel)) {
            predicateChecked.set_value();
        }
        return ready.load(std::memory_order_acquire);
    });

    notifier.join();
    EXPECT_TRUE(ready.load(std::memory_order_acquire));
}

TEST(ArtifactThreadContractTest, WakeAllReleasesEveryAlreadyWaitingThread)
{
    Cond condition;
    std::atomic<int> waiting{0};
    std::atomic<int> released{0};
    std::atomic<bool> releasePredicate{false};
    Thread first;
    Thread second;
    Thread third;

    const auto worker = [&] {
        Mutex mutex;
        Lock lock(mutex);
        waiting.fetch_add(1, std::memory_order_release);
        condition.wait(lock, [&] {
            return releasePredicate.load(std::memory_order_acquire);
        });
        released.fetch_add(1, std::memory_order_release);
    };
    first.start(worker);
    second.start(worker);
    third.start(worker);

    const auto setupDeadline = std::chrono::steady_clock::now() +
                               std::chrono::seconds(2);
    while (waiting.load(std::memory_order_acquire) < 3 &&
           std::chrono::steady_clock::now() < setupDeadline) {
        std::this_thread::yield();
    }
    const bool allWorkersStarted = waiting.load(std::memory_order_acquire) == 3;

    releasePredicate.store(true, std::memory_order_release);
    condition.wakeAll();

    const auto releaseDeadline = std::chrono::steady_clock::now() +
                                 std::chrono::seconds(2);
    while (released.load(std::memory_order_acquire) < 3 &&
           std::chrono::steady_clock::now() < releaseDeadline) {
        std::this_thread::yield();
    }
    const int releasedByFirstBroadcast = released.load(std::memory_order_acquire);

    // Make cleanup bounded even if a regression leaves a waiter asleep.
    for (int attempt = 0; attempt < 3 &&
         released.load(std::memory_order_acquire) < 3; ++attempt) {
        condition.wakeAll();
    }
    first.join();
    second.join();
    third.join();

    EXPECT_TRUE(allWorkersStarted);
    EXPECT_EQ(releasedByFirstBroadcast, 3);
    EXPECT_EQ(released.load(std::memory_order_acquire), 3);
}

TEST(ArtifactThreadContractTest, WakeOneReleasesOnlyOneWaiterBeforeWakeAll)
{
    constexpr int waiterCount = 3;
    Mutex mutex;
    Cond condition;
    std::atomic<int> waiting{0};
    std::atomic<int> released{0};
    int permits = 0;
    bool shutdown = false;
    std::array<std::thread, waiterCount> waiters;

    for (auto& waiter : waiters) {
        waiter = std::thread([&] {
            Lock lock(mutex);
            waiting.fetch_add(1, std::memory_order_release);
            condition.wait(lock, [&] { return shutdown || permits > 0; });
            if (permits > 0) --permits;
            released.fetch_add(1, std::memory_order_release);
        });
    }

    const auto setupDeadline = std::chrono::steady_clock::now() +
                               std::chrono::seconds(2);
    while (waiting.load(std::memory_order_acquire) < waiterCount &&
           std::chrono::steady_clock::now() < setupDeadline) {
        std::this_thread::yield();
    }
    const bool allWaitersStarted =
        waiting.load(std::memory_order_acquire) == waiterCount;

    {
        Lock lock(mutex);
        ++permits;
        condition.wakeOne();
    }

    const auto oneReleaseDeadline = std::chrono::steady_clock::now() +
                                    std::chrono::seconds(2);
    while (released.load(std::memory_order_acquire) < 1 &&
           std::chrono::steady_clock::now() < oneReleaseDeadline) {
        std::this_thread::yield();
    }
    const int releasedAfterWakeOne = released.load(std::memory_order_acquire);

    {
        Lock lock(mutex);
        shutdown = true;
        condition.wakeAll();
    }
    for (auto& waiter : waiters) waiter.join();

    EXPECT_TRUE(allWaitersStarted);
    EXPECT_EQ(releasedAfterWakeOne, 1);
    EXPECT_EQ(released.load(std::memory_order_acquire), waiterCount);
}
