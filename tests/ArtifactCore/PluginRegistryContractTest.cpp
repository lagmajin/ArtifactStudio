#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;
import Core.ArtifactOptional;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
PluginDescriptor descriptor(const char* id, const char* name, PluginCategory category)
{
    PluginDescriptor result;
    result.id = String(id);
    result.displayName = String(name);
    result.version = String("1.0.0");
    result.category = category;
    result.apiVersion = 1;
    return result;
}

bool containsPlugin(const std::vector<PluginDescriptor>& plugins, const std::string& id)
{
    for (const auto& plugin : plugins) {
        if (toStdString(plugin.id) == id) return true;
    }
    return false;
}
}

TEST(PluginRegistryContractTest, LifecycleTransitionsAndReregistrationResetState)
{
    auto& registry = ArtifactPluginRegistry::instance();
    const std::string id = "contract.registry.lifecycle";

    EXPECT_FALSE(registry.isRegistered(id));
    EXPECT_FALSE(registry.isActive(id));
    EXPECT_EQ(registry.pluginState(id), PluginState::Discovered);
    registry.activatePlugin(id);
    EXPECT_FALSE(registry.isActive(id));

    const auto original = descriptor(id.c_str(), "Lifecycle", PluginCategory::Effect);
    registry.registerPlugin(original);
    EXPECT_TRUE(registry.isRegistered(id));
    EXPECT_EQ(registry.pluginState(id), PluginState::Registered);

    registry.activatePlugin(id);
    EXPECT_TRUE(registry.isActive(id));
    EXPECT_EQ(registry.pluginState(id), PluginState::Active);
    registry.deactivatePlugin(id);
    EXPECT_FALSE(registry.isActive(id));
    EXPECT_EQ(registry.pluginState(id), PluginState::Inactive);
    registry.activatePlugin(id);
    EXPECT_TRUE(registry.isActive(id));

    registry.unregisterPlugin(id);
    EXPECT_TRUE(registry.isRegistered(id));
    EXPECT_FALSE(registry.isActive(id));
    EXPECT_EQ(registry.pluginState(id), PluginState::Unloaded);
    registry.activatePlugin(id);
    EXPECT_EQ(registry.pluginState(id), PluginState::Unloaded);

    registry.registerPlugin(original);
    EXPECT_EQ(registry.pluginState(id), PluginState::Registered);
    EXPECT_FALSE(registry.isActive(id));
    registry.unregisterPlugin(id);
}

TEST(PluginRegistryContractTest, InvalidReplacementPreservesExistingActiveDescriptor)
{
    auto& registry = ArtifactPluginRegistry::instance();
    const std::string id = "contract.registry.invalid-replacement";
    const auto original = descriptor(id.c_str(), "Original", PluginCategory::Tool);
    registry.registerPlugin(original);
    registry.activatePlugin(id);

    auto invalidApi = descriptor(id.c_str(), "Invalid API", PluginCategory::ImportExport);
    invalidApi.apiVersion = 0;
    registry.registerPlugin(invalidApi);

    auto invalidCategory = descriptor(id.c_str(), "Invalid category", PluginCategory::Tool);
    invalidCategory.category = static_cast<PluginCategory>(-1);
    registry.registerPlugin(invalidCategory);

    auto emptyId = descriptor("", "Empty ID", PluginCategory::Tool);
    registry.registerPlugin(emptyId);

    const auto registered = registry.pluginById(id);
    ASSERT_TRUE(registered.has_value());
    EXPECT_EQ(registered->displayName, String("Original"));
    EXPECT_EQ(registered->category, PluginCategory::Tool);
    EXPECT_EQ(registered->state, PluginState::Active);
    EXPECT_TRUE(registry.isActive(id));
    EXPECT_FALSE(registry.isRegistered(""));
    registry.unregisterPlugin(id);
}

