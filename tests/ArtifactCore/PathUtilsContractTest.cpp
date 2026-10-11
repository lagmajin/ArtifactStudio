#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringView>
#include <QTemporaryDir>

#include <filesystem>
#include <type_traits>

import Utils.Path;

using namespace ArtifactCore;

TEST(PathUtilsContractTest, IconResourceNameNormalizesSlashesAndLeadingSeparators)
{
    EXPECT_EQ(normalizeIconResourceName(QStringLiteral("\\\\icons\\visibility.svg")),
              QStringLiteral("icons/visibility.svg"));
    EXPECT_EQ(normalizeIconResourceName(QStringLiteral("/icons//custom.svg")),
              QStringLiteral("icons//custom.svg"));
    EXPECT_TRUE(normalizeIconResourceName(QString{}).isEmpty());
}

TEST(PathUtilsContractTest, IconResourcePathUsesStableResourceRoot)
{
    EXPECT_EQ(getIconResourceRoot(), QStringLiteral(":/icons"));
    EXPECT_EQ(resolveIconResourcePath(QStringLiteral("\\visibility.svg")),
              QStringLiteral(":/icons/visibility.svg"));
}

TEST(PathUtilsContractTest, MissingDiskIconFallsBackToNormalizedResourcePath)
{
    const QString missing = QStringLiteral("standalone-no-such-icon-83927\\asset.svg");
    EXPECT_EQ(resolveIconPath(missing), QStringLiteral(":/icons/standalone-no-such-icon-83927/asset.svg"));
}

TEST(PathUtilsContractTest, ExecutableAndCurrentDirectoryHelpersAreNonempty)
{
    EXPECT_FALSE(getAppPath().isEmpty());
    EXPECT_EQ(getExeDir(), getAppPath());
    EXPECT_FALSE(getCurrentDir().isEmpty());
    EXPECT_EQ(getIconPath(), QDir(getAppPath()).filePath(QStringLiteral("Icon")));
}

TEST(PathUtilsContractTest, QStringAndQStringViewConvertToFilesystemPath)
{
    const QString source = QStringLiteral("folder/subfolder/file.txt");
    const std::filesystem::path fromString = toStdPath(source);
    const QStringView view(source);
    const std::filesystem::path fromView = toStdPath(view);
    EXPECT_EQ(fromString, fromView);
    EXPECT_EQ(toQString(fromString), source);
    EXPECT_EQ(toQString(view), source);
}

TEST(PathUtilsContractTest, FilesystemPathRoundTripPreservesUnicodeSegments)
{
    const QString source = QStringLiteral("素材/😀/frame_0001.exr");

    const std::filesystem::path converted = toStdPath(source);

    EXPECT_FALSE(converted.is_absolute());
    EXPECT_EQ(toQString(converted), source);
}

TEST(PathUtilsContractTest, FilesystemPathViewConversionPreservesSupplementaryUnicode)
{
    const QString source = QStringLiteral("素材/😀/テイク 01.exr");
    const QStringView view(source);

    const std::filesystem::path converted = toStdPath(view);

    EXPECT_FALSE(converted.is_absolute());
    EXPECT_EQ(toQString(converted), source);
}

TEST(PathUtilsContractTest, FilesystemPathConversionPreservesRelativeAndAbsoluteStatus)
{
    const std::filesystem::path relative("folder/file.txt");
    EXPECT_FALSE(toStdPath(toQString(relative)).is_absolute());

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString absolute = QDir::cleanPath(directory.path() + QStringLiteral("/file.txt"));
    EXPECT_TRUE(toStdPath(absolute).is_absolute());
    EXPECT_EQ(toQString(toStdPath(absolute)), absolute);
}

TEST(PathUtilsContractTest, JoinPathUsesBaseAndChild)
{
    EXPECT_EQ(joinPath(QStringLiteral("/project/assets"), QStringLiteral("image.png")),
              QStringLiteral("/project/assets/image.png"));
}

TEST(PathUtilsContractTest, NormalizedPathCleansDotAndParentSegments)
{
    EXPECT_EQ(normalizedPath(QStringLiteral("folder/./child/../image.png")),
              QStringLiteral("folder/image.png"));
    EXPECT_EQ(cleanPath(QStringLiteral("folder/./child/../image.png")),
              QStringLiteral("folder/image.png"));
    EXPECT_EQ(normalizePath(QStringLiteral("folder/./child/../image.png")),
              QStringLiteral("folder/image.png"));
}

TEST(PathUtilsContractTest, FileAndDirectoryQueriesUseExistingTemporaryFile)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("scene.backup.project"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write("data"), 4);
    file.close();

    EXPECT_TRUE(exists(path));
    EXPECT_TRUE(pathExists(path));
    EXPECT_FALSE(exists(directory.filePath(QStringLiteral("missing.file"))));
    EXPECT_EQ(fileName(path), QStringLiteral("scene.backup.project"));
    EXPECT_EQ(lastSegment(path), fileName(path));
    EXPECT_EQ(leafName(path), fileName(path));
    EXPECT_EQ(terminalName(path), fileName(path));
}

TEST(PathUtilsContractTest, BaseNameAndCompleteBaseNameHandleMultipleDots)
{
    const QString path = QStringLiteral("/assets/scene.backup.project");
    EXPECT_EQ(baseName(path), QStringLiteral("scene"));
    EXPECT_EQ(fileStem(path), baseName(path));
    EXPECT_EQ(completeBaseName(path), QStringLiteral("scene.backup"));
    EXPECT_EQ(fileTitle(path), completeBaseName(path));
    EXPECT_EQ(completeTitle(path), completeBaseName(path));
}

