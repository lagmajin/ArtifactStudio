#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

import Container.Debug;

using namespace ArtifactCore;

TEST(ContainerDebugStandaloneTest, DefaultContainerNameIsEmpty)
{
    const ContainerName name;
    EXPECT_TRUE(name.isEmpty());
}

TEST(ContainerDebugStandaloneTest, NullContainerNameIsNormalizedToEmpty)
{
    const ContainerName name(nullptr);
    EXPECT_TRUE(name.isEmpty());
    EXPECT_STREQ(name.value, "");
}

TEST(ContainerDebugStandaloneTest, NamedContainerHasStableName)
{
    constexpr auto name = namedContainer("render-cache");
    EXPECT_FALSE(name.isEmpty());
    EXPECT_STREQ(name.value, "render-cache");
}

TEST(ContainerDebugStandaloneTest, SourceLocationAndOwnerHelpersPreserveInputs)
{
    constexpr auto location = containerSourceLocation("unit.cpp", "test", 23);
    constexpr auto owner = containerOwner("RenderGraph", "graph-4");
    EXPECT_STREQ(location.file, "unit.cpp");
    EXPECT_STREQ(location.function, "test");
    EXPECT_EQ(location.line, 23);
    EXPECT_STREQ(owner.name, "RenderGraph");
    EXPECT_STREQ(owner.id, "graph-4");
}

TEST(ContainerDebugStandaloneTest, ContainerHereHelperCreatesSourceLocation)
{
    const auto location = containerHere("source.cpp", "call", 7);
    EXPECT_STREQ(location.file, "source.cpp");
    EXPECT_STREQ(location.function, "call");
    EXPECT_EQ(location.line, 7);
}

TEST(ContainerDebugStandaloneTest, ContainerOwnerHelperPreservesFields)
{
    const auto owner = containerOwner("Owner", "id");
    EXPECT_STREQ(owner.name, "Owner");
    EXPECT_STREQ(owner.id, "id");
}

TEST(ContainerDebugStandaloneTest, ContainerDomainsHaveStableNames)
{
    EXPECT_STREQ(toString(ContainerDomain::Unknown), "Unknown");
    EXPECT_STREQ(toString(ContainerDomain::Timeline), "Timeline");
    EXPECT_STREQ(toString(ContainerDomain::Render), "Render");
    EXPECT_STREQ(toString(ContainerDomain::Selection), "Selection");
    EXPECT_STREQ(toString(ContainerDomain::Asset), "Asset");
    EXPECT_STREQ(toString(ContainerDomain::Cache), "Cache");
    EXPECT_STREQ(toString(ContainerDomain::Diagnostics), "Diagnostics");
    EXPECT_STREQ(toString(static_cast<ContainerDomain>(255)), "Unknown");
}

TEST(ContainerDebugStandaloneTest, NoteSeverityNamesAndValidationCoverAllValues)
{
    EXPECT_STREQ(toString(ContainerDebugNoteSeverity::Info), "info");
    EXPECT_STREQ(toString(ContainerDebugNoteSeverity::Warning), "warning");
    EXPECT_STREQ(toString(ContainerDebugNoteSeverity::Error), "error");
    EXPECT_STREQ(toString(ContainerDebugNoteSeverity::Hypothesis), "hypothesis");
    EXPECT_TRUE(isValidContainerDebugNoteSeverity(ContainerDebugNoteSeverity::Info));
    EXPECT_TRUE(isValidContainerDebugNoteSeverity(ContainerDebugNoteSeverity::Hypothesis));
    EXPECT_FALSE(isValidContainerDebugNoteSeverity(
        static_cast<ContainerDebugNoteSeverity>(255)));
    EXPECT_STREQ(toString(static_cast<ContainerDebugNoteSeverity>(255)), "info");
}

TEST(ContainerDebugStandaloneTest, NoteAuthorNamesAndValidationCoverAllValues)
{
    EXPECT_STREQ(toString(ContainerDebugNoteAuthor::Runtime), "runtime");
    EXPECT_STREQ(toString(ContainerDebugNoteAuthor::Developer), "developer");
    EXPECT_STREQ(toString(ContainerDebugNoteAuthor::AI), "ai");
    EXPECT_TRUE(isValidContainerDebugNoteAuthor(ContainerDebugNoteAuthor::Runtime));
    EXPECT_TRUE(isValidContainerDebugNoteAuthor(ContainerDebugNoteAuthor::AI));
    EXPECT_FALSE(isValidContainerDebugNoteAuthor(static_cast<ContainerDebugNoteAuthor>(255)));
    EXPECT_STREQ(toString(static_cast<ContainerDebugNoteAuthor>(255)), "runtime");
}