TEST(PluginRegistryContractTest, ReplacingDescriptorMovesCategoryAndRemovesActiveListing)
{
    auto& registry = ArtifactPluginRegistry::instance();
    const std::string id = "contract.registry.category-replacement";
    registry.registerPlugin(descriptor(id.c_str(), "Before", PluginCategory::Effect));
    registry.activatePlugin(id);
    ASSERT_TRUE(registry.isActive(id));

    auto replacement = descriptor(id.c_str(), "After", PluginCategory::ImportExport);
    replacement.version = String("2.0.0");
    registry.registerPlugin(replacement);

    EXPECT_EQ(registry.pluginState(id), PluginState::Registered);
    EXPECT_FALSE(registry.isActive(id));
    EXPECT_FALSE(containsPlugin(registry.pluginsOfCategory(PluginCategory::Effect), id));
    EXPECT_TRUE(containsPlugin(registry.pluginsOfCategory(PluginCategory::ImportExport), id));
    EXPECT_FALSE(containsPlugin(registry.activePlugins(), id));
    const auto updated = registry.pluginById(id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->displayName, String("After"));
    EXPECT_EQ(updated->version, String("2.0.0"));
    registry.unregisterPlugin(id);
}

TEST(PluginRegistryContractTest,
     UnregisterRetainsDescriptorTombstoneAndReregistrationResetsLifecycle)
{
    auto& registry = ArtifactPluginRegistry::instance();
    const std::string id = "contract.registry.tombstone";
    const auto original = descriptor(id.c_str(), "Tombstone", PluginCategory::Tool);
    registry.registerPlugin(original);
    registry.activatePlugin(id);
    ASSERT_TRUE(registry.isActive(id));

    registry.unregisterPlugin(id);
    registry.unregisterPlugin(id);

    EXPECT_TRUE(registry.isRegistered(id));
    EXPECT_EQ(registry.pluginState(id), PluginState::Unloaded);
    EXPECT_FALSE(registry.isActive(id));
    EXPECT_TRUE(containsPlugin(registry.allPlugins(), id));
    EXPECT_TRUE(containsPlugin(registry.pluginsOfCategory(PluginCategory::Tool), id));
    EXPECT_FALSE(containsPlugin(registry.activePlugins(), id));

    auto replacement = descriptor(id.c_str(), "Restored", PluginCategory::ImportExport);
    replacement.version = String("2.0.0");
    registry.registerPlugin(replacement);

    const auto restored = registry.pluginById(id);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->displayName, String("Restored"));
    EXPECT_EQ(restored->version, String("2.0.0"));
    EXPECT_EQ(restored->state, PluginState::Registered);
    EXPECT_FALSE(registry.isActive(id));
    EXPECT_FALSE(containsPlugin(registry.pluginsOfCategory(PluginCategory::Tool), id));
    EXPECT_TRUE(containsPlugin(registry.pluginsOfCategory(PluginCategory::ImportExport), id));
    EXPECT_FALSE(containsPlugin(registry.activePlugins(), id));
    registry.unregisterPlugin(id);
}

