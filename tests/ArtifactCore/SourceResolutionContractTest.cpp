#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTemporaryDir>

import Asset.Manager;

using namespace ArtifactCore;

namespace {

bool writeTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write("artifact\n") > 0;
}

}

TEST(SourceResolutionContractTest, AdoptsExistingRelativeCandidate)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir(dir.path()).mkpath(QStringLiteral("assets")));
    const QString stored = dir.filePath(QStringLiteral("assets/shot.png"));
    ASSERT_TRUE(writeTextFile(stored));

    const auto resolution = resolveProjectRelativeSource(
        dir.path(),
        SourceResolutionCandidateKind::ProjectRelativePath,
        QStringLiteral("D:/elsewhere/shot.png"),
        QStringLiteral("assets/shot.png"),
        true);

    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::AdoptedExistingCandidate);
    EXPECT_EQ(resolution.resolvedPath, QDir::cleanPath(stored));
}

TEST(SourceResolutionContractTest, MissingCandidateKeepsStoredPath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const auto strict = resolveProjectRelativeSource(
        dir.path(),
        SourceResolutionCandidateKind::ProjectRelativePath,
        QStringLiteral("D:/elsewhere/shot.png"),
        QStringLiteral("assets/missing.png"),
        false);
    EXPECT_FALSE(strict.adopted);
    EXPECT_EQ(strict.outcome, SourceCandidateOutcome::KeptOriginalMissingCandidate);
    EXPECT_EQ(strict.resolvedPath, QStringLiteral("D:/elsewhere/shot.png"));

    const auto lenient = resolveProjectRelativeSource(
        dir.path(),
        SourceResolutionCandidateKind::ProjectRelativePath,
        QStringLiteral("D:/elsewhere/shot.png"),
        QStringLiteral("assets/missing.png"),
        true);
    EXPECT_FALSE(lenient.adopted);
    EXPECT_EQ(lenient.outcome, SourceCandidateOutcome::KeptOriginalMissingCandidate);
    EXPECT_EQ(lenient.resolvedPath, QStringLiteral("D:/elsewhere/shot.png"));
}

TEST(SourceResolutionContractTest, MissingCandidateAdoptedForEmptyStoredPath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const auto resolution = resolveProjectRelativeSource(
        dir.path(),
        SourceResolutionCandidateKind::RegistryRelativePath,
        QString(),
        QStringLiteral("assets/recovered.png"),
        true);

    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome,
              SourceCandidateOutcome::AdoptedCandidateForEmptyOriginal);
    EXPECT_EQ(resolution.resolvedPath,
              QDir::cleanPath(dir.filePath(QStringLiteral("assets/recovered.png"))));
}

TEST(SourceResolutionContractTest, EmptyRelativeCandidateKeepsStoredPath)
{
    const auto resolution = resolveProjectRelativeSource(
        QStringLiteral("C:/project"),
        SourceResolutionCandidateKind::AbsolutePathFallback,
        QStringLiteral("D:/keep/me.png"),
        QStringLiteral("   "),
        true);

    EXPECT_FALSE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::KeptOriginalEmptyCandidate);
    EXPECT_EQ(resolution.resolvedPath, QStringLiteral("D:/keep/me.png"));
}

TEST(SourceResolutionContractTest, SequenceEntryPolicyKeepsMissingFrameSlot)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const auto resolution = resolveProjectRelativeSource(
        dir.path(),
        SourceResolutionCandidateKind::ProjectRelativePath,
        QString(),
        QStringLiteral("seq/frame_0007.png"),
        false);

    EXPECT_FALSE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::KeptOriginalMissingCandidate);
    EXPECT_TRUE(resolution.resolvedPath.isEmpty());
}

TEST(SourceResolutionContractTest, ProjectRelativeCandidateRoundTrip)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir(dir.path()).mkpath(QStringLiteral("assets")));
    const QString absolute = dir.filePath(QStringLiteral("assets/take2.png"));
    ASSERT_TRUE(writeTextFile(absolute));

    const QString relative =
        projectRelativeSourceCandidate(dir.path(), absolute);
    EXPECT_FALSE(relative.isEmpty());

    const auto resolution = resolveProjectRelativeSource(
        dir.path(),
        SourceResolutionCandidateKind::RegistryRelativePath,
        absolute,
        relative,
        true);

    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::AdoptedExistingCandidate);
    EXPECT_EQ(resolution.resolvedPath, QDir::cleanPath(absolute));
}

TEST(SourceResolutionContractTest, ProjectRelativeCandidateHandlesEmptyInput)
{
    EXPECT_TRUE(projectRelativeSourceCandidate(
                    QStringLiteral("C:/project"), QStringLiteral("   "))
                    .isEmpty());
}
