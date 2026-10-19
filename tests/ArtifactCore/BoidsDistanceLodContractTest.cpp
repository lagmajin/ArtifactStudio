#include <gtest/gtest.h>

#include <cmath>
#include <limits>

import ArtifactCore.Crowd.Boids;

using namespace ArtifactCore;

TEST(BoidsDistanceLodContractTest, FarAgentsUseReducedNeighborRange)
{
    BoidsSwarmSystem fullDetail;
    BoidsSwarmSystem reducedDetail;
    for (auto* system : {&fullDetail, &reducedDetail}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({30.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) agent.desiredSeparation = 40.0f;
    }
    fullDetail.setCameraPosition({15.0f, 0.0f, 0.0f});
    reducedDetail.setCameraPosition({1000.0f, 0.0f, 0.0f});

    fullDetail.update(1.0f / 60.0f);
    reducedDetail.update(1.0f / 60.0f);

    ASSERT_EQ(fullDetail.getAgents().size(), 2u);
    ASSERT_EQ(reducedDetail.getAgents().size(), 2u);
    EXPECT_GT(std::abs(fullDetail.getAgents()[0].velocity.x),
              std::abs(reducedDetail.getAgents()[0].velocity.x));
    EXPECT_GT(std::abs(fullDetail.getAgents()[1].velocity.x),
              std::abs(reducedDetail.getAgents()[1].velocity.x));
}

TEST(BoidsDistanceLodContractTest, CameraDistanceUsesVerticalAndDepthAxes)
{
    for (int axis : {1, 2}) {
        BoidsSwarmSystem fullDetail;
        BoidsSwarmSystem reducedDetail;
        for (auto* system : {&fullDetail, &reducedDetail}) {
            system->minAgentCount = 2;
            system->maxAgentCount = 2;
            system->separationWeight = 1.0f;
            system->alignmentWeight = 0.0f;
            system->cohesionWeight = 0.0f;
            system->wanderWeight = 0.0f;
            system->obstacleWeight = 0.0f;
            system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
            system->addAgent({30.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
            for (auto& agent : system->getAgents()) {
                agent.desiredSeparation = 40.0f;
            }
        }
        fullDetail.setCameraPosition({15.0f, 0.0f, 0.0f});
        float3 distantCamera{0.0f, 0.0f, 0.0f};
        if (axis == 1) {
            distantCamera.y = 301.0f;
        } else {
            distantCamera.z = 301.0f;
        }
        reducedDetail.setCameraPosition(distantCamera);

        fullDetail.update(1.0f / 60.0f);
        reducedDetail.update(1.0f / 60.0f);

        ASSERT_EQ(fullDetail.getAgents().size(), 2u);
        ASSERT_EQ(reducedDetail.getAgents().size(), 2u);
        EXPECT_GT(std::abs(fullDetail.getAgents()[0].velocity.x),
                  std::abs(reducedDetail.getAgents()[0].velocity.x))
            << "camera axis " << axis;
        EXPECT_GT(std::abs(fullDetail.getAgents()[1].velocity.x),
                  std::abs(reducedDetail.getAgents()[1].velocity.x))
            << "camera axis " << axis;
    }
}

TEST(BoidsDistanceLodContractTest,
     NonFiniteCameraDistanceFallsBackToFullNeighborRange)
{
    BoidsSwarmSystem fullDetail;
    BoidsSwarmSystem invalidCamera;
    for (auto* system : {&fullDetail, &invalidCamera}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({30.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) {
            agent.desiredSeparation = 40.0f;
        }
    }
    fullDetail.setCameraPosition({15.0f, 0.0f, 0.0f});
    invalidCamera.setCameraPosition({
        std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f});

    fullDetail.update(1.0f / 60.0f);
    invalidCamera.update(1.0f / 60.0f);

    ASSERT_EQ(fullDetail.getAgents().size(), 2u);
    ASSERT_EQ(invalidCamera.getAgents().size(), 2u);
    EXPECT_FLOAT_EQ(invalidCamera.getAgents()[0].velocity.x,
                    fullDetail.getAgents()[0].velocity.x);
    EXPECT_FLOAT_EQ(invalidCamera.getAgents()[1].velocity.x,
                    fullDetail.getAgents()[1].velocity.x)
        << "an undefined camera distance should not reduce neighbor range";
}

TEST(BoidsDistanceLodContractTest,
     NonFiniteDistanceThresholdDoesNotReduceNeighborRange)
{
    BoidsSwarmSystem fullDetail;
    BoidsSwarmSystem invalidThreshold;
    for (auto* system : {&fullDetail, &invalidThreshold}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({30.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) {
            agent.desiredSeparation = 40.0f;
        }
    }
    fullDetail.setCameraPosition({15.0f, 0.0f, 0.0f});
    invalidThreshold.setCameraPosition({1000.0f, 0.0f, 0.0f});
    invalidThreshold.lodThreshold2 =
        -std::numeric_limits<float>::infinity();

    fullDetail.update(1.0f / 60.0f);
    invalidThreshold.update(1.0f / 60.0f);

    ASSERT_EQ(fullDetail.getAgents().size(), 2u);
    ASSERT_EQ(invalidThreshold.getAgents().size(), 2u);
    EXPECT_FLOAT_EQ(invalidThreshold.getAgents()[0].velocity.x,
                    fullDetail.getAgents()[0].velocity.x);
    EXPECT_FLOAT_EQ(invalidThreshold.getAgents()[1].velocity.x,
                    fullDetail.getAgents()[1].velocity.x)
        << "an invalid distance threshold should not silently reduce the "
           "neighbor search range";
}

TEST(BoidsDistanceLodContractTest,
     NaNDistanceThresholdsFallBackToFullNeighborRange)
{
    BoidsSwarmSystem fullDetail;
    BoidsSwarmSystem invalidThresholds;
    for (auto* system : {&fullDetail, &invalidThresholds}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({30.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) {
            agent.desiredSeparation = 40.0f;
        }
    }
    fullDetail.setCameraPosition({15.0f, 0.0f, 0.0f});
    invalidThresholds.setCameraPosition({1000.0f, 0.0f, 0.0f});
    invalidThresholds.lodThreshold1 =
        std::numeric_limits<float>::quiet_NaN();
    invalidThresholds.lodThreshold2 =
        std::numeric_limits<float>::quiet_NaN();

    fullDetail.update(1.0f / 60.0f);
    invalidThresholds.update(1.0f / 60.0f);

    ASSERT_EQ(fullDetail.getAgents().size(), 2u);
    ASSERT_EQ(invalidThresholds.getAgents().size(), 2u);
    EXPECT_FLOAT_EQ(invalidThresholds.getAgents()[0].velocity.x,
                    fullDetail.getAgents()[0].velocity.x);
    EXPECT_FLOAT_EQ(invalidThresholds.getAgents()[1].velocity.x,
                    fullDetail.getAgents()[1].velocity.x)
        << "unordered NaN threshold comparisons should retain the full "
           "neighbor search range";
}

TEST(BoidsDistanceLodContractTest, ThresholdBoundariesRemainFullAtEquality)
{
    BoidsSwarmSystem atFirstThreshold;
    BoidsSwarmSystem beyondFirstThreshold;
    for (auto* system : {&atFirstThreshold, &beyondFirstThreshold}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({30.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) agent.desiredSeparation = 40.0f;
    }
    atFirstThreshold.setCameraPosition({300.0f, 0.0f, 0.0f});
    beyondFirstThreshold.setCameraPosition({300.01f, 0.0f, 0.0f});

    atFirstThreshold.update(1.0f / 60.0f);
    beyondFirstThreshold.update(1.0f / 60.0f);

    EXPECT_GT(std::abs(atFirstThreshold.getAgents()[0].velocity.x),
              std::abs(beyondFirstThreshold.getAgents()[0].velocity.x));
}

TEST(BoidsDistanceLodContractTest, CustomDistanceThresholdsControlBandTransitions)
{
    BoidsSwarmSystem atCustomThreshold;
    BoidsSwarmSystem beyondCustomThreshold;
    for (auto* system : {&atCustomThreshold, &beyondCustomThreshold}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->lodThreshold1 = 50.0f;
        system->lodThreshold2 = 100.0f;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({30.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) {
            agent.desiredSeparation = 40.0f;
        }
    }
    atCustomThreshold.setCameraPosition({-50.0f, 0.0f, 0.0f});
    beyondCustomThreshold.setCameraPosition({-50.01f, 0.0f, 0.0f});

    atCustomThreshold.update(1.0f / 60.0f);
    beyondCustomThreshold.update(1.0f / 60.0f);

    ASSERT_EQ(atCustomThreshold.getAgents().size(), 2u);
    ASSERT_EQ(beyondCustomThreshold.getAgents().size(), 2u);
    EXPECT_GT(std::abs(atCustomThreshold.getAgents()[0].velocity.x),
              std::abs(beyondCustomThreshold.getAgents()[0].velocity.x))
        << "the custom threshold should use full neighbor distance at equality "
           "and Medium LOD immediately beyond it";
}

TEST(BoidsDistanceLodContractTest,
     CustomFarThresholdUsesMediumNeighborRangeAtEquality)
{
    BoidsSwarmSystem atCustomFarThreshold;
    BoidsSwarmSystem beyondCustomFarThreshold;
    for (auto* system : {&atCustomFarThreshold, &beyondCustomFarThreshold}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->lodThreshold1 = 5.0f;
        system->lodThreshold2 = 10.0f;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({20.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) {
            agent.desiredSeparation = 40.0f;
        }
    }
    atCustomFarThreshold.setCameraPosition({-10.0f, 0.0f, 0.0f});
    beyondCustomFarThreshold.setCameraPosition({-10.01f, 0.0f, 0.0f});

    atCustomFarThreshold.update(1.0f / 60.0f);
    beyondCustomFarThreshold.update(1.0f / 60.0f);

    ASSERT_EQ(atCustomFarThreshold.getAgents().size(), 2u);
    ASSERT_EQ(beyondCustomFarThreshold.getAgents().size(), 2u);
    EXPECT_GT(std::abs(atCustomFarThreshold.getAgents()[0].velocity.x),
              std::abs(beyondCustomFarThreshold.getAgents()[0].velocity.x))
        << "the configured far threshold is inclusive for Medium LOD; "
           "the immediately larger distance should use Far LOD's shorter "
           "neighbor range";
}

TEST(BoidsDistanceLodContractTest, SecondDistanceThresholdReducesNeighborRangeAgain)
{
    BoidsSwarmSystem mediumDetail;
    BoidsSwarmSystem lowDetail;
    for (auto* system : {&mediumDetail, &lowDetail}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({20.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) agent.desiredSeparation = 40.0f;
    }
    mediumDetail.setCameraPosition({-400.0f, 0.0f, 0.0f});
    lowDetail.setCameraPosition({-700.0f, 0.0f, 0.0f});

    mediumDetail.update(1.0f / 60.0f);
    lowDetail.update(1.0f / 60.0f);

    ASSERT_EQ(mediumDetail.getAgents().size(), 2u);
    ASSERT_EQ(lowDetail.getAgents().size(), 2u);
    EXPECT_LT(mediumDetail.getAgents()[0].velocity.x,
              lowDetail.getAgents()[0].velocity.x);
    EXPECT_GT(mediumDetail.getAgents()[1].velocity.x,
              lowDetail.getAgents()[1].velocity.x);
}

TEST(BoidsDistanceLodContractTest, SecondThresholdEqualityUsesMediumNeighborRange)
{
    BoidsSwarmSystem atSecondThreshold;
    BoidsSwarmSystem beyondSecondThreshold;
    for (auto* system : {&atSecondThreshold, &beyondSecondThreshold}) {
        system->minAgentCount = 2;
        system->maxAgentCount = 2;
        system->separationWeight = 1.0f;
        system->alignmentWeight = 0.0f;
        system->cohesionWeight = 0.0f;
        system->wanderWeight = 0.0f;
        system->obstacleWeight = 0.0f;
        system->addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        system->addAgent({20.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
        for (auto& agent : system->getAgents()) agent.desiredSeparation = 40.0f;
    }
    atSecondThreshold.setCameraPosition({-600.0f, 0.0f, 0.0f});
    beyondSecondThreshold.setCameraPosition({-600.01f, 0.0f, 0.0f});

    atSecondThreshold.update(1.0f / 60.0f);
    beyondSecondThreshold.update(1.0f / 60.0f);

    EXPECT_LT(atSecondThreshold.getAgents()[0].velocity.x,
              beyondSecondThreshold.getAgents()[0].velocity.x);
}

TEST(BoidsDistanceLodContractTest,
     SecondThresholdEqualityUsesMediumForceUpdateCadence)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.setCameraPosition({-600.0f, 0.0f, 0.0f});
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});

    system.update(1.0f / 60.0f);
    ASSERT_EQ(system.getAgents().size(), 1u);
    const float mediumLodAcceleration = system.getAgents()[0].acceleration.x;
    ASSERT_GT(mediumLodAcceleration, 0.0f);

    system.setTarget({-100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    EXPECT_FLOAT_EQ(system.getAgents()[0].acceleration.x,
                    mediumLodAcceleration)
        << "distance equal to the second threshold belongs to Medium LOD, "
           "which skips one intervening force update";

    system.update(1.0f / 60.0f);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "Medium LOD should refresh the changed force after its single skip";
}

TEST(BoidsDistanceLodContractTest, FarAgentRecomputesForcesAfterThreeSkippedFrames)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.setCameraPosition({-1000.0f, 0.0f, 0.0f});
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});

    system.update(1.0f / 60.0f);
    ASSERT_EQ(system.getAgents().size(), 1u);
    ASSERT_GT(system.getAgents()[0].acceleration.x, 0.0f);

    system.setTarget({-100.0f, 0.0f, 0.0f});
    for (int skippedFrame = 0; skippedFrame < 3; ++skippedFrame) {
        const float previousAcceleration = system.getAgents()[0].acceleration.x;
        system.update(1.0f / 60.0f);
        EXPECT_FLOAT_EQ(system.getAgents()[0].acceleration.x,
                        previousAcceleration)
            << "The LOD2 agent should carry its cached force through skipped frames";
    }

    system.update(1.0f / 60.0f);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "The distant agent should recompute its force after three skipped frames";
}

TEST(BoidsDistanceLodContractTest,
     ReplacingFarAgentStartsWithFreshLodSkipState)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.setCameraPosition({-1000.0f, 0.0f, 0.0f});
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});

    system.update(1.0f / 60.0f);
    ASSERT_EQ(system.getAgents().size(), 1u);
    ASSERT_GT(system.getAgents()[0].acceleration.x, 0.0f);

    system.setTarget({-100.0f, 0.0f, 0.0f});
    system.removeAgent(0);
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    ASSERT_EQ(system.getTrails().size(), 1u);

    system.update(1.0f / 60.0f);
    ASSERT_EQ(system.getAgents().size(), 1u);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "a replacement agent should recompute its force immediately instead "
           "of inheriting the removed agent's far-LOD skip window";
}

TEST(BoidsDistanceLodContractTest, NonPositiveDeltaDoesNotConsumeFarLodSkipWindow)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.setCameraPosition({-1000.0f, 0.0f, 0.0f});
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});

    system.update(1.0f / 60.0f);
    ASSERT_EQ(system.getAgents().size(), 1u);
    ASSERT_GT(system.getAgents()[0].acceleration.x, 0.0f);

    system.setTarget({-100.0f, 0.0f, 0.0f});
    const float cachedAcceleration = system.getAgents()[0].acceleration.x;
    system.update(0.0f);
    EXPECT_FLOAT_EQ(system.getAgents()[0].acceleration.x, cachedAcceleration);
    system.update(-1.0f / 60.0f);
    EXPECT_FLOAT_EQ(system.getAgents()[0].acceleration.x, cachedAcceleration);

    for (int skippedFrame = 0; skippedFrame < 3; ++skippedFrame) {
        system.update(1.0f / 60.0f);
        EXPECT_FLOAT_EQ(system.getAgents()[0].acceleration.x, cachedAcceleration)
            << "a zero-delta call must not shorten the far-LOD skip window";
    }

    system.update(1.0f / 60.0f);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "force computation should resume after the original skip window";
}

TEST(BoidsDistanceLodContractTest, MediumDistanceAgentSkipsOneForceUpdate)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.setCameraPosition({-400.0f, 0.0f, 0.0f});
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});

    system.update(1.0f / 60.0f);
    ASSERT_EQ(system.getAgents().size(), 1u);
    ASSERT_GT(system.getAgents()[0].acceleration.x, 0.0f);

    system.setTarget({-100.0f, 0.0f, 0.0f});
    const float previousAcceleration = system.getAgents()[0].acceleration.x;
    system.update(1.0f / 60.0f);
    EXPECT_FLOAT_EQ(system.getAgents()[0].acceleration.x, previousAcceleration)
        << "LOD1 should carry the previous force for one skipped frame";

    system.update(1.0f / 60.0f);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "LOD1 should recompute its force on the following frame";
}