TEST(PluginRegistryContractTest,
     ConcurrentRegistrationActivationAndSnapshotQueriesRetainEveryPlugin)
{
    constexpr int workerCount = 8;
    constexpr int pluginsPerWorker = 24;
    constexpr int expectedPluginCount = workerCount * pluginsPerWorker;
    auto& registry = ArtifactPluginRegistry::instance();

    std::atomic<bool> start{false};
    std::atomic<int> readyWorkers{0};
    std::atomic<int> finishedWorkers{0};
    std::atomic<bool> workerOperationsSucceeded{true};
    std::vector<std::thread> workers;
    workers.reserve(workerCount);
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker] {
            readyWorkers.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int index = 0; index < pluginsPerWorker; ++index) {
                const std::string id = "contract.registry.concurrent." +
                    std::to_string(worker) + "." + std::to_string(index);
                registry.registerPlugin(descriptor(
                    id.c_str(), "Concurrent", PluginCategory::Effect));
                registry.activatePlugin(id);
                const auto registered = registry.pluginById(id);
                if (!registered.has_value() ||
                    registered->state != PluginState::Active ||
                    !registry.isActive(id)) {
                    workerOperationsSucceeded.store(false, std::memory_order_relaxed);
                }
            }
            finishedWorkers.fetch_add(1, std::memory_order_release);
        });
    }

    while (readyWorkers.load(std::memory_order_acquire) != workerCount) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    bool snapshotsAreValid = true;
    while (finishedWorkers.load(std::memory_order_acquire) != workerCount) {
        const auto activeSnapshot = registry.activePlugins();
        for (const auto& plugin : activeSnapshot) {
            if (toStdString(plugin.id).rfind(
                    "contract.registry.concurrent.", 0) == 0 &&
                (plugin.state != PluginState::Active ||
                 plugin.category != PluginCategory::Effect)) {
                snapshotsAreValid = false;
            }
        }
        const auto allSnapshot = registry.allPlugins();
        for (const auto& plugin : allSnapshot) {
            if (toStdString(plugin.id).empty()) snapshotsAreValid = false;
        }
        std::this_thread::yield();
    }
    for (auto& worker : workers) worker.join();

    EXPECT_TRUE(workerOperationsSucceeded.load(std::memory_order_relaxed));
    EXPECT_TRUE(snapshotsAreValid);
    int retainedCount = 0;
    for (int worker = 0; worker < workerCount; ++worker) {
        for (int index = 0; index < pluginsPerWorker; ++index) {
            const std::string id = "contract.registry.concurrent." +
                std::to_string(worker) + "." + std::to_string(index);
            const auto plugin = registry.pluginById(id);
            EXPECT_TRUE(plugin.has_value()) << id;
            if (plugin.has_value()) {
                EXPECT_EQ(plugin->state, PluginState::Active) << id;
                EXPECT_EQ(plugin->category, PluginCategory::Effect) << id;
                ++retainedCount;
            }
            EXPECT_TRUE(registry.isActive(id)) << id;
        }
    }
    EXPECT_EQ(retainedCount, expectedPluginCount);

    const auto finalActive = registry.activePlugins();
    int activeConcurrentCount = 0;
    for (const auto& plugin : finalActive) {
        const std::string id = toStdString(plugin.id);
        if (id.rfind("contract.registry.concurrent.", 0) == 0) {
            ++activeConcurrentCount;
            EXPECT_EQ(plugin.state, PluginState::Active);
        }
    }
    EXPECT_EQ(activeConcurrentCount, expectedPluginCount);

    for (int worker = 0; worker < workerCount; ++worker) {
        for (int index = 0; index < pluginsPerWorker; ++index) {
            registry.unregisterPlugin("contract.registry.concurrent." +
                std::to_string(worker) + "." + std::to_string(index));
        }
    }
}

TEST(PluginRegistryContractTest,
     ConcurrentReplacementReturnsOnlyCompleteDescriptorSnapshots)
{
    constexpr int writerCount = 2;
    constexpr int readerCount = 3;
    constexpr int writesPerWriter = 6000;
    constexpr int readsPerReader = 10000;
    const std::string id = "contract.registry.concurrent-replacement";
    auto& registry = ArtifactPluginRegistry::instance();
    registry.unregisterPlugin(id);

    auto first = descriptor(id.c_str(), "Writer A", PluginCategory::Effect);
    first.version = String("1.0.0");
    first.apiVersion = 1;
    auto second = descriptor(id.c_str(), "Writer B", PluginCategory::ImportExport);
    second.version = String("2.0.0");
    second.apiVersion = 2;
    registry.registerPlugin(first);

    std::atomic<bool> start{false};
    std::atomic<int> readyWorkers{0};
    std::atomic<bool> snapshotsWereCoherent{true};
    std::vector<std::thread> workers;
    workers.reserve(writerCount + readerCount);

    for (int writer = 0; writer < writerCount; ++writer) {
        workers.emplace_back([&, writer] {
            readyWorkers.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            const auto& replacement = writer == 0 ? first : second;
            for (int iteration = 0; iteration < writesPerWriter; ++iteration) {
                registry.registerPlugin(replacement);
                std::this_thread::yield();
            }
        });
    }

    for (int reader = 0; reader < readerCount; ++reader) {
        workers.emplace_back([&] {
            readyWorkers.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int iteration = 0; iteration < readsPerReader; ++iteration) {
                const auto snapshot = registry.pluginById(id);
                if (!snapshot.has_value()) {
                    snapshotsWereCoherent.store(false, std::memory_order_relaxed);
                    continue;
                }

                const bool isFirst = snapshot->displayName == String("Writer A") &&
                    snapshot->version == String("1.0.0") &&
                    snapshot->category == PluginCategory::Effect &&
                    snapshot->apiVersion == 1;
                const bool isSecond = snapshot->displayName == String("Writer B") &&
                    snapshot->version == String("2.0.0") &&
                    snapshot->category == PluginCategory::ImportExport &&
                    snapshot->apiVersion == 2;
                if (!isFirst && !isSecond) {
                    snapshotsWereCoherent.store(false, std::memory_order_relaxed);
                }
            }
        });
    }

    while (readyWorkers.load(std::memory_order_acquire) != writerCount + readerCount) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_TRUE(snapshotsWereCoherent.load(std::memory_order_relaxed));
    registry.unregisterPlugin(id);
}

