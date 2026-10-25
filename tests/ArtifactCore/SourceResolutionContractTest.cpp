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

TEST(SourceResolutionContractTest, ExistingRelativeCandidateWinsOverExistingOriginal)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir(dir.path()).mkpath(QStringLiteral("assets")));
    ASSERT_TRUE(QDir(dir.path()).mkpath(QStringLiteral("old-location")));
    const QString projectCandidate = dir.filePath(QStringLiteral("assets/shot.png"));
    const QString original = dir.filePath(QStringLiteral("old-location/shot.png"));
    ASSERT_TRUE(writeTextFile(projectCandidate));
    ASSERT_TRUE(writeTextFile(original));

    const auto resolution = resolveProjectRelativeSource(
        dir.path(), SourceResolutionCandidateKind::ProjectRelativePath,
        original, QStringLiteral("assets/shot.png"), true);

    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::AdoptedExistingCandidate);
    EXPECT_EQ(resolution.resolvedPath, QDir::cleanPath(projectCandidate));
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

TEST(SourceResolutionContractTest, MissingCandidateAdoptedForWhitespaceStoredPath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString relativeCandidate = QStringLiteral("assets/recovered.png");
    ASSERT_FALSE(QFileInfo::exists(dir.filePath(relativeCandidate)));
    const QString whitespacePath = QStringLiteral(" \t\n ");

    const auto resolution = resolveProjectRelativeSource(
        dir.path(), SourceResolutionCandidateKind::ProjectRelativePath,
        whitespacePath, relativeCandidate, true);

    EXPECT_EQ(resolution.kind,
              SourceResolutionCandidateKind::ProjectRelativePath);
    EXPECT_EQ(resolution.originalPath, whitespacePath);
    EXPECT_EQ(resolution.candidatePath, dir.filePath(relativeCandidate));
    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome,
              SourceCandidateOutcome::AdoptedCandidateForEmptyOriginal);
    EXPECT_EQ(resolution.resolvedPath,
              QDir::cleanPath(dir.filePath(relativeCandidate)));
}

TEST(SourceResolutionContractTest, ExistingCandidateUsesExistingFileOutcomeForEmptyStoredPath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir(dir.path()).mkpath(QStringLiteral("assets")));
    const QString candidate = dir.filePath(QStringLiteral("assets/recovered.png"));
    ASSERT_TRUE(writeTextFile(candidate));

    const auto resolution = resolveProjectRelativeSource(
        dir.path(), SourceResolutionCandidateKind::RegistryRelativePath,
        QString(), QStringLiteral("assets/recovered.png"), true);

    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::AdoptedExistingCandidate);
    EXPECT_EQ(resolution.resolvedPath, QDir::cleanPath(candidate));
}

TEST(SourceResolutionContractTest, TrimsWhitespaceAroundExistingRelativeCandidate)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir(dir.path()).mkpath(QStringLiteral("assets")));
    const QString candidate = dir.filePath(QStringLiteral("assets/shot 01.png"));
    ASSERT_TRUE(writeTextFile(candidate));

    const auto resolution = resolveProjectRelativeSource(
        dir.path(), SourceResolutionCandidateKind::ProjectRelativePath,
        QStringLiteral("D:/stored/shot.png"),
        QStringLiteral("  assets/shot 01.png  "), false);

    EXPECT_EQ(resolution.kind,
              SourceResolutionCandidateKind::ProjectRelativePath);
    EXPECT_EQ(resolution.originalPath, QStringLiteral("D:/stored/shot.png"));
    EXPECT_EQ(resolution.candidatePath, candidate);
    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::AdoptedExistingCandidate);
    EXPECT_EQ(resolution.resolvedPath, QDir::cleanPath(candidate));
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

TEST(SourceResolutionContractTest, EmptyCandidateDoesNotAdoptForEmptyOriginal)
{
    const auto resolution = resolveProjectRelativeSource(
        QStringLiteral("C:/project"),
        SourceResolutionCandidateKind::RegistryRelativePath,
        QString(), QStringLiteral("  "), true);

    EXPECT_FALSE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::KeptOriginalEmptyCandidate);
    EXPECT_TRUE(resolution.candidatePath.isEmpty());
    EXPECT_TRUE(resolution.resolvedPath.isEmpty());
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