TEST(ContainerDebugStandaloneTest, EmptyWatchRuleUsesGenericWatchName)
{
    const ContainerWatchRule rule;
    EXPECT_STREQ(toString(rule), "watch");
}

TEST(ContainerDebugStandaloneTest, WatchRuleNamePrioritizesMutationThenFailedAccess)
{
    ContainerWatchRule rule;
    rule.watchFailedAccess = true;
    EXPECT_STREQ(toString(rule), "watchFailedAccess");
    rule.watchMutation = true;
    EXPECT_STREQ(toString(rule), "watchMutation");
}

TEST(ContainerDebugStandaloneTest, DebugInfoEmptinessAndNameReflectCountAndName)
{
    ContainerDebugInfo info;
    EXPECT_TRUE(info.isEmpty());
    EXPECT_FALSE(info.hasName());
    info.name = ContainerName{"items"};
    info.count = 1;
    EXPECT_FALSE(info.isEmpty());
    EXPECT_TRUE(info.hasName());
}

TEST(ContainerDebugStandaloneTest, DebugSnapshotEmptinessAndSamplesFollowContents)
{
    ContainerDebugSnapshot snapshot;
    EXPECT_TRUE(snapshot.isEmpty());
    EXPECT_FALSE(snapshot.hasSamples());
    snapshot.info.count = 2;
    snapshot.samples.push_back(ContainerElementSample{0, nullptr, "first"});
    EXPECT_FALSE(snapshot.isEmpty());
    EXPECT_TRUE(snapshot.hasSamples());
}

TEST(ContainerDebugStandaloneTest, ValueCheckpointOwnsCapturedValuesAndMetadata)
{
    ContainerDebugValueCheckpoint<int> checkpoint;
    const int source = 5;
    checkpoint.source = &source;
    checkpoint.capturedAtMilliseconds = 100;
    checkpoint.version = 4;
    checkpoint.failedAccessCount = 2;
    checkpoint.values = {3, 5, 8};

    EXPECT_EQ(checkpoint.source, &source);
    EXPECT_EQ(checkpoint.capturedAtMilliseconds, 100u);
    EXPECT_EQ(checkpoint.version, 4u);
    EXPECT_EQ(checkpoint.failedAccessCount, 2u);
    EXPECT_EQ(checkpoint.values, (std::vector<int>{3, 5, 8}));
}

TEST(ContainerDebugStandaloneTest, VerificationDefaultsToUnmatchedAndUnchangedFalse)
{
    const ContainerDebugVerification verification;
    EXPECT_FALSE(verification.sourceMatches);
    EXPECT_FALSE(verification.unchangedSinceCheckpoint);
    EXPECT_EQ(verification.expectedVersion, 0u);
    EXPECT_EQ(verification.actualCount, 0u);
}

TEST(ContainerDebugStandaloneTest, DebugCountersKeepIndependentMeasurements)
{
    ContainerDebugCounters counters;
    counters.version = 5;
    counters.mutationCount = 3;
    counters.readCount = 9;
    counters.failedAccessCount = 1;
    counters.addedCount = 4;
    counters.removedCount = 2;
    counters.capacityChangeCount = 1;

    EXPECT_EQ(counters.version, 5u);
    EXPECT_EQ(counters.mutationCount, 3u);
    EXPECT_EQ(counters.readCount, 9u);
    EXPECT_EQ(counters.failedAccessCount, 1u);
    EXPECT_EQ(counters.addedCount, 4u);
    EXPECT_EQ(counters.removedCount, 2u);
    EXPECT_EQ(counters.capacityChangeCount, 1u);
}

TEST(ContainerDebugStandaloneTest, ContainerDebugNowMillisecondsReturnsEpochMilliseconds)
{
    const auto now = containerDebugNowMilliseconds();
    EXPECT_GT(now, 0u);
}
