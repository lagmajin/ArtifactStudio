#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <typeindex>
#include <utility>
#include <vector>

import Event.Bus;

namespace {

struct ContractEvent {
    int value = 0;
};

struct ConcurrentContractEvent {
    int id = 0;
    int sequence = 0;
};

struct ConcurrentQueuedContractEvent {
    int priority = 0;
    int producer = 0;
    int sequence = 0;
};

} // namespace

TEST(EventBusContractTest,
     DisconnectDuringDispatchSkipsSubscriberAlreadyInDispatchSnapshot)
{
    ArtifactCore::EventBus bus;
    int firstSubscriberCalls = 0;
    int secondSubscriberCalls = 0;
    ArtifactCore::EventBus::Subscription secondSubscription;

    auto firstSubscription = bus.subscribe<ContractEvent>([&](const auto&) {
        ++firstSubscriberCalls;
        secondSubscription.disconnect();
    });
    secondSubscription = bus.subscribe<ContractEvent>([&](const auto&) {
        ++secondSubscriberCalls;
    });
    ASSERT_TRUE(firstSubscription.connected());
    ASSERT_TRUE(secondSubscription.connected());

    const auto delivered = bus.publish(ContractEvent{1});

    EXPECT_EQ(delivered, 1u);
    EXPECT_EQ(firstSubscriberCalls, 1);
    EXPECT_EQ(secondSubscriberCalls, 0);
    EXPECT_TRUE(firstSubscription.connected());
    EXPECT_FALSE(secondSubscription.connected());

    EXPECT_EQ(bus.publish(ContractEvent{2}), 1u);
    EXPECT_EQ(firstSubscriberCalls, 2);
    EXPECT_EQ(secondSubscriberCalls, 0);
}

TEST(EventBusContractTest,
     SubscriberAddedDuringDispatchJoinsNestedButNotInflightSnapshot)
{
    ArtifactCore::EventBus bus;
    std::vector<int> observed;
    std::size_t nestedDelivered = 0;
    ArtifactCore::EventBus::Subscription lateSubscription;

    auto firstSubscription = bus.subscribe<ContractEvent>([&](const auto& event) {
        observed.push_back(100 + event.value);
        if (event.value == 1 && !lateSubscription.connected()) {
            lateSubscription = bus.subscribe<ContractEvent>([&](const auto& nestedEvent) {
                observed.push_back(300 + nestedEvent.value);
            });
            nestedDelivered = bus.publish(ContractEvent{2});
        }
    });
    auto secondSubscription = bus.subscribe<ContractEvent>([&](const auto& event) {
        observed.push_back(200 + event.value);
    });
    ASSERT_TRUE(firstSubscription.connected());
    ASSERT_TRUE(secondSubscription.connected());

    EXPECT_EQ(bus.publish(ContractEvent{1}), 2u);
    EXPECT_EQ(nestedDelivered, 3u);
    EXPECT_EQ(observed, (std::vector<int>{101, 102, 202, 302, 201}));

    observed.clear();
    EXPECT_EQ(bus.publish(ContractEvent{3}), 3u);
    EXPECT_EQ(observed, (std::vector<int>{103, 203, 303}));
}

TEST(EventBusContractTest, NestedPublishDispatchesSynchronouslyWithoutDeadlock)
{
    ArtifactCore::EventBus bus;
    std::vector<int> observedValues;
    std::size_t nestedDelivered = 0;
    auto subscription = bus.subscribe<ContractEvent>([&](const auto& event) {
        observedValues.push_back(event.value);
        if (event.value == 1) {
            nestedDelivered = bus.publish(ContractEvent{2});
        }
    });
    ASSERT_TRUE(subscription.connected());

    const auto outerDelivered = bus.publish(ContractEvent{1});

    EXPECT_EQ(outerDelivered, 1u);
    EXPECT_EQ(nestedDelivered, 1u);
    EXPECT_EQ(observedValues, (std::vector<int>{1, 2}));
}

