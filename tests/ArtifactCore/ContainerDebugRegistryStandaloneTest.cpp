#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

import Container.Debug;
import Container.Debug.Registry;

using ArtifactCore::ContainerDebugNoteAuthor;
using ArtifactCore::ContainerDebugNoteSeverity;
using ArtifactCore::ContainerDebugRegistry;
using ArtifactCore::ContainerDebugSnapshot;
using ArtifactCore::ContainerDomain;
using ArtifactCore::ContainerName;
using ArtifactCore::ContainerOwner;
using ArtifactCore::ContainerSourceLocation;
using ArtifactCore::registerContainerDebugSnapshot;

namespace {

auto sampleSnapshot(std::size_t count = 3) -> ContainerDebugSnapshot
{
    ContainerDebugSnapshot snapshot;
    snapshot.info.name = ContainerName{"sample"};
    snapshot.info.domain = ContainerDomain::Diagnostics;
    snapshot.info.owner = ContainerOwner{"test-owner", "owner-1"};
    snapshot.info.count = count;
    snapshot.counters.version = 7;
    snapshot.createdAt = ContainerSourceLocation{"sample.cpp", "sampleSnapshot", 12};
    return snapshot;
}

struct SnapshotContainer {
    ContainerDebugSnapshot value;
    auto debugSnapshot() const -> ContainerDebugSnapshot { return value; }
};

} // namespace

TEST(ContainerDebugRegistryStandaloneTest, RejectsEmptyRegistrationId)
{
    ContainerDebugRegistry registry;
    EXPECT_FALSE(registry.registerReader("", [] { return sampleSnapshot(); }));
    EXPECT_TRUE(registry.registeredIds().empty());
}

TEST(ContainerDebugRegistryStandaloneTest, RejectsEmptyReader)
{
    ContainerDebugRegistry registry;
    EXPECT_FALSE(registry.registerReader("container-1", {}));
    EXPECT_FALSE(registry.contains("container-1"));
}

TEST(ContainerDebugRegistryStandaloneTest, RejectsDuplicateRegistrationId)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(1); }));
    EXPECT_FALSE(registry.registerReader("container-1", [] { return sampleSnapshot(2); }));
    EXPECT_EQ(registry.registeredIds().size(), 1u);
}

TEST(ContainerDebugRegistryStandaloneTest, InspectCopiesReaderSnapshot)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(9); }));
    ContainerDebugSnapshot result;
    ASSERT_TRUE(registry.inspect("container-1", result));
    EXPECT_EQ(result.info.count, 9u);
    EXPECT_EQ(result.counters.version, 7u);
    EXPECT_EQ(result.info.domain, ContainerDomain::Diagnostics);
    EXPECT_STREQ(result.createdAt.file, "sample.cpp");
}

TEST(ContainerDebugRegistryStandaloneTest, InspectInvokesReaderOnEveryRequest)
{
    ContainerDebugRegistry registry;
    std::size_t count = 0;
    ASSERT_TRUE(registry.registerReader("changing", [&] {
        return sampleSnapshot(++count);
    }));
    ContainerDebugSnapshot first;
    ContainerDebugSnapshot second;
    ASSERT_TRUE(registry.inspect("changing", first));
    ASSERT_TRUE(registry.inspect("changing", second));
    EXPECT_EQ(first.info.count, 1u);
    EXPECT_EQ(second.info.count, 2u);
}

TEST(ContainerDebugRegistryStandaloneTest, InspectUnknownIdReturnsFalseWithoutChangingOutput)
{
    ContainerDebugRegistry registry;
    ContainerDebugSnapshot result = sampleSnapshot(41);
    EXPECT_FALSE(registry.inspect("missing", result));
    EXPECT_EQ(result.info.count, 41u);
}

TEST(ContainerDebugRegistryStandaloneTest, UnregisterRemovesReaderAndIsNotRepeatable)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(); }));
    EXPECT_TRUE(registry.unregisterReader("container-1"));
    EXPECT_FALSE(registry.contains("container-1"));
    EXPECT_FALSE(registry.unregisterReader("container-1"));
}

TEST(ContainerDebugRegistryStandaloneTest, RegisteredIdsPreserveRegistrationOrder)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("first", [] { return sampleSnapshot(); }));
    ASSERT_TRUE(registry.registerReader("second", [] { return sampleSnapshot(); }));
    ASSERT_TRUE(registry.registerReader("third", [] { return sampleSnapshot(); }));
    EXPECT_EQ(registry.registeredIds(), (std::vector<std::string>{"first", "second", "third"}));
}

TEST(ContainerDebugRegistryStandaloneTest, AnnotateForwardsTextSeverityAndAuthor)
{
    ContainerDebugRegistry registry;
    std::string capturedText;
    ContainerDebugNoteSeverity capturedSeverity = ContainerDebugNoteSeverity::Info;
    ContainerDebugNoteAuthor capturedAuthor = ContainerDebugNoteAuthor::Runtime;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(); },
        [&](std::string text, ContainerDebugNoteSeverity severity,
            ContainerDebugNoteAuthor author) {
            capturedText = std::move(text);
            capturedSeverity = severity;
            capturedAuthor = author;
            return true;
        }));

    EXPECT_TRUE(registry.annotate("container-1", "inspect capacity",
                                  ContainerDebugNoteSeverity::Warning,
                                  ContainerDebugNoteAuthor::Developer));
    EXPECT_EQ(capturedText, "inspect capacity");
    EXPECT_EQ(capturedSeverity, ContainerDebugNoteSeverity::Warning);
    EXPECT_EQ(capturedAuthor, ContainerDebugNoteAuthor::Developer);
}