TEST(BoidsDistanceLodContractTest,
     FirstThresholdEqualityRecomputesForcesOnEveryFrame)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.setCameraPosition({-300.0f, 0.0f, 0.0f});
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});

    system.update(1.0f / 60.0f);
    ASSERT_EQ(system.getAgents().size(), 1u);
    ASSERT_GT(system.getAgents()[0].acceleration.x, 0.0f);

    system.setTarget({-100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "the first distance threshold is inclusive for Full LOD, so its "
           "force must refresh immediately on the next frame";
}

TEST(BoidsDistanceLodContractTest, EnteringFarLodRecomputesBeforeStartingItsSkipWindow)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});

    system.setCameraPosition({-1000.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    const float farAcceleration = system.getAgents()[0].acceleration.x;
    ASSERT_GT(farAcceleration, 0.0f);

    system.setCameraPosition({-100.0f, 0.0f, 0.0f});
    system.setTarget({-100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    ASSERT_LT(system.getAgents()[0].acceleration.x, 0.0f);
    system.setTarget({100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    ASSERT_GT(system.getAgents()[0].acceleration.x, 0.0f);

    system.setCameraPosition({-1000.0f, 0.0f, 0.0f});
    system.setTarget({-100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "A new far-LOD interval should start with a force refresh, not a stale skip counter";
}

TEST(BoidsDistanceLodContractTest, ReturningToFarLodAfterNearRefreshDoesNotReuseOldSkipWindow)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});

    system.setCameraPosition({-1000.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    ASSERT_GT(system.getAgents()[0].acceleration.x, 0.0f);

    // Force one near-LOD recomputation in the opposite direction, then return
    // to the distant LOD before its previous skip window expires.
    system.setCameraPosition({-100.0f, 0.0f, 0.0f});
    system.setTarget({-100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    ASSERT_LT(system.getAgents()[0].acceleration.x, 0.0f);

    system.setCameraPosition({-1000.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    EXPECT_GT(system.getAgents()[0].acceleration.x, 0.0f)
        << "re-entering a distant LOD should refresh forces before skipping";
}

TEST(BoidsDistanceLodContractTest,
     ReturningFromFarLodRecomputesForcesImmediatelyAtNearDistance)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});

    system.setCameraPosition({-1000.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    const float farAcceleration = system.getAgents()[0].acceleration.x;
    ASSERT_GT(farAcceleration, 0.0f);

    system.setTarget({-100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    EXPECT_FLOAT_EQ(system.getAgents()[0].acceleration.x, farAcceleration)
        << "the Far LOD should not require an update on every intervening frame";

    system.setCameraPosition({-100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "leaving Far LOD should recompute a changed force on the first Near frame";
}

TEST(BoidsDistanceLodContractTest,
     ReturningFromMediumLodRecomputesForcesDuringItsSkippedFrame)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 1;
    system.maxAgentCount = 1;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.setCameraPosition({-400.0f, 0.0f, 0.0f});
    system.addAgent({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.setTarget({100.0f, 0.0f, 0.0f});

    system.update(1.0f / 60.0f);
    ASSERT_EQ(system.getAgents().size(), 1u);
    ASSERT_GT(system.getAgents()[0].acceleration.x, 0.0f);

    system.setTarget({-100.0f, 0.0f, 0.0f});
    system.setCameraPosition({-100.0f, 0.0f, 0.0f});
    system.update(1.0f / 60.0f);
    EXPECT_LT(system.getAgents()[0].acceleration.x, 0.0f)
        << "returning to Full LOD should refresh forces on the frame Medium "
           "would otherwise skip";
}

TEST(BoidsDistanceLodContractTest, PopulationBudgetRemovesAgentsAboveMaximum)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 0;
    system.maxAgentCount = 3;
    system.initializeRandom(5, {20.0f, 20.0f, 20.0f}, 17);

    system.update(1.0f / 60.0f);

    EXPECT_EQ(system.getAgents().size(), 3u);
    EXPECT_EQ(system.getTrails().size(), system.getAgents().size());

    // Run again after automatic removals so stale per-agent LOD state is used.
    system.update(1.0f / 60.0f);
    EXPECT_EQ(system.getAgents().size(), 3u);
    EXPECT_EQ(system.getTrails().size(), system.getAgents().size());
}

TEST(BoidsDistanceLodContractTest,
     PopulationBudgetRetainsAgentsNearestToTheActiveCamera)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 0;
    system.maxAgentCount = 2;
    system.separationWeight = 0.0f;
    system.alignmentWeight = 0.0f;
    system.cohesionWeight = 0.0f;
    system.wanderWeight = 0.0f;
    system.obstacleWeight = 0.0f;
    system.setCameraPosition({10.0f, 0.0f, 0.0f});
    system.addAgent({11.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.addAgent({12.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    system.addAgent({100.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});

    system.update(1.0f / 60.0f);

    ASSERT_EQ(system.getAgents().size(), 2u);
    EXPECT_FLOAT_EQ(system.getAgents()[0].position.x, 11.0f);
    EXPECT_FLOAT_EQ(system.getAgents()[1].position.x, 12.0f)
        << "the population budget should discard the agent farthest from the camera";
}

TEST(BoidsDistanceLodContractTest, PopulationBudgetAddsAgentsBelowMinimum)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 4;
    system.maxAgentCount = 8;
    system.initializeRandom(2, {20.0f, 20.0f, 20.0f}, 23);

    system.update(1.0f / 60.0f);

    EXPECT_EQ(system.getAgents().size(), 4u);
    EXPECT_EQ(system.getTrails().size(), system.getAgents().size());

    // Newly added agents must have matching Trail and LOD bookkeeping.
    system.update(1.0f / 60.0f);
    EXPECT_EQ(system.getAgents().size(), 4u);
    EXPECT_EQ(system.getTrails().size(), system.getAgents().size());
}

TEST(BoidsDistanceLodContractTest, PopulationInsideBudgetRemainsUnchanged)
{
    BoidsSwarmSystem system;
    system.minAgentCount = 2;
    system.maxAgentCount = 5;
    system.initializeRandom(3, {20.0f, 20.0f, 20.0f}, 29);

    system.update(1.0f / 60.0f);

    EXPECT_EQ(system.getAgents().size(), 3u);
    EXPECT_EQ(system.getTrails().size(), system.getAgents().size());
}