TEST(EventBusContractTest,
     DisconnectDuringInflightCallbackAllowsItToFinishAndBlocksFutureDispatch)
{
    ArtifactCore::EventBus bus;
    std::promise<void> callbackEntered;
    std::promise<void> releaseCallback;
    auto releaseFuture = releaseCallback.get_future().share();
    std::atomic<int> callbackCalls{0};
    std::atomic<std::size_t> publishDelivered{0};

    auto subscription = bus.subscribe<ContractEvent>(
        [&callbackEntered, releaseFuture, &callbackCalls](const auto&) {
            callbackCalls.fetch_add(1, std::memory_order_relaxed);
            callbackEntered.set_value();
            releaseFuture.wait();
        });
    ASSERT_TRUE(subscription.connected());

    std::thread publisher([&] {
        publishDelivered.store(bus.publish(ContractEvent{1}),
                              std::memory_order_release);
    });

    if (callbackEntered.get_future().wait_for(std::chrono::seconds(5)) !=
        std::future_status::ready) {
        subscription.disconnect();
        releaseCallback.set_value();
        publisher.join();
        FAIL() << "subscriber callback did not start";
        return;
    }

    subscription.disconnect();
    EXPECT_FALSE(subscription.connected());
    releaseCallback.set_value();
    publisher.join();

    EXPECT_EQ(callbackCalls.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(publishDelivered.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(bus.publish(ContractEvent{2}), 0u);
    EXPECT_EQ(callbackCalls.load(std::memory_order_relaxed), 1);
}

TEST(EventBusContractTest,
     ConcurrentPublishersDeliverEveryEventOnceWithOverlappingCallbacks)
{
    constexpr int publisherCount = 8;
    constexpr int eventsPerPublisher = 32;
    constexpr int totalEvents = publisherCount * eventsPerPublisher;
    ArtifactCore::EventBus bus;
    std::array<std::atomic<int>, totalEvents> deliveryCounts{};
    for (auto& count : deliveryCounts) count.store(0, std::memory_order_relaxed);
    std::atomic<int> activeCallbacks{0};
    std::atomic<int> maximumConcurrentCallbacks{0};
    std::atomic<bool> invalidEventId{false};
    std::atomic<bool> rendezvousCompleted{true};
    std::mutex rendezvousMutex;
    std::condition_variable rendezvousChanged;
    int callbacksAtRendezvous = 0;
    const auto rendezvousDeadline = std::chrono::steady_clock::now() +
                                    std::chrono::seconds(5);
    std::vector<std::size_t> deliveredPerPublisher(publisherCount, 0);

    auto subscription = bus.subscribe<ConcurrentContractEvent>(
        [&](const auto& event) {
            const int active = activeCallbacks.fetch_add(1, std::memory_order_acq_rel) + 1;
            int maximum = maximumConcurrentCallbacks.load(std::memory_order_relaxed);
            while (maximum < active &&
                   !maximumConcurrentCallbacks.compare_exchange_weak(
                       maximum, active, std::memory_order_relaxed)) {
            }

            if (event.sequence == 0) {
                std::unique_lock lock(rendezvousMutex);
                ++callbacksAtRendezvous;
                rendezvousChanged.notify_all();
                if (!rendezvousChanged.wait_until(lock, rendezvousDeadline, [&] {
                        return callbacksAtRendezvous == publisherCount;
                    })) {
                    rendezvousCompleted.store(false, std::memory_order_relaxed);
                }
            }
            if (event.id < 0 || event.id >= totalEvents) {
                invalidEventId.store(true, std::memory_order_relaxed);
            } else {
                deliveryCounts[static_cast<std::size_t>(event.id)].fetch_add(
                    1, std::memory_order_relaxed);
            }
            activeCallbacks.fetch_sub(1, std::memory_order_acq_rel);
        });
    ASSERT_TRUE(subscription.connected());

    std::vector<std::thread> publishers;
    publishers.reserve(publisherCount);
    for (int publisher = 0; publisher < publisherCount; ++publisher) {
        publishers.emplace_back([&, publisher] {
            for (int sequence = 0; sequence < eventsPerPublisher; ++sequence) {
                const int id = publisher * eventsPerPublisher + sequence;
                deliveredPerPublisher[static_cast<std::size_t>(publisher)] +=
                    bus.publish(ConcurrentContractEvent{id, sequence});
            }
        });
    }
    for (auto& publisher : publishers) publisher.join();

    EXPECT_FALSE(invalidEventId.load(std::memory_order_relaxed));
    EXPECT_TRUE(rendezvousCompleted.load(std::memory_order_relaxed));
    EXPECT_GE(maximumConcurrentCallbacks.load(std::memory_order_relaxed), publisherCount);
    for (int publisher = 0; publisher < publisherCount; ++publisher) {
        EXPECT_EQ(deliveredPerPublisher[static_cast<std::size_t>(publisher)],
                  static_cast<std::size_t>(eventsPerPublisher));
    }
    for (int id = 0; id < totalEvents; ++id) {
        EXPECT_EQ(deliveryCounts[static_cast<std::size_t>(id)].load(
                      std::memory_order_relaxed), 1)
            << "event id=" << id;
    }
}

TEST(EventBusContractTest,
     ClearingPublishHookDuringInvocationKeepsOnlyInflightCopy)
{
    ArtifactCore::EventBus bus;
    auto subscription = bus.subscribe<ContractEvent>([](const auto&) {});
    ASSERT_TRUE(subscription.connected());

    std::promise<void> hookEntered;
    std::promise<void> releaseHook;
    auto releaseFuture = releaseHook.get_future().share();
    std::atomic<int> hookCalls{0};
    bus.setPublishHook([&](auto&&...) {
        hookCalls.fetch_add(1, std::memory_order_relaxed);
        hookEntered.set_value();
        releaseFuture.wait();
    });

    std::thread publisher([&] { bus.publish(ContractEvent{1}); });
    if (hookEntered.get_future().wait_for(std::chrono::seconds(5)) !=
        std::future_status::ready) {
        bus.clearPublishHook();
        releaseHook.set_value();
        publisher.join();
        FAIL() << "publish hook did not start";
        return;
    }

    bus.clearPublishHook();
    releaseHook.set_value();
    publisher.join();

    EXPECT_EQ(hookCalls.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(bus.publish(ContractEvent{2}), 1u);
    EXPECT_EQ(hookCalls.load(std::memory_order_relaxed), 1);
}

TEST(EventBusContractTest,
     DrainHonorsPriorityOrderAndMaxEventsWithoutDroppingRemainder)
{
    ArtifactCore::EventBus bus;
    std::vector<int> observedValues;
    auto subscription = bus.subscribe<ContractEvent>([&](const auto& event) {
        observedValues.push_back(event.value);
    });
    ASSERT_TRUE(subscription.connected());

    bus.post(ArtifactCore::EventBus::EventPriority::Normal, ContractEvent{1});
    bus.post(ArtifactCore::EventBus::EventPriority::High, ContractEvent{2});
    bus.post(ArtifactCore::EventBus::EventPriority::High, ContractEvent{3});
    bus.post(ArtifactCore::EventBus::EventPriority::Low, ContractEvent{4});
    EXPECT_EQ(bus.pendingCount(), 4u);

    EXPECT_EQ(bus.drain(2), 2u);
    EXPECT_EQ(observedValues, (std::vector<int>{2, 3}));
    EXPECT_EQ(bus.pendingCount(), 2u);
    EXPECT_EQ(bus.drain(1), 1u);
    EXPECT_EQ(observedValues, (std::vector<int>{2, 3, 1}));
    EXPECT_EQ(bus.pendingCount(), 1u);
    EXPECT_EQ(bus.drain(), 1u);
    EXPECT_EQ(observedValues, (std::vector<int>{2, 3, 1, 4}));
    EXPECT_EQ(bus.pendingCount(), 0u);
}

TEST(EventBusContractTest, EventsPostedByCallbackRespectCurrentDrainLimit)
{
    ArtifactCore::EventBus bus;
    std::vector<int> observedValues;
    auto subscription = bus.subscribe<ContractEvent>([&](const auto& event) {
        observedValues.push_back(event.value);
        if (event.value == 1) bus.post(ContractEvent{2});
    });
    ASSERT_TRUE(subscription.connected());

    bus.post(ContractEvent{1});
    EXPECT_EQ(bus.drain(1), 1u);
    EXPECT_EQ(observedValues, (std::vector<int>{1}));
    EXPECT_EQ(bus.pendingCount(), 1u);

    EXPECT_EQ(bus.drain(), 1u);
    EXPECT_EQ(observedValues, (std::vector<int>{1, 2}));
    EXPECT_EQ(bus.pendingCount(), 0u);
}

TEST(EventBusContractTest,
     DrainPropagatesCallbackExceptionAndKeepsLaterEventsQueued)
{
    ArtifactCore::EventBus bus;
    std::vector<int> observedValues;
    int failingCallbackCalls = 0;
    auto subscription = bus.subscribe<ContractEvent>([&](const auto& event) {
        if (event.value == 1) {
            ++failingCallbackCalls;
            throw std::runtime_error("queued callback failure");
        }
        observedValues.push_back(event.value);
    });
    ASSERT_TRUE(subscription.connected());

    bus.post(ContractEvent{1});
    bus.post(ContractEvent{2});
    bus.post(ContractEvent{3});

    EXPECT_THROW(bus.drain(1), std::runtime_error);
    EXPECT_EQ(failingCallbackCalls, 1);
    EXPECT_TRUE(observedValues.empty());
    EXPECT_EQ(bus.pendingCount(), 2u);

    EXPECT_EQ(bus.drain(), 2u);
    EXPECT_EQ(observedValues, (std::vector<int>{2, 3}));
    EXPECT_EQ(bus.pendingCount(), 0u);
}

TEST(EventBusContractTest,
     PublishPropagatesCallbackExceptionAndSkipsRemainingCallbacksAndHook)
{
    ArtifactCore::EventBus bus;
    bool throwOnFirstCall = true;
    int firstCallbackCalls = 0;
    int secondCallbackCalls = 0;
    int hookCalls = 0;
    std::size_t hookedDeliveryCount = 0;

    auto firstSubscription = bus.subscribe<ContractEvent>([&](const auto&) {
        ++firstCallbackCalls;
        if (throwOnFirstCall) {
            throwOnFirstCall = false;
            throw std::runtime_error("synchronous callback failure");
        }
    });
    auto secondSubscription = bus.subscribe<ContractEvent>([&](const auto&) {
        ++secondCallbackCalls;
    });
    ASSERT_TRUE(firstSubscription.connected());
    ASSERT_TRUE(secondSubscription.connected());
    bus.setPublishHook([&](auto&&, auto&&, std::size_t delivered, auto&&, auto&&) {
        ++hookCalls;
        hookedDeliveryCount = delivered;
    });

    EXPECT_THROW(bus.publish(ContractEvent{1}), std::runtime_error);
    EXPECT_EQ(firstCallbackCalls, 1);
    EXPECT_EQ(secondCallbackCalls, 0);
    EXPECT_EQ(hookCalls, 0);

    EXPECT_EQ(bus.publish(ContractEvent{2}), 2u);
    EXPECT_EQ(firstCallbackCalls, 2);
    EXPECT_EQ(secondCallbackCalls, 1);
    EXPECT_EQ(hookCalls, 1);
    EXPECT_EQ(hookedDeliveryCount, 2u);
}

TEST(EventBusContractTest,
     QueuedEventsUseCurrentSubscribersAtDrainTimeAfterReplacement)
{
    ArtifactCore::EventBus bus;
    std::vector<int> originalSubscriberValues;
    std::vector<int> replacementSubscriberValues;
    auto originalSubscription = bus.subscribe<ContractEvent>(
        [&](const auto& event) { originalSubscriberValues.push_back(event.value); });
    ASSERT_TRUE(originalSubscription.connected());

    bus.post(ContractEvent{1});
    originalSubscription.disconnect();
    auto replacementSubscription = bus.subscribe<ContractEvent>(
        [&](const auto& event) { replacementSubscriberValues.push_back(event.value); });
    ASSERT_TRUE(replacementSubscription.connected());
    bus.post(ContractEvent{2});

    EXPECT_EQ(bus.pendingCount(), 2u);
    EXPECT_EQ(bus.drain(), 2u);
    EXPECT_TRUE(originalSubscriberValues.empty());
    EXPECT_EQ(replacementSubscriberValues, (std::vector<int>{1, 2}));
    EXPECT_EQ(bus.pendingCount(), 0u);
}

TEST(EventBusContractTest,
     ClearQueueDuringDrainKeepsBatchAndDropsEventsNotYetTaken)
{
    ArtifactCore::EventBus bus;
    std::promise<void> firstCallbackEntered;
    std::promise<void> releaseFirstCallback;
    auto releaseFuture = releaseFirstCallback.get_future().share();
    std::vector<int> observedValues;
    auto subscription = bus.subscribe<ContractEvent>(
        [&](const auto& event) {
            observedValues.push_back(event.value);
            if (event.value == 1) {
                firstCallbackEntered.set_value();
                releaseFuture.wait();
            }
        });
    ASSERT_TRUE(subscription.connected());

    for (int value = 1; value <= 4; ++value) {
        bus.post(ContractEvent{value});
    }
    ASSERT_EQ(bus.pendingCount(), 4u);

    std::atomic<std::size_t> drainedCount{0};
    std::thread drainer([&] {
        drainedCount.store(bus.drain(2), std::memory_order_release);
    });
    if (firstCallbackEntered.get_future().wait_for(std::chrono::seconds(5)) !=
        std::future_status::ready) {
        releaseFirstCallback.set_value();
        drainer.join();
        FAIL() << "first queued callback did not start";
        return;
    }

    EXPECT_EQ(bus.pendingCount(), 2u);
    bus.clearQueue();
    EXPECT_EQ(bus.pendingCount(), 0u);
    releaseFirstCallback.set_value();
    drainer.join();

    EXPECT_EQ(drainedCount.load(std::memory_order_acquire), 2u);
    EXPECT_EQ(observedValues, (std::vector<int>{1, 2}));
    EXPECT_EQ(bus.pendingCount(), 0u);
}

TEST(EventBusContractTest,
     ClearDuringDrainFinishesInflightCallbackAndResetsQueuedDispatchState)
{
    ArtifactCore::EventBus bus;
    std::promise<void> firstCallbackEntered;
    std::promise<void> releaseFirstCallback;
    auto releaseFuture = releaseFirstCallback.get_future().share();
    std::vector<int> observedValues;
    auto subscription = bus.subscribe<ContractEvent>(
        [&](const auto& event) {
            observedValues.push_back(event.value);
            if (event.value == 1) {
                firstCallbackEntered.set_value();
                releaseFuture.wait();
            }
        });
    ASSERT_TRUE(subscription.connected());

    for (int value = 1; value <= 4; ++value) {
        bus.post(ContractEvent{value});
    }
    std::atomic<std::size_t> drainedCount{0};
    std::thread drainer([&] {
        drainedCount.store(bus.drain(3), std::memory_order_release);
    });
    if (firstCallbackEntered.get_future().wait_for(std::chrono::seconds(5)) !=
        std::future_status::ready) {
        releaseFirstCallback.set_value();
        drainer.join();
        FAIL() << "first queued callback did not start";
        return;
    }

    EXPECT_EQ(bus.pendingCount(), 1u);
    bus.clear();
    EXPECT_FALSE(subscription.connected());
    EXPECT_EQ(bus.pendingCount(), 0u);
    EXPECT_EQ(bus.subscriberCount<ContractEvent>(), 0u);
    releaseFirstCallback.set_value();
    drainer.join();

    EXPECT_EQ(drainedCount.load(std::memory_order_acquire), 3u);
    EXPECT_EQ(observedValues, (std::vector<int>{1}));

    int freshSubscriberCalls = 0;
    auto freshSubscription = bus.subscribe<ContractEvent>([&](const auto&) {
        ++freshSubscriberCalls;
    });
    EXPECT_EQ(bus.publish(ContractEvent{5}), 1u);
    EXPECT_EQ(freshSubscriberCalls, 1);
}

TEST(EventBusContractTest,
     ConcurrentProducersPreservePriorityAndPerProducerFifo)
{
    constexpr int producerCount = 6;
    constexpr int eventsPerPriority = 24;
    constexpr int totalEvents = producerCount * eventsPerPriority * 3;
    using Priority = ArtifactCore::EventBus::EventPriority;
    ArtifactCore::EventBus bus;
    std::atomic<int> readyProducers{0};
    std::atomic<bool> startProducers{false};
    std::vector<ConcurrentQueuedContractEvent> observed;
    auto subscription = bus.subscribe<ConcurrentQueuedContractEvent>(
        [&](const auto& event) { observed.push_back(event); });
    ASSERT_TRUE(subscription.connected());

    std::vector<std::thread> producers;
    producers.reserve(producerCount);
    for (int producer = 0; producer < producerCount; ++producer) {
        producers.emplace_back([&, producer] {
            readyProducers.fetch_add(1, std::memory_order_release);
            while (!startProducers.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int sequence = 0; sequence < eventsPerPriority; ++sequence) {
                bus.post(Priority::High,
                         ConcurrentQueuedContractEvent{2, producer, sequence});
                bus.post(Priority::Normal,
                         ConcurrentQueuedContractEvent{1, producer, sequence});
                bus.post(Priority::Low,
                         ConcurrentQueuedContractEvent{0, producer, sequence});
            }
        });
    }
    while (readyProducers.load(std::memory_order_acquire) != producerCount) {
        std::this_thread::yield();
    }
    startProducers.store(true, std::memory_order_release);
    for (auto& producer : producers) producer.join();

    EXPECT_EQ(bus.pendingCount(), static_cast<std::size_t>(totalEvents));
    EXPECT_EQ(bus.drain(), static_cast<std::size_t>(totalEvents));
    ASSERT_EQ(observed.size(), static_cast<std::size_t>(totalEvents));

    std::array<int, totalEvents> deliveryCounts{};
    std::array<std::array<int, producerCount>, 3> lastSequence{};
    for (auto& priority : lastSequence) priority.fill(-1);
    int previousPriority = 2;
    for (const auto& event : observed) {
        ASSERT_GE(event.priority, 0);
        ASSERT_LE(event.priority, 2);
        EXPECT_LE(event.priority, previousPriority);
        previousPriority = event.priority;
        ASSERT_GE(event.producer, 0);
        ASSERT_LT(event.producer, producerCount);
        ASSERT_GE(event.sequence, 0);
        ASSERT_LT(event.sequence, eventsPerPriority);

        const int prioritySlot = 2 - event.priority;
        EXPECT_GT(event.sequence,
                  lastSequence[static_cast<std::size_t>(prioritySlot)]
                              [static_cast<std::size_t>(event.producer)]);
        lastSequence[static_cast<std::size_t>(prioritySlot)]
                    [static_cast<std::size_t>(event.producer)] = event.sequence;
        const int eventIndex =
            (event.producer * eventsPerPriority + event.sequence) * 3 + prioritySlot;
        ++deliveryCounts[static_cast<std::size_t>(eventIndex)];
    }
    for (int eventIndex = 0; eventIndex < totalEvents; ++eventIndex) {
        EXPECT_EQ(deliveryCounts[static_cast<std::size_t>(eventIndex)], 1)
            << "event index=" << eventIndex;
    }
}

TEST(EventBusContractTest,
     ConcurrentProducersAndDrainerPreserveEveryPerProducerSequence)
{
    constexpr int producerCount = 4;
    constexpr int eventsPerProducer = 128;
    constexpr int totalEvents = producerCount * eventsPerProducer;
    ArtifactCore::EventBus bus;
    std::atomic<int> readyProducers{0};
    std::atomic<int> finishedProducers{0};
    std::atomic<bool> startProducers{false};
    std::atomic<bool> consumerCompleted{false};
    std::atomic<int> drainedCount{0};
    std::vector<ConcurrentContractEvent> observed;
    auto subscription = bus.subscribe<ConcurrentContractEvent>(
        [&](const auto& event) { observed.push_back(event); });
    ASSERT_TRUE(subscription.connected());

    std::thread consumer([&] {
        int localDrainedCount = 0;
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(5);
        bool completed = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (finishedProducers.load(std::memory_order_acquire) == producerCount &&
                bus.pendingCount() == 0) {
                completed = true;
                break;
            }
            const auto drained = bus.drain(17);
            localDrainedCount += static_cast<int>(drained);
            if (drained == 0) std::this_thread::yield();
        }
        if (finishedProducers.load(std::memory_order_acquire) == producerCount &&
            bus.pendingCount() == 0) {
            completed = true;
        }
        drainedCount.store(localDrainedCount, std::memory_order_release);
        consumerCompleted.store(completed, std::memory_order_release);
    });

    std::vector<std::thread> producers;
    producers.reserve(producerCount);
    for (int producer = 0; producer < producerCount; ++producer) {
        producers.emplace_back([&, producer] {
            readyProducers.fetch_add(1, std::memory_order_release);
            while (!startProducers.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int sequence = 0; sequence < eventsPerProducer; ++sequence) {
                bus.post(ConcurrentContractEvent{producer, sequence});
            }
            finishedProducers.fetch_add(1, std::memory_order_release);
        });
    }
    while (readyProducers.load(std::memory_order_acquire) != producerCount) {
        std::this_thread::yield();
    }
    startProducers.store(true, std::memory_order_release);
    for (auto& producer : producers) producer.join();
    consumer.join();

    EXPECT_TRUE(consumerCompleted.load(std::memory_order_acquire));
    EXPECT_EQ(drainedCount.load(std::memory_order_acquire), totalEvents);
    EXPECT_EQ(bus.pendingCount(), 0u);
    ASSERT_EQ(observed.size(), static_cast<std::size_t>(totalEvents));

    std::array<int, producerCount> lastSequence{};
    lastSequence.fill(-1);
    std::array<std::array<int, eventsPerProducer>, producerCount> deliveryCounts{};
    for (const auto& event : observed) {
        ASSERT_GE(event.id, 0);
        ASSERT_LT(event.id, producerCount);
        ASSERT_GE(event.sequence, 0);
        ASSERT_LT(event.sequence, eventsPerProducer);
        EXPECT_GT(event.sequence, lastSequence[static_cast<std::size_t>(event.id)]);
        lastSequence[static_cast<std::size_t>(event.id)] = event.sequence;
        ++deliveryCounts[static_cast<std::size_t>(event.id)]
                        [static_cast<std::size_t>(event.sequence)];
    }
    for (int producer = 0; producer < producerCount; ++producer) {
        EXPECT_EQ(lastSequence[static_cast<std::size_t>(producer)],
                  eventsPerProducer - 1);
        for (int sequence = 0; sequence < eventsPerProducer; ++sequence) {
            EXPECT_EQ(deliveryCounts[static_cast<std::size_t>(producer)]
                                     [static_cast<std::size_t>(sequence)], 1);
        }
    }
}

TEST(EventBusContractTest,
     ConcurrentPostAndClearQueueRespectOperationLinearization)
{
    constexpr int producerCount = 6;
    constexpr int eventsPerProducer = 64;
    constexpr int racingEventBegin = 100;
    constexpr int postClearEventBegin = 600;
    constexpr int postClearEventCount = 16;
    constexpr int totalEventCount = 700;
    ArtifactCore::EventBus bus;
    std::vector<int> observedIds;
    auto subscription = bus.subscribe<ConcurrentContractEvent>(
        [&](const auto& event) { observedIds.push_back(event.id); });
    ASSERT_TRUE(subscription.connected());

    constexpr int preClearEventCount = 16;
    for (int id = 0; id < preClearEventCount; ++id) {
        bus.post(ConcurrentContractEvent{id, 0});
    }

    std::atomic<int> readyWorkers{0};
    std::atomic<bool> startRace{false};
    std::vector<std::thread> workers;
    workers.reserve(producerCount + 1);
    for (int producer = 0; producer < producerCount; ++producer) {
        workers.emplace_back([&, producer] {
            readyWorkers.fetch_add(1, std::memory_order_release);
            while (!startRace.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int sequence = 0; sequence < eventsPerProducer; ++sequence) {
                const int id = racingEventBegin + producer * eventsPerProducer + sequence;
                bus.post(ConcurrentContractEvent{id, sequence});
            }
        });
    }
    workers.emplace_back([&] {
        readyWorkers.fetch_add(1, std::memory_order_release);
        while (!startRace.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        bus.clearQueue();
    });

    while (readyWorkers.load(std::memory_order_acquire) != producerCount + 1) {
        std::this_thread::yield();
    }
    startRace.store(true, std::memory_order_release);
    for (auto& worker : workers) worker.join();

    for (int index = 0; index < postClearEventCount; ++index) {
        bus.post(ConcurrentContractEvent{postClearEventBegin + index, 0});
    }
    const auto queuedBeforeDrain = bus.pendingCount();
    const auto drained = bus.drain();
    EXPECT_EQ(drained, queuedBeforeDrain);
    EXPECT_EQ(observedIds.size(), drained);
    EXPECT_EQ(bus.pendingCount(), 0u);

    std::array<int, totalEventCount> deliveryCounts{};
    for (const int id : observedIds) {
        ASSERT_GE(id, 0);
        ASSERT_LT(id, totalEventCount);
        ++deliveryCounts[static_cast<std::size_t>(id)];
    }
    for (int id = 0; id < preClearEventCount; ++id) {
        EXPECT_EQ(deliveryCounts[static_cast<std::size_t>(id)], 0)
            << "pre-clear event id=" << id;
    }
    for (int index = 0; index < producerCount * eventsPerProducer; ++index) {
        const int id = racingEventBegin + index;
        EXPECT_LE(deliveryCounts[static_cast<std::size_t>(id)], 1)
            << "racing event id=" << id;
    }
    for (int index = 0; index < postClearEventCount; ++index) {
        const int id = postClearEventBegin + index;
        EXPECT_EQ(deliveryCounts[static_cast<std::size_t>(id)], 1)
            << "post-clear event id=" << id;
    }
}

TEST(EventBusContractTest,
     ConcurrentPriorityPostsAndClearKeepQueueOrderingAndStableTies)
{
    constexpr int producerCount = 4;
    constexpr int eventsPerProducer = 48;
    constexpr int totalRaceEvents = producerCount * eventsPerProducer;
    constexpr int markerProducer = producerCount;
    using Priority = ArtifactCore::EventBus::EventPriority;
    ArtifactCore::EventBus bus;
    std::vector<ConcurrentQueuedContractEvent> observed;
    auto subscription = bus.subscribe<ConcurrentQueuedContractEvent>(
        [&](const auto& event) { observed.push_back(event); });
    ASSERT_TRUE(subscription.connected());

    for (int priority = 0; priority <= 2; ++priority) {
        bus.post(static_cast<Priority>(priority),
                 ConcurrentQueuedContractEvent{priority, -1, priority});
    }

    std::atomic<int> readyWorkers{0};
    std::atomic<bool> startRace{false};
    std::atomic<bool> clearReturned{false};
    std::array<std::atomic<bool>, totalRaceEvents> expectedRaceDelivery{};
    for (auto& expected : expectedRaceDelivery) {
        expected.store(false, std::memory_order_relaxed);
    }
    std::vector<std::thread> workers;
    workers.reserve(producerCount + 1);
    for (int producer = 0; producer < producerCount; ++producer) {
        workers.emplace_back([&, producer] {
            readyWorkers.fetch_add(1, std::memory_order_release);
            while (!startRace.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int sequence = 0; sequence < eventsPerProducer; ++sequence) {
                const int priority = 2 - (sequence % 3);
                const bool clearHadReturned =
                    clearReturned.load(std::memory_order_acquire);
                bus.post(static_cast<Priority>(priority),
                         ConcurrentQueuedContractEvent{priority, producer, sequence});
                if (clearHadReturned) {
                    const int eventIndex = producer * eventsPerProducer + sequence;
                    expectedRaceDelivery[static_cast<std::size_t>(eventIndex)].store(
                        true, std::memory_order_release);
                }
            }
        });
    }
    workers.emplace_back([&] {
        readyWorkers.fetch_add(1, std::memory_order_release);
        while (!startRace.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        bus.clearQueue();
        clearReturned.store(true, std::memory_order_release);
    });
    while (readyWorkers.load(std::memory_order_acquire) != producerCount + 1) {
        std::this_thread::yield();
    }
    startRace.store(true, std::memory_order_release);
    for (auto& worker : workers) worker.join();

    for (int priority = 0; priority <= 2; ++priority) {
        bus.post(static_cast<Priority>(priority),
                 ConcurrentQueuedContractEvent{priority, markerProducer, priority});
    }
    const auto queuedCount = bus.pendingCount();
    EXPECT_EQ(bus.drain(), queuedCount);
    ASSERT_EQ(observed.size(), queuedCount);
    EXPECT_EQ(bus.pendingCount(), 0u);

    std::array<int, totalRaceEvents> raceDeliveryCounts{};
    std::array<std::array<int, producerCount>, 3> lastSequence{};
    for (auto& perPriority : lastSequence) perPriority.fill(-1);
    std::array<int, 3> markerCounts{};
    int previousPriority = 2;
    for (const auto& event : observed) {
        ASSERT_GE(event.priority, 0);
        ASSERT_LE(event.priority, 2);
        EXPECT_LE(event.priority, previousPriority);
        previousPriority = event.priority;

        if (event.producer == markerProducer) {
            ASSERT_EQ(event.sequence, event.priority);
            ++markerCounts[static_cast<std::size_t>(event.priority)];
            continue;
        }
        ASSERT_GE(event.producer, 0);
        ASSERT_LT(event.producer, producerCount);
        ASSERT_GE(event.sequence, 0);
        ASSERT_LT(event.sequence, eventsPerProducer);
        const int prioritySlot = 2 - event.priority;
        EXPECT_GT(event.sequence,
                  lastSequence[static_cast<std::size_t>(prioritySlot)]
                              [static_cast<std::size_t>(event.producer)]);
        lastSequence[static_cast<std::size_t>(prioritySlot)]
                    [static_cast<std::size_t>(event.producer)] = event.sequence;
        const int eventIndex = event.producer * eventsPerProducer + event.sequence;
        ++raceDeliveryCounts[static_cast<std::size_t>(eventIndex)];
    }
    EXPECT_EQ(markerCounts, (std::array<int, 3>{1, 1, 1}));
    for (int eventIndex = 0; eventIndex < totalRaceEvents; ++eventIndex) {
        EXPECT_LE(raceDeliveryCounts[static_cast<std::size_t>(eventIndex)], 1)
            << "race event index=" << eventIndex;
        if (expectedRaceDelivery[static_cast<std::size_t>(eventIndex)].load(
                std::memory_order_acquire)) {
            EXPECT_EQ(raceDeliveryCounts[static_cast<std::size_t>(eventIndex)], 1)
                << "post-clear race event index=" << eventIndex;
        }
    }
}

TEST(EventBusContractTest,
     RegisteredTypeIterationUsesSnapshotAndAllowsRegistryMutation)
{
    ArtifactCore::EventBus bus;
    auto originalSubscription = bus.subscribe<ContractEvent>([](const auto&) {});
    ASSERT_TRUE(originalSubscription.connected());
    ArtifactCore::EventBus::Subscription addedSubscription;
    bool mutationPerformed = false;
    std::vector<std::type_index> firstPassTypes;

    bus.forEachRegisteredType([&](std::type_index type, std::string_view name,
                                  std::size_t subscriberCount) {
        firstPassTypes.push_back(type);
        EXPECT_FALSE(name.empty());
        EXPECT_EQ(subscriberCount, 1u);
        if (type == std::type_index(typeid(ContractEvent))) {
            originalSubscription.disconnect();
            addedSubscription = bus.subscribe<ConcurrentContractEvent>(
                [](const auto&) {});
            mutationPerformed = true;
            EXPECT_EQ(bus.subscriberCount<ContractEvent>(), 0u);
        }
    });

    EXPECT_TRUE(mutationPerformed);
    EXPECT_EQ(firstPassTypes,
              (std::vector<std::type_index>{std::type_index(typeid(ContractEvent))}));
    ASSERT_TRUE(addedSubscription.connected());

    std::vector<std::type_index> secondPassTypes;
    bus.forEachRegisteredType([&](std::type_index type, std::string_view name,
                                  std::size_t subscriberCount) {
        secondPassTypes.push_back(type);
        EXPECT_FALSE(name.empty());
        EXPECT_EQ(subscriberCount, 1u);
    });
    EXPECT_EQ(secondPassTypes,
              (std::vector<std::type_index>{
                  std::type_index(typeid(ConcurrentContractEvent))}));
}

TEST(EventBusContractTest,
     ClearFromRegisteredTypeCallbackInvalidatesAllSnapshotEntriesSafely)
{
    ArtifactCore::EventBus bus;
    auto contractSubscription = bus.subscribe<ContractEvent>([](const auto&) {});
    auto concurrentSubscription = bus.subscribe<ConcurrentContractEvent>([](const auto&) {});
    ASSERT_TRUE(contractSubscription.connected());
    ASSERT_TRUE(concurrentSubscription.connected());
    bus.post(ContractEvent{1});
    bus.post(ConcurrentContractEvent{2, 0});

    std::vector<std::type_index> visitedTypes;
    std::vector<std::size_t> subscriberCounts;
    bool clearCalled = false;
    bus.forEachRegisteredType([&](std::type_index type, std::string_view name,
                                  std::size_t subscriberCount) {
        visitedTypes.push_back(type);
        subscriberCounts.push_back(subscriberCount);
        EXPECT_FALSE(name.empty());
        if (!clearCalled) {
            clearCalled = true;
            bus.clear();
        }
    });

    EXPECT_TRUE(clearCalled);
    ASSERT_EQ(visitedTypes.size(), 2u);
    EXPECT_NE(visitedTypes[0], visitedTypes[1]);
    ASSERT_EQ(subscriberCounts.size(), 2u);
    EXPECT_EQ(subscriberCounts[0], 1u);
    EXPECT_EQ(subscriberCounts[1], 0u);
    EXPECT_FALSE(contractSubscription.connected());
    EXPECT_FALSE(concurrentSubscription.connected());
    EXPECT_EQ(bus.subscriberCount<ContractEvent>(), 0u);
    EXPECT_EQ(bus.subscriberCount<ConcurrentContractEvent>(), 0u);
    EXPECT_EQ(bus.pendingCount(), 0u);
    EXPECT_EQ(bus.drain(), 0u);

    int callbacksAfterClear = 0;
    auto freshContractSubscription = bus.subscribe<ContractEvent>([&](const auto&) {
        ++callbacksAfterClear;
    });
    auto freshConcurrentSubscription =
        bus.subscribe<ConcurrentContractEvent>([&](const auto&) {
            ++callbacksAfterClear;
        });
    EXPECT_EQ(bus.publish(ContractEvent{3}), 1u);
    EXPECT_EQ(bus.publish(ConcurrentContractEvent{4, 1}), 1u);
    EXPECT_EQ(callbacksAfterClear, 2);
}

TEST(EventBusContractTest, CopiesShareSubscriptionsQueueHooksAndClearState)
{
    ArtifactCore::EventBus original;
    ArtifactCore::EventBus copy = original;
    int subscriberCalls = 0;
    auto subscription = original.subscribe<ContractEvent>([&](const auto&) {
        ++subscriberCalls;
    });
    ASSERT_TRUE(subscription.connected());

    copy.post(ContractEvent{1});
    EXPECT_EQ(original.pendingCount(), 1u);
    EXPECT_EQ(original.drain(), 1u);
    EXPECT_EQ(subscriberCalls, 1);

    int hookCalls = 0;
    original.setPublishHook([&](auto&&...) { ++hookCalls; });
    EXPECT_EQ(copy.publish(ContractEvent{2}), 1u);
    EXPECT_EQ(hookCalls, 1);

    original.post(ContractEvent{3});
    EXPECT_EQ(copy.pendingCount(), 1u);
    copy.clearQueue();
    EXPECT_EQ(original.pendingCount(), 0u);
    EXPECT_EQ(copy.drain(), 0u);

    original.post(ContractEvent{4});
    copy.clear();
    EXPECT_FALSE(subscription.connected());
    EXPECT_EQ(original.subscriberCount<ContractEvent>(), 0u);
    EXPECT_EQ(original.pendingCount(), 0u);
    EXPECT_EQ(original.publish(ContractEvent{5}), 0u);

    int freshSubscriberCalls = 0;
    auto freshSubscription = copy.subscribe<ContractEvent>([&](const auto&) {
        ++freshSubscriberCalls;
    });
    EXPECT_EQ(original.publish(ContractEvent{6}), 1u);
    EXPECT_EQ(freshSubscriberCalls, 1);
    EXPECT_EQ(hookCalls, 2);
}

TEST(EventBusContractTest, MoveConstructionTransfersStateAndLeavesSourceInert)
{
    ArtifactCore::EventBus source;
    int subscriberCalls = 0;
    int hookCalls = 0;
    auto subscription = source.subscribe<ContractEvent>([&](const auto&) {
        ++subscriberCalls;
    });
    ASSERT_TRUE(subscription.connected());
    source.setPublishHook([&](auto&&...) { ++hookCalls; });
    source.post(ContractEvent{1});

    ArtifactCore::EventBus destination(std::move(source));

    EXPECT_EQ(source.pendingCount(), 0u);
    EXPECT_EQ(source.subscriberCount<ContractEvent>(), 0u);
    EXPECT_EQ(source.drain(), 0u);
    EXPECT_EQ(source.publish(ContractEvent{2}), 0u);
    source.post(ContractEvent{3});
    source.clearQueue();
    source.clear();
    EXPECT_EQ(destination.pendingCount(), 1u);
    EXPECT_TRUE(subscription.connected());

    EXPECT_EQ(destination.drain(), 1u);
    EXPECT_EQ(subscriberCalls, 1);
    EXPECT_EQ(hookCalls, 1);
    EXPECT_EQ(destination.publish(ContractEvent{4}), 1u);
    EXPECT_EQ(subscriberCalls, 2);
    EXPECT_EQ(hookCalls, 2);

    auto sourceSubscription = source.subscribe<ContractEvent>([](const auto&) {});
    EXPECT_FALSE(sourceSubscription.connected());
    EXPECT_EQ(destination.subscriberCount<ContractEvent>(), 1u);
}

TEST(EventBusContractTest,
     MoveAssignmentReplacesOldQueueAndTransfersSourcePendingEvents)
{
    ArtifactCore::EventBus destination;
    destination.post(ContractEvent{90});
    EXPECT_EQ(destination.pendingCount(), 1u);

    ArtifactCore::EventBus source;
    std::vector<int> received;
    auto subscription = source.subscribe<ContractEvent>([&](const auto& event) {
        received.push_back(event.value);
    });
    ASSERT_TRUE(subscription.connected());
    source.post(ContractEvent{1});
    source.post(ContractEvent{2});

    destination = std::move(source);

    EXPECT_EQ(destination.pendingCount(), 2u);
    EXPECT_EQ(source.pendingCount(), 0u);
    source.clearQueue();
    source.post(ContractEvent{3});
    EXPECT_EQ(destination.pendingCount(), 2u);

    EXPECT_EQ(destination.drain(), 2u);
    EXPECT_EQ(received, (std::vector<int>{1, 2}));
    EXPECT_EQ(destination.pendingCount(), 0u);
    EXPECT_EQ(source.drain(), 0u);
}

TEST(EventBusContractTest,
     MoveAssignmentInvalidatesSubscriptionsFromReplacedDestination)
{
    ArtifactCore::EventBus destination;
    int replacedBusCalls = 0;
    auto replacedSubscription = destination.subscribe<ContractEvent>([&](const auto&) {
        ++replacedBusCalls;
    });
    ASSERT_TRUE(replacedSubscription.connected());

    ArtifactCore::EventBus source;
    int sourceBusCalls = 0;
    auto sourceSubscription = source.subscribe<ContractEvent>([&](const auto&) {
        ++sourceBusCalls;
    });
    ASSERT_TRUE(sourceSubscription.connected());

    destination = std::move(source);

    EXPECT_FALSE(replacedSubscription.connected());
    EXPECT_TRUE(sourceSubscription.connected());
    EXPECT_EQ(destination.subscriberCount<ContractEvent>(), 1u);
    EXPECT_EQ(destination.publish(ContractEvent{1}), 1u);
    EXPECT_EQ(replacedBusCalls, 0);
    EXPECT_EQ(sourceBusCalls, 1);

    replacedSubscription.disconnect();
    EXPECT_TRUE(sourceSubscription.connected());
    EXPECT_EQ(destination.publish(ContractEvent{2}), 1u);
    EXPECT_EQ(sourceBusCalls, 2);
}

TEST(EventBusContractTest,
     MoveAssignmentPreservesReplacedStateWhileAnotherCopyOwnsIt)
{
    ArtifactCore::EventBus destination;
    ArtifactCore::EventBus oldStateAlias = destination;
    int oldStateCalls = 0;
    auto oldStateSubscription = destination.subscribe<ContractEvent>([&](const auto&) {
        ++oldStateCalls;
    });
    ASSERT_TRUE(oldStateSubscription.connected());

    ArtifactCore::EventBus source;
    int movedStateCalls = 0;
    auto movedStateSubscription = source.subscribe<ContractEvent>([&](const auto&) {
        ++movedStateCalls;
    });
    ASSERT_TRUE(movedStateSubscription.connected());

    destination = std::move(source);

    EXPECT_TRUE(oldStateSubscription.connected());
    EXPECT_TRUE(movedStateSubscription.connected());
    EXPECT_EQ(oldStateAlias.publish(1), 1u);
    EXPECT_EQ(destination.publish(2), 1u);
    EXPECT_EQ(oldStateCalls, 1);
    EXPECT_EQ(movedStateCalls, 1);

    oldStateSubscription.disconnect();
    EXPECT_FALSE(oldStateSubscription.connected());
    EXPECT_TRUE(movedStateSubscription.connected());
    EXPECT_EQ(oldStateAlias.publish(3), 0u);
    EXPECT_EQ(destination.publish(4), 1u);
    EXPECT_EQ(oldStateCalls, 1);
    EXPECT_EQ(movedStateCalls, 2);
}

TEST(EventBusContractTest,
     SubscriptionMoveAssignmentDisconnectsOldRecordAndTransfersOwnership)
{
    ArtifactCore::EventBus bus;
    int replacedCalls = 0;
    int transferredCalls = 0;
    auto destination = bus.subscribe<ContractEvent>([&](const auto&) {
        ++replacedCalls;
    });
    auto source = bus.subscribe<ContractEvent>([&](const auto&) {
        ++transferredCalls;
    });
    ASSERT_TRUE(destination.connected());
    ASSERT_TRUE(source.connected());

    destination = std::move(source);

    EXPECT_FALSE(source.connected());
    EXPECT_TRUE(destination.connected());
    EXPECT_EQ(bus.subscriberCount<ContractEvent>(), 1u);
    EXPECT_EQ(bus.publish(ContractEvent{1}), 1u);
    EXPECT_EQ(replacedCalls, 0);
    EXPECT_EQ(transferredCalls, 1);

    destination = std::move(destination);
    EXPECT_TRUE(destination.connected());
    EXPECT_EQ(bus.subscriberCount<ContractEvent>(), 1u);
    EXPECT_EQ(bus.publish(ContractEvent{2}), 1u);
    EXPECT_EQ(transferredCalls, 2);

    destination.disconnect();
    EXPECT_FALSE(destination.connected());
    EXPECT_EQ(bus.subscriberCount<ContractEvent>(), 0u);
    EXPECT_EQ(bus.publish(ContractEvent{3}), 0u);
}

TEST(EventBusContractTest, SubscriptionDestructorDisconnectsOnlyItsOwnRecord)
{
    ArtifactCore::EventBus bus;
    ArtifactCore::EventBus alias = bus;
    int persistentCalls = 0;
    int scopedCalls = 0;
    auto persistentSubscription = bus.subscribe<ContractEvent>([&](const auto&) {
        ++persistentCalls;
    });
    ASSERT_TRUE(persistentSubscription.connected());

    {
        auto scopedSubscription = alias.subscribe<ContractEvent>([&](const auto&) {
            ++scopedCalls;
        });
        ASSERT_TRUE(scopedSubscription.connected());
        EXPECT_EQ(bus.subscriberCount<ContractEvent>(), 2u);
    }

    EXPECT_EQ(alias.subscriberCount<ContractEvent>(), 1u);
    EXPECT_EQ(alias.publish(ContractEvent{1}), 1u);
    EXPECT_EQ(persistentCalls, 1);
    EXPECT_EQ(scopedCalls, 0);
}
