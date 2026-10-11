#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

import Core.ArtifactString;
import ArtifactCore.Core.Simulation;
import Memory.SharedPtr;

using namespace ArtifactCore;

namespace {
class CountingSimulation final : public ISimulationSystem {
public:
    explicit CountingSimulation(String name) : name_(std::move(name)) {}

    void update(double deltaTime) override
    {
        ++updateCount;
        lastDelta = deltaTime;
    }

    void reset() override { ++resetCount; }
    const String& getName() const override { return name_; }
    void setPaused(bool paused) override { paused_ = paused; }
    bool isPaused() const override { return paused_; }

    int updateCount = 0;
    int resetCount = 0;
    double lastDelta = 0.0;

private:
    String name_;
    bool paused_ = false;
};

struct UpdateOrderState {
    std::array<int, 2> order{};
    int count = 0;
    std::array<int, 2> resetOrder{};
    int resetCount = 0;
};

class OrderedSimulation final : public ISimulationSystem {
public:
    OrderedSimulation(SharedPtr<UpdateOrderState> state, int order)
        : state_(std::move(state)), order_(order), name_(String("ordered")) {}

    void update(double) override
    {
        if (state_->count < static_cast<int>(state_->order.size()))
            state_->order[static_cast<std::size_t>(state_->count)] = order_;
        ++state_->count;
    }
    void reset() override
    {
        if (state_->resetCount < static_cast<int>(state_->resetOrder.size()))
            state_->resetOrder[static_cast<std::size_t>(state_->resetCount)] = order_;
        ++state_->resetCount;
    }
    const String& getName() const override { return name_; }
    void setPaused(bool paused) override { paused_ = paused; }
    bool isPaused() const override { return paused_; }

private:
    SharedPtr<UpdateOrderState> state_;
    int order_;
    String name_;
    bool paused_ = false;
};
}

TEST(SimulationSystemContractTest, ManagerUpdatesOnlyUnpausedSystemsAndResetsAll)
{
    auto& manager = SimulationManager::instance();
    auto active = makeShared<CountingSimulation>(String("active"));
    auto paused = makeShared<CountingSimulation>(String("paused"));
    paused->setPaused(true);
    manager.addSystem(active);
    manager.addSystem(paused);

    manager.update(0.25);
    EXPECT_EQ(active->getName(), "active");
    EXPECT_EQ(active->updateCount, 1);
    EXPECT_DOUBLE_EQ(active->lastDelta, 0.25);
    EXPECT_EQ(paused->updateCount, 0);

    manager.resetAll();
    EXPECT_EQ(active->resetCount, 1);
    EXPECT_EQ(paused->resetCount, 1);

    paused->setPaused(false);
    manager.update(0.5);
    EXPECT_EQ(active->updateCount, 2);
    EXPECT_EQ(paused->updateCount, 1);
    EXPECT_DOUBLE_EQ(paused->lastDelta, 0.5);
}

TEST(SimulationSystemContractTest, InstanceReturnsProcessWideManager)
{
    auto& first = SimulationManager::instance();
    auto& second = SimulationManager::instance();

    EXPECT_EQ(&first, &second);
}

TEST(SimulationSystemContractTest, ManagerForwardsZeroDeltaToUnpausedSystems)
{
    auto& manager = SimulationManager::instance();
    auto system = makeShared<CountingSimulation>(String("zero-delta"));
    manager.addSystem(system);

    manager.update(0.0);

    EXPECT_EQ(system->updateCount, 1);
    EXPECT_DOUBLE_EQ(system->lastDelta, 0.0);
}

TEST(SimulationSystemContractTest, ManagerForwardsNegativeDeltaWithoutClamping)
{
    auto& manager = SimulationManager::instance();
    auto system = makeShared<CountingSimulation>(String("negative-delta"));
    manager.addSystem(system);

    manager.update(-0.25);

    EXPECT_EQ(system->updateCount, 1);
    EXPECT_DOUBLE_EQ(system->lastDelta, -0.25);
}

TEST(SimulationSystemContractTest, ManagerForwardsNonFiniteDeltaValuesUnchanged)
{
    auto& manager = SimulationManager::instance();
    const double positiveInfinity = std::numeric_limits<double>::infinity();
    const double negativeInfinity = -positiveInfinity;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto positive = makeShared<CountingSimulation>(String("positive-infinity"));
    auto negative = makeShared<CountingSimulation>(String("negative-infinity"));
    auto notANumber = makeShared<CountingSimulation>(String("nan"));
    manager.addSystem(positive);
    manager.addSystem(negative);
    manager.addSystem(notANumber);

    manager.update(positiveInfinity);
    EXPECT_TRUE(std::isinf(positive->lastDelta));
    EXPECT_FALSE(std::signbit(positive->lastDelta));
    manager.update(negativeInfinity);
    EXPECT_TRUE(std::isinf(negative->lastDelta));
    EXPECT_TRUE(std::signbit(negative->lastDelta));
    manager.update(nan);

    EXPECT_EQ(positive->updateCount, 3);
    EXPECT_EQ(negative->updateCount, 3);
    EXPECT_EQ(notANumber->updateCount, 3);
    EXPECT_TRUE(std::isnan(notANumber->lastDelta));
}

TEST(SimulationSystemContractTest, ResetAllResetsPausedSystemsWithoutResumingThem)
{
    auto& manager = SimulationManager::instance();
    auto system = makeShared<CountingSimulation>(String("paused-reset"));
    system->setPaused(true);
    manager.addSystem(system);

    manager.resetAll();

    EXPECT_EQ(system->resetCount, 1);
    EXPECT_TRUE(system->isPaused());
}

TEST(SimulationSystemContractTest, ManagerUpdatesSystemsInRegistrationOrder)
{
    auto& manager = SimulationManager::instance();
    const auto state = makeShared<UpdateOrderState>();
    manager.addSystem(makeShared<OrderedSimulation>(state, 1));
    manager.addSystem(makeShared<OrderedSimulation>(state, 2));

    manager.update(0.125);

    ASSERT_EQ(state->count, 2);
    EXPECT_EQ(state->order, (std::array<int, 2>{1, 2}));
}

TEST(SimulationSystemContractTest, ManagerResetsSystemsInRegistrationOrder)
{
    auto& manager = SimulationManager::instance();
    const auto state = makeShared<UpdateOrderState>();
    manager.addSystem(makeShared<OrderedSimulation>(state, 1));
    manager.addSystem(makeShared<OrderedSimulation>(state, 2));

    manager.resetAll();

    ASSERT_EQ(state->resetCount, 2);
    EXPECT_EQ(state->resetOrder, (std::array<int, 2>{1, 2}));
}
