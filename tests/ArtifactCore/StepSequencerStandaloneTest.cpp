#include <gtest/gtest.h>

#include <cstdint>

import Sequence.StepSequencer;

using ArtifactCore::StepSequencer;
using ArtifactCore::StepSequencerStep;

TEST(StepSequencerStandaloneTest, ConstructorClampsStepCountToSupportedRange)
{
  const StepSequencer emptyCount{0};
  const StepSequencer negativeCount{-7};
  const StepSequencer oversizedCount{5000};
  EXPECT_EQ(emptyCount.stepCount(), 1);
  EXPECT_EQ(emptyCount.loopStart(), 0);
  EXPECT_EQ(emptyCount.loopEnd(), 0);
  EXPECT_EQ(negativeCount.stepCount(), 1);
  EXPECT_EQ(oversizedCount.stepCount(), 4096);
  EXPECT_EQ(oversizedCount.loopEnd(), 4095);
}

TEST(StepSequencerStandaloneTest, ResizingPreservesExistingStepsAndDefaultInitializesNewOnes)
{
  StepSequencer sequencer{3};
  sequencer.setStep(1, StepSequencerStep{0.75, 0.5, 0.25, true});

  sequencer.setStepCount(5);

  ASSERT_EQ(sequencer.stepCount(), 5);
  EXPECT_DOUBLE_EQ(sequencer.step(1).value, 0.75);
  EXPECT_DOUBLE_EQ(sequencer.step(1).velocity, 0.5);
  EXPECT_FALSE(sequencer.step(3).enabled);
  EXPECT_DOUBLE_EQ(sequencer.step(3).gate, 1.0);
}

TEST(StepSequencerStandaloneTest, StepAccessClampsIndicesToAvailableSteps)
{
  StepSequencer sequencer{3};
  sequencer.setStep(2, StepSequencerStep{0.4, 0.8, 0.6, true});

  EXPECT_DOUBLE_EQ(sequencer.step(-1).value, 0.0);
  EXPECT_DOUBLE_EQ(sequencer.step(100).value, 0.4);
  EXPECT_TRUE(sequencer.step(100).enabled);
}

TEST(StepSequencerStandaloneTest, LoopBoundsClampAndCurrentStepTracksTheLoop)
{
  StepSequencer sequencer{8};
  sequencer.setCurrentStep(6);
  sequencer.setLoop(2, 4);

  EXPECT_EQ(sequencer.loopStart(), 2);
  EXPECT_EQ(sequencer.loopEnd(), 4);
  EXPECT_EQ(sequencer.currentStep(), 4);

  sequencer.setLoop(-5, 100);
  EXPECT_EQ(sequencer.loopStart(), 0);
  EXPECT_EQ(sequencer.loopEnd(), 7);
}

TEST(StepSequencerStandaloneTest, AdvanceWrapsForPositiveAndNegativeAmounts)
{
  StepSequencer sequencer{8};
  sequencer.setLoop(2, 5);
  sequencer.setCurrentStep(5);

  EXPECT_EQ(sequencer.advance().value, 0.0);
  EXPECT_EQ(sequencer.currentStep(), 2);
  EXPECT_EQ(sequencer.advance(-1).value, 0.0);
  EXPECT_EQ(sequencer.currentStep(), 5);
  sequencer.advance(10);
  EXPECT_EQ(sequencer.currentStep(), 3);
}

TEST(StepSequencerStandaloneTest, ClearLoopRestoresWholeStepRange)
{
  StepSequencer sequencer{6};
  sequencer.setLoop(2, 3);
  sequencer.clearLoop();

  EXPECT_EQ(sequencer.loopStart(), 0);
  EXPECT_EQ(sequencer.loopEnd(), 5);
}

TEST(StepSequencerStandaloneTest, ShrinkingStepsClampsLoopAndCurrentPosition)
{
  StepSequencer sequencer{8};
  sequencer.setLoop(4, 7);
  sequencer.setCurrentStep(6);

  sequencer.setStepCount(3);

  EXPECT_EQ(sequencer.loopStart(), 2);
  EXPECT_EQ(sequencer.loopEnd(), 2);
  EXPECT_EQ(sequencer.currentStep(), 2);
}

TEST(StepSequencerStandaloneTest, StartStopAndStateReportTransportFields)
{
  StepSequencer sequencer{4};
  sequencer.setLoop(1, 3);
  sequencer.setCurrentStep(2);
  sequencer.start();

  const auto playing = sequencer.state();
  EXPECT_TRUE(sequencer.isPlaying());
  EXPECT_EQ(playing.currentStep, 2);
  EXPECT_EQ(playing.stepCount, 4);
  EXPECT_EQ(playing.loopStart, 1);
  EXPECT_EQ(playing.loopEnd, 3);
  EXPECT_TRUE(playing.playing);

  sequencer.stop();
  EXPECT_FALSE(sequencer.isPlaying());
  EXPECT_FALSE(sequencer.state().playing);
}

TEST(StepSequencerStandaloneTest, TriggerReturnsCurrentStepWithoutAdvancing)
{
  StepSequencer sequencer{4};
  sequencer.setStep(2, StepSequencerStep{0.3, 0.9, 0.7, true});
  sequencer.setCurrentStep(2);

  const auto& triggered = sequencer.trigger();

  EXPECT_EQ(sequencer.currentStep(), 2);
  EXPECT_DOUBLE_EQ(triggered.value, 0.3);
  EXPECT_DOUBLE_EQ(triggered.velocity, 0.9);
  EXPECT_DOUBLE_EQ(triggered.gate, 0.7);
  EXPECT_TRUE(triggered.enabled);
}

TEST(StepSequencerStandaloneTest, AdvanceAmountHandlesLargePositiveAndNegativeOffsets)
{
  StepSequencer sequencer{7};
  sequencer.setLoop(1, 5);
  sequencer.setCurrentStep(4);

  sequencer.advance(INT32_MAX);
  EXPECT_EQ(sequencer.currentStep(), 1);

  sequencer.advance(INT32_MIN);
  EXPECT_EQ(sequencer.currentStep(), 3);
}
