#include <gtest/gtest.h>

#include <QString>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

import Core.Thread.BackgroundTaskRuntime;
import Core.Thread.BackgroundTaskWorkerPool;
import Memory.SharedPtr;

namespace {

using namespace std::chrono_literals;
using ArtifactCore::BackgroundTaskWorkerPool;
using ArtifactCore::CancelToken;
using ArtifactCore::IBackgroundTask;
using ArtifactCore::SharedPtr;
using ArtifactCore::TaskCategory;
using ArtifactCore::TaskId;
using ArtifactCore::TaskOptions;
using ArtifactCore::TaskPriority;
using ArtifactCore::TaskProgress;
using ArtifactCore::TaskSnapshot;
using ArtifactCore::TaskState;

class FunctionTask final : public IBackgroundTask {
public:
    using Body = std::function<void(CancelToken&, std::function<void(TaskProgress)>)>;

    FunctionTask(TaskOptions options, Body body)
        : options_(std::move(options)), body_(std::move(body))
    {
    }

    auto GetOptions() const -> TaskOptions override { return options_; }

    auto Execute(CancelToken& token,
                 std::function<void(TaskProgress)> reportProgress) -> void override
    {
        body_(token, std::move(reportProgress));
    }

private:
    TaskOptions options_;
    Body body_;
};

auto makeTask(TaskOptions options, FunctionTask::Body body)
    -> SharedPtr<IBackgroundTask>
{
    return SharedPtr<IBackgroundTask>{
        std::make_shared<FunctionTask>(std::move(options), std::move(body))};
}

class SnapshotRecorder {
public:
    void attach(BackgroundTaskWorkerPool& pool)
    {
        pool.SetSnapshotSink([this, &pool](const TaskSnapshot& snapshot) {
            // Re-enter the pool from the sink to ensure callbacks are made
            // without holding the snapshot or queue mutex.
            const auto current = pool.GetTaskSnapshot(snapshot.id);
            {
                std::lock_guard lock(mutex_);
                snapshots_[snapshot.id.value] = current;
            }
            changed_.notify_all();
        });
    }

    template <typename Predicate>
    auto waitFor(TaskId id, Predicate predicate) -> bool
    {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, 5s, [&] {
            const auto it = snapshots_.find(id.value);
            return it != snapshots_.end() && predicate(it->second);
        });
    }

    auto snapshot(TaskId id) -> TaskSnapshot
    {
        std::lock_guard lock(mutex_);
        return snapshots_.at(id.value);
    }

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::unordered_map<std::uint64_t, TaskSnapshot> snapshots_;
};

auto singleWorkerConfig(int maxPending = 16) -> BackgroundTaskWorkerPool::Config
{
    return BackgroundTaskWorkerPool::Config{
        .maxWorkers = 1,
        .maxPendingTasks = maxPending,
        .enableDependencyResolution = false};
}

} // namespace

TEST(BackgroundTaskWorkerPoolContractTest, EnforcesPendingCapacityAndKeepsAcceptedTask)
{
    BackgroundTaskWorkerPool pool{singleWorkerConfig(1)};
    const auto accepted = pool.SubmitTask(makeTask(TaskOptions{},
        [](CancelToken&, std::function<void(TaskProgress)>) {}));

    EXPECT_EQ(pool.GetPendingCount(), 1);
    EXPECT_EQ(pool.GetTaskSnapshot(accepted).state, TaskState::Pending);
    EXPECT_THROW(pool.SubmitTask(makeTask(TaskOptions{},
        [](CancelToken&, std::function<void(TaskProgress)>) {})), std::runtime_error);
    EXPECT_EQ(pool.GetPendingCount(), 1);
    EXPECT_EQ(pool.GetAllSnapshots().size(), 1u);
}