TEST(ContainerDebugRegistryStandaloneTest, AnnotateReturnsFalseWhenReaderHasNoNoteWriter)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(); }));
    EXPECT_FALSE(registry.annotate("container-1", "note",
                                   ContainerDebugNoteSeverity::Info,
                                   ContainerDebugNoteAuthor::AI));
}

TEST(ContainerDebugRegistryStandaloneTest, AnnotateUnknownIdReturnsFalse)
{
    ContainerDebugRegistry registry;
    EXPECT_FALSE(registry.annotate("missing", "note",
                                   ContainerDebugNoteSeverity::Info,
                                   ContainerDebugNoteAuthor::Runtime));
}

TEST(ContainerDebugRegistryStandaloneTest, AnnotateRejectsEmptyTextAndOversizedText)
{
    ContainerDebugRegistry registry;
    int writes = 0;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(); },
        [&](std::string, ContainerDebugNoteSeverity, ContainerDebugNoteAuthor) {
            ++writes;
            return true;
        }));
    EXPECT_FALSE(registry.annotate("container-1", "", ContainerDebugNoteSeverity::Info,
                                   ContainerDebugNoteAuthor::Runtime));
    EXPECT_FALSE(registry.annotate("container-1", std::string(1025, 'x'),
                                   ContainerDebugNoteSeverity::Info,
                                   ContainerDebugNoteAuthor::Runtime));
    EXPECT_EQ(writes, 0);
}

TEST(ContainerDebugRegistryStandaloneTest, AnnotateAcceptsMaximumAllowedTextLength)
{
    ContainerDebugRegistry registry;
    std::size_t capturedLength = 0;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(); },
        [&](std::string text, ContainerDebugNoteSeverity, ContainerDebugNoteAuthor) {
            capturedLength = text.size();
            return true;
        }));

    EXPECT_TRUE(registry.annotate("container-1", std::string(1024, 'n'),
                                  ContainerDebugNoteSeverity::Info,
                                  ContainerDebugNoteAuthor::Runtime));
    EXPECT_EQ(capturedLength, 1024u);
}

TEST(ContainerDebugRegistryStandaloneTest, AnnotateRejectsUnknownSeverityAndAuthor)
{
    ContainerDebugRegistry registry;
    int writes = 0;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(); },
        [&](std::string, ContainerDebugNoteSeverity, ContainerDebugNoteAuthor) {
            ++writes;
            return true;
        }));
    EXPECT_FALSE(registry.annotate("container-1", "note",
        static_cast<ContainerDebugNoteSeverity>(255), ContainerDebugNoteAuthor::Runtime));
    EXPECT_FALSE(registry.annotate("container-1", "note", ContainerDebugNoteSeverity::Info,
        static_cast<ContainerDebugNoteAuthor>(255)));
    EXPECT_EQ(writes, 0);
}

TEST(ContainerDebugRegistryStandaloneTest, AnnotatePropagatesWriterFailure)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(); },
        [](std::string, ContainerDebugNoteSeverity, ContainerDebugNoteAuthor) { return false; }));
    EXPECT_FALSE(registry.annotate("container-1", "rejected",
                                   ContainerDebugNoteSeverity::Error,
                                   ContainerDebugNoteAuthor::AI));
}

TEST(ContainerDebugRegistryStandaloneTest, AnnotateCatchesWriterException)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("container-1", [] { return sampleSnapshot(); },
        [](std::string, ContainerDebugNoteSeverity, ContainerDebugNoteAuthor) -> bool {
            throw std::runtime_error("writer failure");
        }));
    EXPECT_FALSE(registry.annotate("container-1", "note",
                                   ContainerDebugNoteSeverity::Error,
                                   ContainerDebugNoteAuthor::AI));
    EXPECT_TRUE(registry.contains("container-1"));
    EXPECT_TRUE(registry.unregisterReader("container-1"));
}

TEST(ContainerDebugRegistryStandaloneTest, InspectCatchesReaderExceptionAndKeepsRegistration)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("container-1", []() -> ContainerDebugSnapshot {
        throw std::runtime_error("reader failure");
    }));
    ContainerDebugSnapshot result;
    EXPECT_FALSE(registry.inspect("container-1", result));
    EXPECT_TRUE(registry.contains("container-1"));
    EXPECT_TRUE(registry.unregisterReader("container-1"));
}