TEST(PluginRegistryContractTest,
     ConcurrentLifecycleTransitionsKeepActiveSnapshotsCoherent)
{
    constexpr int writerCount = 4;
    constexpr int readerCount = 2;
    constexpr int operationsPerWriter = 1500;
    const std::string id = "contract.registry.concurrent-lifecycle";
    auto& registry = ArtifactPluginRegistry::instance();
    registry.unregisterPlugin(id);

    auto first = descriptor(id.c_str(), "Lifecycle A", PluginCategory::Effect);
    first.version = String("1.0.0");
    first.apiVersion = 1;
    auto second = descriptor(id.c_str(), "Lifecycle B", PluginCategory::ImportExport);
    second.version = String("2.0.0");
    second.apiVersion = 2;

    std::atomic<bool> start{false};
    std::atomic<int> readyWorkers{0};
    std::atomic<int> finishedWriters{0};
    std::atomic<bool> snapshotsWereCoherent{true};
    std::vector<std::thread> workers;
    workers.reserve(writerCount + readerCount);

    for (int writer = 0; writer < writerCount; ++writer) {
        workers.emplace_back([&, writer] {
            readyWorkers.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int iteration = 0; iteration < operationsPerWriter; ++iteration) {
                switch ((iteration + writer) % 5) {
                case 0: registry.registerPlugin(first); break;
                case 1: registry.activatePlugin(id); break;
                case 2: registry.deactivatePlugin(id); break;
                case 3: registry.registerPlugin(second); break;
                default: registry.unregisterPlugin(id); break;
                }
            }
            finishedWriters.fetch_add(1, std::memory_order_release);
        });
    }

    const auto descriptorIsComplete = [&](const PluginDescriptor& plugin) {
        const bool isFirst = plugin.displayName == String("Lifecycle A") &&
            plugin.version == String("1.0.0") &&
            plugin.category == PluginCategory::Effect && plugin.apiVersion == 1;
        const bool isSecond = plugin.displayName == String("Lifecycle B") &&
            plugin.version == String("2.0.0") &&
            plugin.category == PluginCategory::ImportExport && plugin.apiVersion == 2;
        const bool knownState = plugin.state == PluginState::Registered ||
            plugin.state == PluginState::Active || plugin.state == PluginState::Inactive ||
            plugin.state == PluginState::Unloaded;
        return (isFirst || isSecond) && knownState;
    };
    for (int reader = 0; reader < readerCount; ++reader) {
        workers.emplace_back([&] {
            readyWorkers.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            do {
                int activeMatches = 0;
                for (const auto& plugin : registry.activePlugins()) {
                    if (toStdString(plugin.id) != id) continue;
                    ++activeMatches;
                    if (plugin.state != PluginState::Active ||
                        !descriptorIsComplete(plugin)) {
                        snapshotsWereCoherent.store(false, std::memory_order_relaxed);
                    }
                }
                if (activeMatches > 1) {
                    snapshotsWereCoherent.store(false, std::memory_order_relaxed);
                }

                const auto snapshot = registry.pluginById(id);
                if (snapshot.has_value() && !descriptorIsComplete(*snapshot)) {
                    snapshotsWereCoherent.store(false, std::memory_order_relaxed);
                }
            } while (finishedWriters.load(std::memory_order_acquire) != writerCount);
        });
    }

    while (readyWorkers.load(std::memory_order_acquire) != writerCount + readerCount) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (auto& worker : workers) worker.join();

    EXPECT_TRUE(snapshotsWereCoherent.load(std::memory_order_relaxed));
    registry.unregisterPlugin(id);
    EXPECT_FALSE(registry.isActive(id));
    EXPECT_EQ(registry.pluginState(id), PluginState::Unloaded);
}
