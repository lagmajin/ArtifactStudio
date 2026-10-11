#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

import Data.FileWatcher;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
class TemporaryFile {
public:
    TemporaryFile()
    {
        static std::atomic_uint64_t nextId{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("artifact-file-watcher-" + std::to_string(stamp) + "-" +
             std::to_string(nextId.fetch_add(1)) + ".tmp");
        write();
    }

    ~TemporaryFile()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

    void changeTimestamp() const
    {
        const auto current = std::filesystem::last_write_time(path_);
        std::filesystem::last_write_time(path_, current + std::chrono::seconds(2));
    }

private:
    void write() const
    {
        std::ofstream file(path_, std::ios::binary);
        file << "watch";
        file.close();
        if (!file) throw std::runtime_error("failed to write temporary watched file");
    }

    std::filesystem::path path_;
};

struct WatchCleanup {
    ~WatchCleanup() { FileWatcher::instance().unwatchAll(); }
};

bool waitForCount(const std::shared_ptr<std::atomic_int>& count, int expected)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (count->load() >= expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return count->load() >= expected;
}
}

TEST(FileWatcherStandaloneTest, MissingPathIsIgnored)
{
    WatchCleanup cleanup;
    const auto absent = std::filesystem::temp_directory_path() / "artifact-file-watcher-absent.tmp";
    std::error_code error;
    std::filesystem::remove(absent, error);

    FileWatcher::instance().watch(String(absent.string()), [](const String&) {}, 5);

    EXPECT_FALSE(FileWatcher::instance().isWatching(String(absent.string())));
}

TEST(FileWatcherStandaloneTest, ExistingPathCanBeWatchedAndUnwatched)
{
    TemporaryFile file;
    WatchCleanup cleanup;
    auto& watcher = FileWatcher::instance();
    const String path(file.path().string());

    watcher.watch(path, [](const String&) {}, 5);
    EXPECT_TRUE(watcher.isWatching(path));
    watcher.unwatch(path);
    EXPECT_FALSE(watcher.isWatching(path));
}

TEST(FileWatcherStandaloneTest, RegistrationDoesNotNotifyWithoutAChange)
{
    TemporaryFile file;
    WatchCleanup cleanup;
    auto callbackCount = std::make_shared<std::atomic_int>(0);
    const String path(file.path().string());
    FileWatcher::instance().watch(path,
        [callbackCount](const String&) { callbackCount->fetch_add(1); }, 5);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(callbackCount->load(), 0);
}

TEST(FileWatcherStandaloneTest, UnwatchAllClearsEveryRegisteredPath)
{
    TemporaryFile first;
    TemporaryFile second;
    WatchCleanup cleanup;
    auto& watcher = FileWatcher::instance();
    const String firstPath(first.path().string());
    const String secondPath(second.path().string());

    watcher.watch(firstPath, [](const String&) {}, 5);
    watcher.watch(secondPath, [](const String&) {}, 5);
    watcher.unwatchAll();

    EXPECT_FALSE(watcher.isWatching(firstPath));
    EXPECT_FALSE(watcher.isWatching(secondPath));
}

TEST(FileWatcherStandaloneTest, TimestampChangeDispatchesCallbackWithWatchedPath)
{
    TemporaryFile file;
    WatchCleanup cleanup;
    auto& watcher = FileWatcher::instance();
    auto callbackCount = std::make_shared<std::atomic_int>(0);
    auto receivedPath = std::make_shared<std::atomic_bool>(false);
    const String path(file.path().string());

    watcher.watch(path, [callbackCount, receivedPath, expected = toStdString(path)](const String& changed) {
        if (toStdString(changed) == expected) receivedPath->store(true);
        callbackCount->fetch_add(1);
    }, 5);
    file.changeTimestamp();

    ASSERT_TRUE(waitForCount(callbackCount, 1));
    EXPECT_TRUE(receivedPath->load());
}

TEST(FileWatcherStandaloneTest, RewatchingPathReplacesItsCallback)
{
    TemporaryFile file;
    WatchCleanup cleanup;
    auto& watcher = FileWatcher::instance();
    auto oldCallbackCount = std::make_shared<std::atomic_int>(0);
    auto newCallbackCount = std::make_shared<std::atomic_int>(0);
    const String path(file.path().string());

    watcher.watch(path, [oldCallbackCount](const String&) { oldCallbackCount->fetch_add(1); }, 5);
    watcher.watch(path, [newCallbackCount](const String&) { newCallbackCount->fetch_add(1); }, 5);
    file.changeTimestamp();

    ASSERT_TRUE(waitForCount(newCallbackCount, 1));
    EXPECT_EQ(oldCallbackCount->load(), 0);
}

TEST(FileWatcherStandaloneTest, UnwatchedFileChangesNoLongerDispatch)
{
    TemporaryFile file;
    WatchCleanup cleanup;
    auto callbackCount = std::make_shared<std::atomic_int>(0);
    const String path(file.path().string());
    auto& watcher = FileWatcher::instance();
    watcher.watch(path, [callbackCount](const String&) { callbackCount->fetch_add(1); }, 5);
    watcher.unwatch(path);
    file.changeTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(callbackCount->load(), 0);
}

TEST(FileWatcherStandaloneTest, ChangeDispatchesOnlyForTheModifiedWatchedFile)
{
    TemporaryFile changedFile;
    TemporaryFile unchangedFile;
    WatchCleanup cleanup;
    auto changedCount = std::make_shared<std::atomic_int>(0);
    auto unchangedCount = std::make_shared<std::atomic_int>(0);
    auto& watcher = FileWatcher::instance();
    watcher.watch(String(changedFile.path().string()),
        [changedCount](const String&) { changedCount->fetch_add(1); }, 5);
    watcher.watch(String(unchangedFile.path().string()),
        [unchangedCount](const String&) { unchangedCount->fetch_add(1); }, 5);
    changedFile.changeTimestamp();

    ASSERT_TRUE(waitForCount(changedCount, 1));
    EXPECT_EQ(unchangedCount->load(), 0);
}

TEST(FileWatcherStandaloneTest, CallbackCanUnwatchItsOwnPath)
{
    TemporaryFile file;
    WatchCleanup cleanup;
    auto callbackCount = std::make_shared<std::atomic_int>(0);
    auto& watcher = FileWatcher::instance();
    const String path(file.path().string());
    watcher.watch(path, [callbackCount, path, &watcher](const String&) {
        watcher.unwatch(path);
        callbackCount->fetch_add(1);
    }, 5);
    file.changeTimestamp();

    ASSERT_TRUE(waitForCount(callbackCount, 1));
    EXPECT_FALSE(watcher.isWatching(path));
}

TEST(FileWatcherStandaloneTest, DeletedPathDoesNotTriggerOnTimestampPolling)
{
    TemporaryFile file;
    WatchCleanup cleanup;
    auto& watcher = FileWatcher::instance();
    auto callbackCount = std::make_shared<std::atomic_int>(0);
    const String path(file.path().string());
    watcher.watch(path, [callbackCount](const String&) { callbackCount->fetch_add(1); }, 5);
    ASSERT_TRUE(watcher.isWatching(path));

    std::filesystem::remove(file.path());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(callbackCount->load(), 0);
    EXPECT_TRUE(watcher.isWatching(path));
}