TEST(BackgroundTaskWorkerPoolContractTest,
     ConcurrentSubmissionsNeverExceedPendingCapacity)
{
    constexpr int submitterCount = 12;
    constexpr int pendingCapacity = 5;
    BackgroundTaskWorkerPool pool{singleWorkerConfig(pendingCapacity)};
    std::atomic<int> readySubmitters{0};
    std::atomic<bool> startSubmissions{false};
    std::atomic<int> accepted{0};
    std::atomic<int> rejected{0};
    std::atomic<int> unexpectedErrors{0};
    std::vector<std::thread> submitters;
    submitters.reserve(submitterCount);

    for (int i = 0; i < submitterCount; ++i) {
        submitters.emplace_back([&] {
            readySubmitters.fetch_add(1, std::memory_order_release);
            while (!startSubmissions.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            try {
                pool.SubmitTask(makeTask(TaskOptions{},
                    [](CancelToken&, std::function<void(TaskProgress)>) {}));
                accepted.fetch_add(1, std::memory_order_relaxed);
            } catch (const std::runtime_error&) {
                rejected.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                unexpectedErrors.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    while (readySubmitters.load(std::memory_order_acquire) != submitterCount) {
        std::this_thread::yield();
    }
    startSubmissions.store(true, std::memory_order_release);
    for (auto& submitter : submitters) {
        submitter.join();
    }

    EXPECT_EQ(unexpectedErrors.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(accepted.load(std::memory_order_relaxed), pendingCapacity);
    EXPECT_EQ(rejected.load(std::memory_order_relaxed),
              submitterCount - pendingCapacity);
    EXPECT_EQ(pool.GetPendingCount(), pendingCapacity);
    const auto snapshots = pool.GetAllSnapshots();
    ASSERT_EQ(snapshots.size(), static_cast<std::size_t>(pendingCapacity));
    for (const auto& snapshot : snapshots) {
        EXPECT_EQ(snapshot.state, TaskState::Pending);
    }
}

TEST(BackgroundTaskWorkerPoolContractTest, PublishesProgressAndCompletedSnapshot)
{
    BackgroundTaskWorkerPool pool{singleWorkerConfig()};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    pool.Start();

    const auto id = pool.SubmitTask(makeTask(TaskOptions{.name = QStringLiteral("progress")},
        [](CancelToken&, std::function<void(TaskProgress)> report) {
            report(TaskProgress{.completed = 3.0, .total = 4.0,
                                .message = QStringLiteral("three of four")});
        }));

    ASSERT_TRUE(recorder.waitFor(id, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    const auto snapshot = recorder.snapshot(id);
    EXPECT_EQ(snapshot.name, QStringLiteral("progress"));
    EXPECT_DOUBLE_EQ(snapshot.progress.completed, 3.0);
    EXPECT_DOUBLE_EQ(snapshot.progress.GetPercentage(), 75.0);
    EXPECT_EQ(snapshot.progress.message, QStringLiteral("three of four"));
    EXPECT_NE(snapshot.startTime, std::chrono::steady_clock::time_point{});
    EXPECT_GE(snapshot.endTime, snapshot.startTime);
    EXPECT_EQ(pool.GetRunningCount(), 0);
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest, ConvertsTaskExceptionsToFailedSnapshots)
{
    BackgroundTaskWorkerPool pool{singleWorkerConfig()};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    pool.Start();

    const auto id = pool.SubmitTask(makeTask(TaskOptions{.name = QStringLiteral("failure")},
        [](CancelToken&, std::function<void(TaskProgress)>) {
            throw std::runtime_error("contract failure");
        }));

    ASSERT_TRUE(recorder.waitFor(id, [](const auto& snapshot) {
        return snapshot.state == TaskState::Failed;
    }));
    const auto snapshot = recorder.snapshot(id);
    EXPECT_EQ(snapshot.error.code, -1);
    EXPECT_EQ(snapshot.error.message, QStringLiteral("contract failure"));
    EXPECT_EQ(snapshot.error.category, QStringLiteral("runtime"));
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest, CancelTaskReachesTheExecutingTaskToken)
{
    BackgroundTaskWorkerPool pool{singleWorkerConfig()};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::promise<void> started;
    auto startedFuture = started.get_future();
    pool.Start();

    const auto id = pool.SubmitTask(makeTask(TaskOptions{.taskCategory = TaskCategory::Render},
        [&started](CancelToken& token, std::function<void(TaskProgress)>) {
            started.set_value();
            while (!token.IsCancelled()) {
                std::this_thread::sleep_for(1ms);
            }
            token.ThrowIfCancelled();
        }));

    ASSERT_EQ(startedFuture.wait_for(5s), std::future_status::ready);
    pool.CancelTask(id);
    ASSERT_TRUE(recorder.waitFor(id, [](const auto& snapshot) {
        return snapshot.state == TaskState::Cancelled;
    }));
    EXPECT_EQ(recorder.snapshot(id).state, TaskState::Cancelled);
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest,
     CancelTaskRacingWithCompletionLeavesOneConsistentTerminalSnapshot)
{
    constexpr int iterationCount = 32;
    BackgroundTaskWorkerPool pool{singleWorkerConfig()};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    pool.Start();

    for (int iteration = 0; iteration < iterationCount; ++iteration) {
        std::promise<void> started;
        std::promise<void> releaseTask;
        auto releaseFuture = releaseTask.get_future().share();
        std::atomic<bool> cancellationObserved{false};
        const auto id = pool.SubmitTask(makeTask(TaskOptions{},
            [&started, releaseFuture, &cancellationObserved]
            (CancelToken& token, std::function<void(TaskProgress)>) {
                started.set_value();
                releaseFuture.wait();
                const bool wasCancelled = token.IsCancelled();
                cancellationObserved.store(wasCancelled, std::memory_order_release);
                if (wasCancelled) token.ThrowIfCancelled();
            }));

        if (started.get_future().wait_for(5s) != std::future_status::ready) {
            releaseTask.set_value();
            pool.Shutdown();
            FAIL() << "task " << iteration << " did not start";
            return;
        }

        std::atomic<bool> raceStarted{false};
        std::thread canceller([&] {
            while (!raceStarted.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            pool.CancelTask(id);
        });
        raceStarted.store(true, std::memory_order_release);
        releaseTask.set_value();
        canceller.join();

        ASSERT_TRUE(recorder.waitFor(id, [](const auto& snapshot) {
            return snapshot.state == TaskState::Completed ||
                   snapshot.state == TaskState::Cancelled;
        })) << "task " << iteration;
        const auto snapshot = pool.GetTaskSnapshot(id);
        EXPECT_NE(snapshot.startTime, std::chrono::steady_clock::time_point{});
        EXPECT_GE(snapshot.endTime, snapshot.startTime);
        EXPECT_EQ(snapshot.state == TaskState::Cancelled,
                  cancellationObserved.load(std::memory_order_acquire));

        // A late cancel must be harmless after ExecuteTask removes its token.
        pool.CancelTask(id);
        EXPECT_EQ(pool.GetTaskSnapshot(id).state, snapshot.state);
    }

    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest, CancelTaskRemovesQueuedTaskBeforeExecution)
{
    BackgroundTaskWorkerPool pool{singleWorkerConfig()};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::promise<void> firstStarted;
    std::promise<void> releaseFirst;
    auto releaseFuture = releaseFirst.get_future().share();
    std::atomic<bool> queuedBodyRan{false};
    pool.Start();

    pool.SubmitTask(makeTask(TaskOptions{},
        [&firstStarted, releaseFuture](CancelToken&, std::function<void(TaskProgress)>) {
            firstStarted.set_value();
            releaseFuture.wait();
        }));

    if (firstStarted.get_future().wait_for(5s) != std::future_status::ready) {
        releaseFirst.set_value();
        pool.Shutdown();
        FAIL() << "the task occupying the only worker did not start";
        return;
    }

    const auto queuedId = pool.SubmitTask(makeTask(TaskOptions{},
        [&queuedBodyRan](CancelToken&, std::function<void(TaskProgress)>) {
            queuedBodyRan.store(true, std::memory_order_release);
        }));
    ASSERT_EQ(pool.GetTaskSnapshot(queuedId).state, TaskState::Pending);

    pool.CancelTask(queuedId);
    releaseFirst.set_value();

    ASSERT_TRUE(recorder.waitFor(queuedId, [](const auto& snapshot) {
        return snapshot.state == TaskState::Cancelled ||
               snapshot.state == TaskState::Completed ||
               snapshot.state == TaskState::Failed;
    }));
    EXPECT_EQ(recorder.snapshot(queuedId).state, TaskState::Cancelled);
    EXPECT_FALSE(queuedBodyRan.load(std::memory_order_acquire));
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest,
     ClearQueueCancelsRunningAndPendingTasksWithoutExecutingQueuedWork)
{
    BackgroundTaskWorkerPool pool{singleWorkerConfig()};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::promise<void> runningStarted;
    std::promise<void> releaseRunning;
    auto releaseFuture = releaseRunning.get_future().share();
    std::atomic<bool> queuedBodyRan{false};
    pool.Start();

    const auto running = pool.SubmitTask(makeTask(TaskOptions{},
        [&runningStarted, releaseFuture](CancelToken& token,
                                         std::function<void(TaskProgress)>) {
            runningStarted.set_value();
            releaseFuture.wait();
            token.ThrowIfCancelled();
        }));

    if (runningStarted.get_future().wait_for(5s) != std::future_status::ready) {
        releaseRunning.set_value();
        pool.Shutdown();
        FAIL() << "the running task did not start";
        return;
    }

    const auto queued = pool.SubmitTask(makeTask(TaskOptions{},
        [&queuedBodyRan](CancelToken&, std::function<void(TaskProgress)>) {
            queuedBodyRan.store(true, std::memory_order_release);
        }));
    EXPECT_EQ(pool.GetTaskSnapshot(queued).state, TaskState::Pending);

    pool.ClearQueue();
    releaseRunning.set_value();
    const bool runningFinished = recorder.waitFor(running, [](const auto& snapshot) {
        return snapshot.state == TaskState::Cancelled ||
               snapshot.state == TaskState::Completed ||
               snapshot.state == TaskState::Failed;
    });

    EXPECT_TRUE(runningFinished);
    if (runningFinished) {
        EXPECT_EQ(recorder.snapshot(running).state, TaskState::Cancelled);
    }
    EXPECT_EQ(pool.GetTaskSnapshot(queued).state, TaskState::Cancelled);
    EXPECT_FALSE(queuedBodyRan.load(std::memory_order_acquire));
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest, CategoryCancellationReachesMatchingTasksOnly)
{
    BackgroundTaskWorkerPool pool{BackgroundTaskWorkerPool::Config{
        .maxWorkers = 2,
        .maxPendingTasks = 16,
        .enableDependencyResolution = false}};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::promise<void> renderStarted;
    std::promise<void> analysisStarted;
    auto renderStartedFuture = renderStarted.get_future();
    auto analysisStartedFuture = analysisStarted.get_future();
    pool.Start();

    const auto renderId = pool.SubmitTask(makeTask(
        TaskOptions{.taskCategory = TaskCategory::Render},
        [&renderStarted](CancelToken& token, std::function<void(TaskProgress)>) {
            renderStarted.set_value();
            while (!token.IsCancelled()) {
                std::this_thread::sleep_for(1ms);
            }
            token.ThrowIfCancelled();
        }));
    const auto analysisId = pool.SubmitTask(makeTask(
        TaskOptions{.taskCategory = TaskCategory::Analysis},
        [&analysisStarted](CancelToken& token, std::function<void(TaskProgress)>) {
            analysisStarted.set_value();
            while (!token.IsCancelled()) {
                std::this_thread::sleep_for(1ms);
            }
            token.ThrowIfCancelled();
        }));

    ASSERT_EQ(renderStartedFuture.wait_for(5s), std::future_status::ready);
    ASSERT_EQ(analysisStartedFuture.wait_for(5s), std::future_status::ready);
    pool.CancelTasksByCategory(TaskCategory::Render);
    ASSERT_TRUE(recorder.waitFor(renderId, [](const auto& snapshot) {
        return snapshot.state == TaskState::Cancelled;
    }));
    EXPECT_EQ(recorder.snapshot(analysisId).state, TaskState::Running);
    pool.CancelTask(analysisId);
    ASSERT_TRUE(recorder.waitFor(analysisId, [](const auto& snapshot) {
        return snapshot.state == TaskState::Cancelled;
    }));
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest, SchedulesQueuedTasksByPriority)
{
    BackgroundTaskWorkerPool pool{singleWorkerConfig()};
    std::promise<void> firstStarted;
    std::promise<void> releaseFirst;
    auto releaseFuture = releaseFirst.get_future().share();
    std::mutex orderMutex;
    std::vector<int> order;
    SnapshotRecorder recorder;
    recorder.attach(pool);
    pool.Start();

    const auto first = pool.SubmitTask(makeTask(TaskOptions{},
        [&firstStarted, releaseFuture](CancelToken&, std::function<void(TaskProgress)>) {
            firstStarted.set_value();
            releaseFuture.wait();
        }));
    ASSERT_EQ(firstStarted.get_future().wait_for(5s), std::future_status::ready);

    const auto low = pool.SubmitTask(makeTask(TaskOptions{.priority = TaskPriority::Low},
        [&orderMutex, &order](CancelToken&, std::function<void(TaskProgress)>) {
            std::lock_guard lock(orderMutex);
            order.push_back(1);
        }));
    const auto high = pool.SubmitTask(makeTask(TaskOptions{.priority = TaskPriority::Critical},
        [&orderMutex, &order](CancelToken&, std::function<void(TaskProgress)>) {
            std::lock_guard lock(orderMutex);
            order.push_back(2);
        }));

    releaseFirst.set_value();
    ASSERT_TRUE(recorder.waitFor(low, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    ASSERT_TRUE(recorder.waitFor(high, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    {
        std::lock_guard lock(orderMutex);
        ASSERT_EQ(order.size(), 2u);
        EXPECT_EQ(order[0], 2);
        EXPECT_EQ(order[1], 1);
    }
    EXPECT_EQ(recorder.snapshot(first).state, TaskState::Completed);
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest, DependencyWaitDoesNotBlockIndependentWork)
{
    BackgroundTaskWorkerPool pool{BackgroundTaskWorkerPool::Config{
        .maxWorkers = 2,
        .maxPendingTasks = 16,
        .enableDependencyResolution = true}};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::promise<void> prerequisiteStarted;
    std::promise<void> releasePrerequisite;
    auto releaseFuture = releasePrerequisite.get_future().share();
    std::atomic<bool> prerequisiteCompleted{false};
    std::atomic<bool> dependencyStartedTooEarly{false};
    std::mutex orderMutex;
    std::vector<int> order;
    pool.Start();

    const auto prerequisite = pool.SubmitTask(makeTask(TaskOptions{},
        [&prerequisiteStarted, &prerequisiteCompleted, &orderMutex, &order,
         releaseFuture](CancelToken&, std::function<void(TaskProgress)>) {
            prerequisiteStarted.set_value();
            releaseFuture.wait();
            {
                std::lock_guard lock(orderMutex);
                order.push_back(0);
            }
            prerequisiteCompleted.store(true, std::memory_order_release);
        }));

    if (prerequisiteStarted.get_future().wait_for(5s) != std::future_status::ready) {
        releasePrerequisite.set_value();
        pool.Shutdown();
        FAIL() << "prerequisite task did not start";
        return;
    }

    TaskOptions dependentOptions;
    dependentOptions.priority = TaskPriority::Critical;
    dependentOptions.dependencies.push_back(prerequisite);
    const auto dependent = pool.SubmitTask(makeTask(std::move(dependentOptions),
        [&prerequisiteCompleted, &dependencyStartedTooEarly, &orderMutex, &order]
        (CancelToken&, std::function<void(TaskProgress)>) {
            if (!prerequisiteCompleted.load(std::memory_order_acquire)) {
                dependencyStartedTooEarly.store(true, std::memory_order_release);
            }
            std::lock_guard lock(orderMutex);
            order.push_back(1);
        }));
    const auto independent = pool.SubmitTask(makeTask(
        TaskOptions{.priority = TaskPriority::Low},
        [&orderMutex, &order](CancelToken&, std::function<void(TaskProgress)>) {
            std::lock_guard lock(orderMutex);
            order.push_back(2);
        }));

    if (!recorder.waitFor(independent, [](const auto& snapshot) {
            return snapshot.state == TaskState::Completed;
        })) {
        releasePrerequisite.set_value();
        pool.Shutdown();
        FAIL() << "independent work did not complete while prerequisite was blocked";
        return;
    }

    EXPECT_EQ(recorder.snapshot(dependent).state, TaskState::Pending);
    EXPECT_FALSE(dependencyStartedTooEarly.load(std::memory_order_acquire));
    releasePrerequisite.set_value();

    EXPECT_TRUE(recorder.waitFor(prerequisite, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    EXPECT_TRUE(recorder.waitFor(dependent, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    EXPECT_FALSE(dependencyStartedTooEarly.load(std::memory_order_acquire));
    {
        std::lock_guard lock(orderMutex);
        ASSERT_EQ(order.size(), 3u);
        EXPECT_EQ(order[0], 2);
        EXPECT_EQ(order[1], 0);
        EXPECT_EQ(order[2], 1);
    }
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest,
     FanInDependencyWaitsUntilEveryPrerequisiteCompletes)
{
    BackgroundTaskWorkerPool pool{BackgroundTaskWorkerPool::Config{
        .maxWorkers = 2,
        .maxPendingTasks = 16,
        .enableDependencyResolution = true}};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::promise<void> firstStarted;
    std::promise<void> secondStarted;
    std::promise<void> releaseFirst;
    std::promise<void> releaseSecond;
    auto releaseFirstFuture = releaseFirst.get_future().share();
    auto releaseSecondFuture = releaseSecond.get_future().share();
    auto firstStartedFuture = firstStarted.get_future();
    auto secondStartedFuture = secondStarted.get_future();
    std::atomic<bool> dependentRan{false};
    pool.Start();

    const auto first = pool.SubmitTask(makeTask(TaskOptions{},
        [&firstStarted, releaseFirstFuture]
        (CancelToken&, std::function<void(TaskProgress)>) {
            firstStarted.set_value();
            releaseFirstFuture.wait();
        }));
    const auto second = pool.SubmitTask(makeTask(TaskOptions{},
        [&secondStarted, releaseSecondFuture]
        (CancelToken&, std::function<void(TaskProgress)>) {
            secondStarted.set_value();
            releaseSecondFuture.wait();
        }));

    const bool bothStarted =
        firstStartedFuture.wait_for(5s) == std::future_status::ready &&
        secondStartedFuture.wait_for(5s) == std::future_status::ready;
    if (!bothStarted) {
        releaseFirst.set_value();
        releaseSecond.set_value();
        pool.Shutdown();
        FAIL() << "both prerequisite tasks did not start";
        return;
    }

    TaskOptions dependentOptions;
    dependentOptions.priority = TaskPriority::Critical;
    dependentOptions.dependencies.push_back(first);
    dependentOptions.dependencies.push_back(second);
    const auto dependent = pool.SubmitTask(makeTask(std::move(dependentOptions),
        [&dependentRan](CancelToken&, std::function<void(TaskProgress)>) {
            dependentRan.store(true, std::memory_order_release);
        }));

    releaseFirst.set_value();
    const bool firstCompleted = recorder.waitFor(first, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    });
    if (!firstCompleted) {
        releaseSecond.set_value();
        pool.Shutdown();
        FAIL() << "first prerequisite did not complete";
        return;
    }
    EXPECT_EQ(pool.GetTaskSnapshot(second).state, TaskState::Running);
    EXPECT_EQ(pool.GetTaskSnapshot(dependent).state, TaskState::Pending);
    EXPECT_FALSE(dependentRan.load(std::memory_order_acquire));

    releaseSecond.set_value();
    ASSERT_TRUE(recorder.waitFor(dependent, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    EXPECT_TRUE(dependentRan.load(std::memory_order_acquire));
    EXPECT_EQ(pool.GetTaskSnapshot(first).state, TaskState::Completed);
    EXPECT_EQ(pool.GetTaskSnapshot(second).state, TaskState::Completed);
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest,
     FanOutDependentsAllRunAfterTheirSharedPrerequisite)
{
    BackgroundTaskWorkerPool pool{BackgroundTaskWorkerPool::Config{
        .maxWorkers = 3,
        .maxPendingTasks = 16,
        .enableDependencyResolution = true}};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::promise<void> prerequisiteStarted;
    std::promise<void> releasePrerequisite;
    auto releaseFuture = releasePrerequisite.get_future().share();
    auto prerequisiteStartedFuture = prerequisiteStarted.get_future();
    std::atomic<int> earlyRuns{0};
    std::atomic<int> firstRuns{0};
    std::atomic<int> secondRuns{0};
    pool.Start();

    const auto prerequisite = pool.SubmitTask(makeTask(TaskOptions{},
        [&prerequisiteStarted, releaseFuture]
        (CancelToken&, std::function<void(TaskProgress)>) {
            prerequisiteStarted.set_value();
            releaseFuture.wait();
        }));
    if (prerequisiteStartedFuture.wait_for(5s) != std::future_status::ready) {
        releasePrerequisite.set_value();
        pool.Shutdown();
        FAIL() << "shared prerequisite did not start";
        return;
    }

    TaskOptions firstOptions;
    firstOptions.priority = TaskPriority::High;
    firstOptions.dependencies.push_back(prerequisite);
    const auto first = pool.SubmitTask(makeTask(std::move(firstOptions),
        [&earlyRuns, &firstRuns]
        (CancelToken&, std::function<void(TaskProgress)>) {
            firstRuns.fetch_add(1, std::memory_order_relaxed);
            earlyRuns.fetch_add(1, std::memory_order_relaxed);
        }));

    TaskOptions secondOptions;
    secondOptions.priority = TaskPriority::Critical;
    secondOptions.dependencies.push_back(prerequisite);
    const auto second = pool.SubmitTask(makeTask(std::move(secondOptions),
        [&earlyRuns, &secondRuns]
        (CancelToken&, std::function<void(TaskProgress)>) {
            secondRuns.fetch_add(1, std::memory_order_relaxed);
            earlyRuns.fetch_add(1, std::memory_order_relaxed);
        }));

    EXPECT_EQ(pool.GetTaskSnapshot(first).state, TaskState::Pending);
    EXPECT_EQ(pool.GetTaskSnapshot(second).state, TaskState::Pending);
    EXPECT_EQ(earlyRuns.load(std::memory_order_relaxed), 0);

    releasePrerequisite.set_value();
    const bool firstCompleted = recorder.waitFor(first, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    });
    const bool secondCompleted = recorder.waitFor(second, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    });

    EXPECT_TRUE(firstCompleted);
    EXPECT_TRUE(secondCompleted);
    EXPECT_EQ(pool.GetTaskSnapshot(prerequisite).state, TaskState::Completed);
    EXPECT_EQ(firstRuns.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(secondRuns.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(earlyRuns.load(std::memory_order_relaxed), 2);
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest,
     FailedDependencyNeverRunsDependentAndLeavesIndependentWorkAvailable)
{
    BackgroundTaskWorkerPool pool{BackgroundTaskWorkerPool::Config{
        .maxWorkers = 2,
        .maxPendingTasks = 16,
        .enableDependencyResolution = true}};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::atomic<bool> dependentRan{false};
    std::promise<void> independentCompleted;
    pool.Start();

    const auto prerequisite = pool.SubmitTask(makeTask(TaskOptions{},
        [](CancelToken&, std::function<void(TaskProgress)>) {
            throw std::runtime_error("prerequisite failed");
        }));

    TaskOptions dependentOptions;
    dependentOptions.priority = TaskPriority::Critical;
    dependentOptions.dependencies.push_back(prerequisite);
    const auto dependent = pool.SubmitTask(makeTask(std::move(dependentOptions),
        [&dependentRan](CancelToken&, std::function<void(TaskProgress)>) {
            dependentRan.store(true, std::memory_order_release);
        }));
    const auto independent = pool.SubmitTask(makeTask(TaskOptions{},
        [&independentCompleted](CancelToken&, std::function<void(TaskProgress)>) {
            independentCompleted.set_value();
        }));

    const bool prerequisiteFailed = recorder.waitFor(prerequisite,
        [](const auto& snapshot) { return snapshot.state == TaskState::Failed; });
    const bool independentFinished = recorder.waitFor(independent,
        [](const auto& snapshot) { return snapshot.state == TaskState::Completed; });
    if (prerequisiteFailed && independentFinished) {
        // Let the dependency resolver and worker loop observe the failed
        // prerequisite before checking that the dependent remains unexecuted.
        std::this_thread::sleep_for(50ms);
    }

    EXPECT_TRUE(prerequisiteFailed);
    EXPECT_TRUE(independentFinished);
    EXPECT_FALSE(dependentRan.load(std::memory_order_acquire));
    EXPECT_EQ(recorder.snapshot(dependent).state, TaskState::Pending);
    EXPECT_EQ(recorder.snapshot(prerequisite).error.message,
              QStringLiteral("prerequisite failed"));

    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest,
     RestartRetainsCompletedSnapshotsAndRunsStoppedQueueDependenciesOnce)
{
    BackgroundTaskWorkerPool pool{BackgroundTaskWorkerPool::Config{
        .maxWorkers = 1,
        .maxPendingTasks = 16,
        .enableDependencyResolution = true}};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::atomic<int> executionCount{0};
    std::mutex orderMutex;
    std::vector<int> executionOrder;

    pool.Start();
    const auto completedBeforeRestart = pool.SubmitTask(makeTask(TaskOptions{},
        [&executionCount](CancelToken&, std::function<void(TaskProgress)>) {
            executionCount.fetch_add(1, std::memory_order_relaxed);
        }));
    ASSERT_TRUE(recorder.waitFor(completedBeforeRestart, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    pool.Shutdown();
    pool.Shutdown();
    EXPECT_EQ(recorder.snapshot(completedBeforeRestart).state, TaskState::Completed);

    auto prerequisiteTask = makeTask(TaskOptions{.priority = TaskPriority::Low},
        [&executionCount, &orderMutex, &executionOrder]
        (CancelToken&, std::function<void(TaskProgress)>) {
            executionCount.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard lock(orderMutex);
            executionOrder.push_back(1);
        });
    const TaskId prerequisite = prerequisiteTask->GetTaskId();
    TaskOptions dependentOptions;
    dependentOptions.priority = TaskPriority::Critical;
    dependentOptions.dependencies.push_back(prerequisite);
    const auto dependent = pool.SubmitTask(makeTask(std::move(dependentOptions),
        [&executionCount, &orderMutex, &executionOrder]
        (CancelToken&, std::function<void(TaskProgress)>) {
            executionCount.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard lock(orderMutex);
            executionOrder.push_back(2);
        }));
    EXPECT_EQ(pool.GetTaskSnapshot(dependent).state, TaskState::Pending);
    const TaskId queuedPrerequisite = pool.SubmitTask(std::move(prerequisiteTask));
    ASSERT_EQ(queuedPrerequisite, prerequisite);
    EXPECT_EQ(pool.GetPendingCount(), 2);

    pool.Start();
    const bool prerequisiteCompleted = recorder.waitFor(queuedPrerequisite,
        [](const auto& snapshot) { return snapshot.state == TaskState::Completed; });
    const bool dependentCompleted = recorder.waitFor(dependent,
        [](const auto& snapshot) { return snapshot.state == TaskState::Completed; });

    EXPECT_TRUE(prerequisiteCompleted);
    EXPECT_TRUE(dependentCompleted);
    EXPECT_EQ(recorder.snapshot(completedBeforeRestart).state, TaskState::Completed);
    EXPECT_EQ(executionCount.load(std::memory_order_relaxed), 3);
    {
        std::lock_guard lock(orderMutex);
        EXPECT_EQ(executionOrder, (std::vector<int>{1, 2}));
    }
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest,
     RestartedPoolResolvesDependencyFromPreviouslyCompletedSnapshot)
{
    BackgroundTaskWorkerPool pool{BackgroundTaskWorkerPool::Config{
        .maxWorkers = 1,
        .maxPendingTasks = 8,
        .enableDependencyResolution = true}};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::atomic<int> prerequisiteExecutions{0};
    std::atomic<int> dependentExecutions{0};
    pool.Start();

    const auto prerequisite = pool.SubmitTask(makeTask(TaskOptions{},
        [&prerequisiteExecutions]
        (CancelToken&, std::function<void(TaskProgress)>) {
            prerequisiteExecutions.fetch_add(1, std::memory_order_relaxed);
        }));
    ASSERT_TRUE(recorder.waitFor(prerequisite, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    pool.Shutdown();

    TaskOptions dependentOptions;
    dependentOptions.dependencies.push_back(prerequisite);
    const auto dependent = pool.SubmitTask(makeTask(std::move(dependentOptions),
        [&dependentExecutions](CancelToken&, std::function<void(TaskProgress)>) {
            dependentExecutions.fetch_add(1, std::memory_order_relaxed);
        }));
    EXPECT_EQ(pool.GetTaskSnapshot(prerequisite).state, TaskState::Completed);
    EXPECT_EQ(pool.GetTaskSnapshot(dependent).state, TaskState::Pending);

    pool.Start();
    const bool dependentCompleted = recorder.waitFor(dependent, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    });

    EXPECT_TRUE(dependentCompleted);
    EXPECT_EQ(pool.GetTaskSnapshot(prerequisite).state, TaskState::Completed);
    EXPECT_EQ(prerequisiteExecutions.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(dependentExecutions.load(std::memory_order_relaxed), 1);
    pool.Shutdown();
}

TEST(BackgroundTaskWorkerPoolContractTest,
     ShutdownWaitsForRunningTaskAndRestartResumesPendingTask)
{
    BackgroundTaskWorkerPool pool{singleWorkerConfig()};
    SnapshotRecorder recorder;
    recorder.attach(pool);
    std::promise<void> runningStarted;
    std::promise<void> releaseRunning;
    auto releaseFuture = releaseRunning.get_future().share();
    std::promise<void> shutdownStarted;
    std::atomic<int> pendingExecutions{0};
    pool.Start();

    const auto running = pool.SubmitTask(makeTask(TaskOptions{},
        [&runningStarted, releaseFuture]
        (CancelToken&, std::function<void(TaskProgress)>) {
            runningStarted.set_value();
            releaseFuture.wait();
        }));
    const bool taskStarted =
        runningStarted.get_future().wait_for(5s) == std::future_status::ready;
    if (!taskStarted) {
        releaseRunning.set_value();
        pool.Shutdown();
        FAIL() << "the running task did not start";
        return;
    }

    const auto pending = pool.SubmitTask(makeTask(TaskOptions{},
        [&pendingExecutions](CancelToken&, std::function<void(TaskProgress)>) {
            pendingExecutions.fetch_add(1, std::memory_order_relaxed);
        }));
    if (pool.GetTaskSnapshot(pending).state != TaskState::Pending) {
        releaseRunning.set_value();
        pool.Shutdown();
        FAIL() << "the queued task should remain pending behind the active worker";
        return;
    }

    auto shutdownFuture = std::async(std::launch::async,
        [&pool, &shutdownStarted] {
            shutdownStarted.set_value();
            pool.Shutdown();
        });
    if (shutdownStarted.get_future().wait_for(5s) != std::future_status::ready) {
        releaseRunning.set_value();
        shutdownFuture.wait();
        FAIL() << "the shutdown request did not start";
        return;
    }
    EXPECT_EQ(shutdownFuture.wait_for(50ms), std::future_status::timeout)
        << "shutdown must join the in-flight worker before returning";

    releaseRunning.set_value();
    shutdownFuture.get();
    EXPECT_EQ(pool.GetTaskSnapshot(running).state, TaskState::Completed);
    EXPECT_EQ(pool.GetTaskSnapshot(pending).state, TaskState::Pending);
    EXPECT_EQ(pendingExecutions.load(std::memory_order_relaxed), 0);

    pool.Start();
    EXPECT_TRUE(recorder.waitFor(pending, [](const auto& snapshot) {
        return snapshot.state == TaskState::Completed;
    }));
    EXPECT_EQ(pendingExecutions.load(std::memory_order_relaxed), 1);
    pool.Shutdown();
}
