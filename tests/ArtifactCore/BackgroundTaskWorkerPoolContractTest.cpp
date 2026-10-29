#include <gtest/gtest.h>

#include <QString>

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