TEST(SourceResolutionContractTest, RelativeSourcePathIsResolvedAgainstCurrentDirectory)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString projectDirectory = dir.filePath(QStringLiteral("project"));
    const QString absoluteSource = dir.filePath(QStringLiteral("media/shot.png"));
    ASSERT_TRUE(QDir().mkpath(projectDirectory));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(absoluteSource).absolutePath()));
    ASSERT_TRUE(writeTextFile(absoluteSource));
    const QString relativeSource = QDir::current().relativeFilePath(absoluteSource);

    const QString candidate =
        projectRelativeSourceCandidate(projectDirectory, relativeSource);
    const QString expectedCandidate = QDir(projectDirectory).relativeFilePath(
        QFileInfo(absoluteSource).absoluteFilePath());

    EXPECT_EQ(candidate, expectedCandidate);
    const auto resolution = resolveProjectRelativeSource(
        projectDirectory, SourceResolutionCandidateKind::ProjectRelativePath,
        QString(), candidate, true);
    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::AdoptedExistingCandidate);
    EXPECT_EQ(resolution.resolvedPath, QDir::cleanPath(absoluteSource));
}

TEST(SourceResolutionContractTest, MissingRelativeSourceCandidateKeepsStoredPath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString projectDirectory = dir.filePath(QStringLiteral("project"));
    const QString missingSource = dir.filePath(QStringLiteral("media/missing.png"));
    ASSERT_TRUE(QDir().mkpath(projectDirectory));
    ASSERT_FALSE(QFileInfo::exists(missingSource));
    const QString relativeSource = QDir::current().relativeFilePath(missingSource);

    const QString relativeCandidate =
        projectRelativeSourceCandidate(projectDirectory, relativeSource);
    const QString expectedCandidate = QDir(projectDirectory).relativeFilePath(
        QFileInfo(missingSource).absoluteFilePath());
    EXPECT_EQ(relativeCandidate, expectedCandidate);

    const QString storedPath = QStringLiteral("D:/archived/original.png");
    const auto resolution = resolveProjectRelativeSource(
        projectDirectory, SourceResolutionCandidateKind::ProjectRelativePath,
        storedPath, relativeCandidate, true);

    EXPECT_EQ(resolution.candidatePath,
              QDir(projectDirectory).absoluteFilePath(expectedCandidate));
    EXPECT_FALSE(resolution.adopted);
    EXPECT_EQ(resolution.outcome,
              SourceCandidateOutcome::KeptOriginalMissingCandidate);
    EXPECT_EQ(resolution.resolvedPath, storedPath);
}

TEST(SourceResolutionContractTest, ProjectRelativePathsPreserveUnicodeAndSpaces)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString relative =
        QStringLiteral("assets/素材 folder/shot 01.png");
    const QString absolute = dir.filePath(relative);
    ASSERT_TRUE(QDir().mkpath(QFileInfo(absolute).absolutePath()));
    ASSERT_TRUE(writeTextFile(absolute));

    const QString candidate = projectRelativeSourceCandidate(dir.path(), absolute);
    EXPECT_EQ(candidate, relative);

    const auto resolution = resolveProjectRelativeSource(
        dir.path(), SourceResolutionCandidateKind::ProjectRelativePath,
        QStringLiteral("D:/moved/shot 01.png"), candidate, true);
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

TEST(SourceResolutionContractTest, RelativeCandidateCanResolveOutsideProjectRoot)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString projectDirectory = dir.filePath(QStringLiteral("project"));
    const QString externalDirectory = dir.filePath(QStringLiteral("shared"));
    ASSERT_TRUE(QDir().mkpath(projectDirectory));
    ASSERT_TRUE(QDir().mkpath(externalDirectory));
    const QString sourcePath = dir.filePath(QStringLiteral("shared/source.png"));
    ASSERT_TRUE(writeTextFile(sourcePath));

    const QString relativeCandidate =
        projectRelativeSourceCandidate(projectDirectory, sourcePath);
    EXPECT_TRUE(relativeCandidate.startsWith(QStringLiteral("..")));

    const auto resolution = resolveProjectRelativeSource(
        projectDirectory, SourceResolutionCandidateKind::RegistryRelativePath,
        QStringLiteral("D:/old/source.png"), relativeCandidate, true);
    EXPECT_TRUE(resolution.adopted);
    EXPECT_EQ(resolution.outcome, SourceCandidateOutcome::AdoptedExistingCandidate);
    EXPECT_EQ(resolution.resolvedPath, QDir::cleanPath(sourcePath));
}