TEST(ContainerDebugRegistryStandaloneTest, ReaderCanQueryRegistryDuringInspection)
{
    ContainerDebugRegistry registry;
    bool observedRegistered = false;
    ASSERT_TRUE(registry.registerReader("query-reader", [&] {
        observedRegistered = registry.contains("query-reader");
        return sampleSnapshot();
    }));
    ContainerDebugSnapshot snapshot;
    EXPECT_TRUE(registry.inspect("query-reader", snapshot));
    EXPECT_TRUE(observedRegistered);
}

TEST(ContainerDebugRegistryStandaloneTest, ScopedReaderUnregistersWhenRegistrationIsDestroyed)
{
    ContainerDebugRegistry registry;
    {
        auto registration = registry.registerScopedReader(
            "scoped", [] { return sampleSnapshot(); });
        ASSERT_TRUE(static_cast<bool>(registration));
        EXPECT_TRUE(registry.contains("scoped"));
    }
    EXPECT_FALSE(registry.contains("scoped"));
}

TEST(ContainerDebugRegistryStandaloneTest, FailedScopedRegistrationDoesNotRemoveExistingReader)
{
    ContainerDebugRegistry registry;
    ASSERT_TRUE(registry.registerReader("shared", [] { return sampleSnapshot(4); }));
    {
        auto duplicate = registry.registerScopedReader(
            "shared", [] { return sampleSnapshot(8); });
        EXPECT_FALSE(static_cast<bool>(duplicate));
    }
    ContainerDebugSnapshot snapshot;
    ASSERT_TRUE(registry.inspect("shared", snapshot));
    EXPECT_EQ(snapshot.info.count, 4u);
}

TEST(ContainerDebugRegistryStandaloneTest, MovingRegistrationTransfersUnregisterResponsibility)
{
    ContainerDebugRegistry registry;
    auto original = registry.registerScopedReader("move", [] { return sampleSnapshot(); });
    ASSERT_TRUE(static_cast<bool>(original));
    auto moved = std::move(original);
    EXPECT_FALSE(static_cast<bool>(original));
    EXPECT_TRUE(static_cast<bool>(moved));
    moved.reset();
    EXPECT_FALSE(registry.contains("move"));
}

TEST(ContainerDebugRegistryStandaloneTest, MoveAssignmentReleasesPreviousRegistration)
{
    ContainerDebugRegistry registry;
    auto first = registry.registerScopedReader("first", [] { return sampleSnapshot(1); });
    auto second = registry.registerScopedReader("second", [] { return sampleSnapshot(2); });
    ASSERT_TRUE(static_cast<bool>(first));
    ASSERT_TRUE(static_cast<bool>(second));

    first = std::move(second);
    EXPECT_FALSE(registry.contains("first"));
    EXPECT_TRUE(registry.contains("second"));
    EXPECT_FALSE(static_cast<bool>(second));
    ContainerDebugSnapshot snapshot;
    ASSERT_TRUE(registry.inspect("second", snapshot));
    EXPECT_EQ(snapshot.info.count, 2u);
}

TEST(ContainerDebugRegistryStandaloneTest, RegistrationResetIsIdempotent)
{
    ContainerDebugRegistry registry;
    auto registration = registry.registerScopedReader("reset", [] { return sampleSnapshot(); });
    ASSERT_TRUE(static_cast<bool>(registration));
    registration.reset();
    registration.reset();
    EXPECT_FALSE(static_cast<bool>(registration));
    EXPECT_FALSE(registry.contains("reset"));
}

TEST(ContainerDebugRegistryStandaloneTest, RegistrationCanOutliveRegistrySafely)
{
    ContainerDebugRegistry::Registration registration;
    {
        ContainerDebugRegistry registry;
        registration = registry.registerScopedReader("short-lived-registry",
            [] { return sampleSnapshot(); });
        ASSERT_TRUE(static_cast<bool>(registration));
    }
    EXPECT_FALSE(static_cast<bool>(registration));
    registration.reset();
    EXPECT_FALSE(static_cast<bool>(registration));
}

TEST(ContainerDebugRegistryStandaloneTest, ContainerAdapterReadsDebugSnapshot)
{
    ContainerDebugRegistry registry;
    SnapshotContainer container{sampleSnapshot(17)};
    auto registration = registerContainerDebugSnapshot(registry, "adapter", container);
    ASSERT_TRUE(static_cast<bool>(registration));

    ContainerDebugSnapshot snapshot;
    ASSERT_TRUE(registry.inspect("adapter", snapshot));
    EXPECT_EQ(snapshot.info.count, 17u);
    EXPECT_STREQ(snapshot.info.name.value, "sample");
}

TEST(ContainerDebugRegistryStandaloneTest, ScopedRegistrationCanWriteNotes)
{
    ContainerDebugRegistry registry;
    std::string received;
    auto registration = registry.registerScoped("scoped-writer",
        [] { return sampleSnapshot(); },
        [&](std::string text, ContainerDebugNoteSeverity, ContainerDebugNoteAuthor) {
            received = std::move(text);
            return true;
        });
    ASSERT_TRUE(static_cast<bool>(registration));
    EXPECT_TRUE(registry.annotate("scoped-writer", "scoped note",
                                  ContainerDebugNoteSeverity::Hypothesis,
                                  ContainerDebugNoteAuthor::AI));
    EXPECT_EQ(received, "scoped note");
}