TEST(PathUtilsContractTest, SuffixAndCompleteSuffixHandleMultipleDots)
{
    const QString path = QStringLiteral("/assets/scene.backup.project.tar.gz");
    EXPECT_EQ(suffix(path), QStringLiteral("gz"));
    EXPECT_EQ(completeSuffix(path), QStringLiteral("backup.project.tar.gz"));
}

TEST(PathUtilsContractTest, NamesWithoutSuffixReturnEmptySuffix)
{
    EXPECT_TRUE(suffix(QStringLiteral("/assets/README")).isEmpty());
    EXPECT_TRUE(completeSuffix(QStringLiteral("/assets/README")).isEmpty());
}

TEST(PathUtilsContractTest, DotfileWithoutExtensionKeepsItsNameAndHasNoSuffix)
{
    const QString path = QStringLiteral("/assets/.gitignore");
    EXPECT_EQ(fileName(path), QStringLiteral(".gitignore"));
    EXPECT_EQ(baseName(path), QStringLiteral(".gitignore"));
    EXPECT_EQ(completeBaseName(path), QStringLiteral(".gitignore"));
    EXPECT_TRUE(suffix(path).isEmpty());
    EXPECT_TRUE(completeSuffix(path).isEmpty());
}

TEST(PathUtilsContractTest, TrailingDotDoesNotProduceANonemptySuffix)
{
    const QString path = QStringLiteral("/assets/README.");
    EXPECT_EQ(fileName(path), QStringLiteral("README."));
    EXPECT_EQ(baseName(path), QStringLiteral("README"));
    EXPECT_EQ(completeBaseName(path), QStringLiteral("README"));
    EXPECT_TRUE(suffix(path).isEmpty());
    EXPECT_TRUE(completeSuffix(path).isEmpty());
}

TEST(PathUtilsContractTest, ParentAndDirectoryAliasesAgreeForExistingFile)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString nested = directory.filePath(QStringLiteral("nested"));
    ASSERT_TRUE(QDir().mkpath(nested));
    const QString path = QDir(nested).filePath(QStringLiteral("file.dat"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    const QString expectedParent = QFileInfo(path).dir().absolutePath();
    EXPECT_EQ(parentPath(path), expectedParent);
    EXPECT_EQ(dirName(path), expectedParent);
    EXPECT_EQ(parentDir(path), expectedParent);
    EXPECT_EQ(pathRoot(path), expectedParent);
    EXPECT_EQ(rootPath(path), expectedParent);
}

TEST(PathUtilsContractTest, AbsoluteAndRelativeQueriesAgreeWithQtFileInfo)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString absolute = directory.filePath(QStringLiteral("file.bin"));
    const QString relative = QStringLiteral("folder/file.bin");
    EXPECT_TRUE(isAbsolute(absolute));
    EXPECT_TRUE(pathIsAbsolute(absolute));
    EXPECT_FALSE(isRelative(absolute));
    EXPECT_FALSE(pathIsRelative(absolute));
    EXPECT_FALSE(isAbsolute(relative));
    EXPECT_FALSE(pathIsAbsolute(relative));
    EXPECT_TRUE(isRelative(relative));
    EXPECT_TRUE(pathIsRelative(relative));
}

TEST(PathUtilsContractTest, AbsolutePathAndRelativePathResolveAgainstSuppliedBase)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString base = directory.path();
    const QString target = QDir(base).filePath(QStringLiteral("sub/file.txt"));
    EXPECT_EQ(absolutePath(target), QFileInfo(target).absoluteFilePath());
    const QString relative = relativePath(base, target);
    EXPECT_EQ(QDir::fromNativeSeparators(relative), QStringLiteral("sub/file.txt"));
}

TEST(PathUtilsContractTest, RelativePathToItsBaseIsCurrentDirectory)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    EXPECT_EQ(relativePath(directory.path(), directory.path()), QStringLiteral("."));
}

TEST(PathUtilsContractTest, AllSegmentAndTitleAliasesAgree)
{
    const QString path = QStringLiteral("/folder/example.multi.ext");
    EXPECT_EQ(baseSegment(path), fileName(path));
    EXPECT_EQ(endSegment(path), fileName(path));
    EXPECT_EQ(trailingName(path), fileName(path));
    EXPECT_EQ(fileTitle(path), QStringLiteral("example.multi"));
    EXPECT_EQ(titleName(path), fileTitle(path));
    EXPECT_EQ(nameTitle(path), fileTitle(path));
    EXPECT_EQ(titlePath(path), fileTitle(path));
    EXPECT_EQ(pathTitle(path), fileTitle(path));
    EXPECT_EQ(titleSegment(path), fileTitle(path));
    EXPECT_EQ(headingName(path), fileTitle(path));
    EXPECT_EQ(headerName(path), fileTitle(path));
    EXPECT_EQ(headName(path), fileTitle(path));
    EXPECT_EQ(topName(path), fileTitle(path));
    EXPECT_EQ(rootName(path), fileTitle(path));
    EXPECT_EQ(baseTitle(path), fileTitle(path));
    EXPECT_EQ(titleBase(path), fileTitle(path));
}

TEST(PathUtilsContractTest, PathViewAndPathTypeAliasesMatchTheirUnderlyingTypes)
{
    static_assert(std::is_same_v<PathView, QStringView>);
    static_assert(std::is_same_v<PathString, QString>);
    static_assert(std::is_same_v<AbsolutePath, QString>);
    static_assert(std::is_same_v<RootPath, QString>);
    static_assert(std::is_same_v<FileTitle, QString>);
    static_assert(std::is_same_v<BaseTitle, QString>);
    static_assert(std::is_same_v<TitleBase, QString>);
    static_assert(std::is_same_v<TitlePath, QString>);
    static_assert(std::is_same_v<PathTitle, QString>);
}
