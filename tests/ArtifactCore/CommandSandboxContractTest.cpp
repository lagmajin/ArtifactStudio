#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

import Core.AI.CommandSandbox;

using namespace ArtifactCore;

namespace {

QString createProgramFile(QTemporaryDir& directory, const QString& name)
{
    const QString path = directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (file.write("placeholder") != 11) {
        return {};
    }
    file.close();
    return path;
}

QVariantMap dryRun(CommandSandbox& sandbox, const QString& program,
                   const QVariantList& arguments = {})
{
    return sandbox.dryRun(program, arguments).toMap();
}

} // namespace

TEST(CommandSandboxContractTest, DefaultsUseNarrowAllowlistAndBoundedLimits)
{
    CommandSandbox sandbox;
    const QJsonObject policy = sandbox.policyJson();

    EXPECT_EQ(policy.value(QStringLiteral("allowedPrograms")).toArray().size(), 12);
    EXPECT_TRUE(policy.value(QStringLiteral("allowedPrograms")).toArray()
                    .contains(QStringLiteral("git.exe")));
    EXPECT_EQ(policy.value(QStringLiteral("defaultTimeoutMs")).toInt(), 5000);
    EXPECT_EQ(policy.value(QStringLiteral("maxOutputBytes")).toInt(), 131072);
    EXPECT_FALSE(policy.value(QStringLiteral("allowShellPrograms")).toBool());
    EXPECT_TRUE(policy.value(QStringLiteral("allowedEnvironmentKeys")).toArray()
                    .contains(QStringLiteral("PATH")));
}

TEST(CommandSandboxContractTest, DryRunValidatesWithoutStartingOrFinishingAProcess)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString program = createProgramFile(directory, QStringLiteral("allowed-tool.exe"));
    ASSERT_FALSE(program.isEmpty());

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({program});
    sandbox.setWorkingDirectory(directory.path());
    const QVariantMap result = dryRun(sandbox, program, {
        QStringLiteral("--flag"), QStringLiteral("value with spaces"),
        QStringLiteral("; this remains an argument"),
    });

    EXPECT_TRUE(result.value(QStringLiteral("dryRun")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("validationPassed")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("allowed")).toBool());
    EXPECT_FALSE(result.value(QStringLiteral("started")).toBool());
    EXPECT_FALSE(result.value(QStringLiteral("finished")).toBool());
    EXPECT_FALSE(result.value(QStringLiteral("ok")).toBool());
    EXPECT_EQ(result.value(QStringLiteral("requestedProgram")).toString(), program);
    EXPECT_EQ(result.value(QStringLiteral("resolvedProgram")).toString(), program);
    EXPECT_EQ(result.value(QStringLiteral("arguments")).toStringList(), QStringList({
        QStringLiteral("--flag"), QStringLiteral("value with spaces"),
        QStringLiteral("; this remains an argument"),
    }));
    EXPECT_EQ(result.value(QStringLiteral("workingDirectory")).toString(),
              QDir::cleanPath(directory.path()));
}

TEST(CommandSandboxContractTest,
     RunStartsAllowlistedTestExecutableAndCapturesItsOutput)
{
    const QString executable = QCoreApplication::applicationFilePath();
    ASSERT_FALSE(executable.isEmpty());
    ASSERT_TRUE(QFile::exists(executable));

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({executable});
    sandbox.setWorkingDirectory(QCoreApplication::applicationDirPath());

    const QVariantMap result = sandbox.run(
        executable, {QStringLiteral("--gtest_list_tests")}).toMap();

    EXPECT_TRUE(result.value(QStringLiteral("allowed")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("started")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("finished")).toBool());
    EXPECT_FALSE(result.value(QStringLiteral("timedOut")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("ok")).toBool());
    EXPECT_EQ(result.value(QStringLiteral("exitCode")).toInt(), 0);
    EXPECT_EQ(result.value(QStringLiteral("arguments")).toStringList(),
              (QStringList{QStringLiteral("--gtest_list_tests")}));
    EXPECT_TRUE(result.value(QStringLiteral("stdout")).toString()
                    .contains(QStringLiteral("CommandSandboxContractTest.")));
    EXPECT_TRUE(result.value(QStringLiteral("stderr")).toString().isEmpty());
}

TEST(CommandSandboxContractTest, ChildProcessSleepHelper)
{
    const QStringList arguments = QCoreApplication::arguments();
    if (!arguments.contains(QStringLiteral(
            "--gtest_filter=CommandSandboxContractTest.ChildProcessSleepHelper"))) {
        return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
}

TEST(CommandSandboxContractTest, ChildProcessOutputHelper)
{
    const QStringList arguments = QCoreApplication::arguments();
    if (!arguments.contains(QStringLiteral(
            "--gtest_filter=CommandSandboxContractTest.ChildProcessOutputHelper"))) {
        return;
    }

    const std::string output(8192, 'x');
    std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
    std::cout.flush();
    std::cerr.write(output.data(), static_cast<std::streamsize>(output.size()));
    std::cerr.flush();
}

TEST(CommandSandboxContractTest, ChildProcessEnvironmentHelper)
{
    const QStringList arguments = QCoreApplication::arguments();
    if (!arguments.contains(QStringLiteral(
            "--gtest_filter=CommandSandboxContractTest.ChildProcessEnvironmentHelper"))) {
        return;
    }

    if (qEnvironmentVariableIsSet("ARTIFACT_SANDBOX_TEST_SECRET")) {
        std::cout << "SANDBOX_ENV_PRESENT:"
                  << qgetenv("ARTIFACT_SANDBOX_TEST_SECRET").constData();
    } else {
        std::cout << "SANDBOX_ENV_ABSENT";
    }
    std::cout.flush();
}

TEST(CommandSandboxContractTest,
     RunPassesOnlyExplicitlyAllowlistedEnvironmentVariables)
{
    constexpr auto key = "ARTIFACT_SANDBOX_TEST_SECRET";
    const bool hadPreviousValue = qEnvironmentVariableIsSet(key);
    const QByteArray previousValue = qgetenv(key);
    struct EnvironmentRestore {
        const char* key;
        bool hadPreviousValue;
        QByteArray previousValue;
        ~EnvironmentRestore()
        {
            if (hadPreviousValue) {
                qputenv(key, previousValue);
            } else {
                qunsetenv(key);
            }
        }
    } restore{key, hadPreviousValue, previousValue};
    ASSERT_TRUE(qputenv(key, QByteArrayLiteral("private-child-value")));

    const QString executable = QCoreApplication::applicationFilePath();
    ASSERT_FALSE(executable.isEmpty());
    ASSERT_TRUE(QFile::exists(executable));

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({executable});
    sandbox.setWorkingDirectory(QCoreApplication::applicationDirPath());
    sandbox.setAllowedEnvironmentKeys({QStringLiteral("PATH")});
    const QVariantMap denied = sandbox.run(executable, {
        QStringLiteral(
            "--gtest_filter=CommandSandboxContractTest.ChildProcessEnvironmentHelper")
    }).toMap();
    ASSERT_TRUE(denied.value(QStringLiteral("ok")).toBool());
    EXPECT_TRUE(denied.value(QStringLiteral("stdout")).toString()
                    .contains(QStringLiteral("SANDBOX_ENV_ABSENT")));
    EXPECT_FALSE(denied.value(QStringLiteral("stdout")).toString()
                     .contains(QStringLiteral("private-child-value")));

    sandbox.setAllowedEnvironmentKeys({QStringLiteral("PATH"),
                                       QString::fromLatin1(key)});
    const QVariantMap allowed = sandbox.run(executable, {
        QStringLiteral(
            "--gtest_filter=CommandSandboxContractTest.ChildProcessEnvironmentHelper")
    }).toMap();
    ASSERT_TRUE(allowed.value(QStringLiteral("ok")).toBool());
    EXPECT_TRUE(allowed.value(QStringLiteral("stdout")).toString()
                    .contains(QStringLiteral("SANDBOX_ENV_PRESENT:private-child-value")));
}

TEST(CommandSandboxContractTest,
     RunCapsStdoutAndStderrIndependentlyAndReportsTruncation)
{
    const QString executable = QCoreApplication::applicationFilePath();
    ASSERT_FALSE(executable.isEmpty());
    ASSERT_TRUE(QFile::exists(executable));

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({executable});
    sandbox.setWorkingDirectory(QCoreApplication::applicationDirPath());
    sandbox.setMaxOutputBytes(1024);

    const QVariantMap result = sandbox.run(executable, {
        QStringLiteral(
            "--gtest_filter=CommandSandboxContractTest.ChildProcessOutputHelper")
    }).toMap();

    ASSERT_TRUE(result.value(QStringLiteral("started")).toBool());
    ASSERT_TRUE(result.value(QStringLiteral("finished")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("ok")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("outputTruncated")).toBool());
    EXPECT_EQ(result.value(QStringLiteral("stdout")).toString().toUtf8().size(), 1024);
    EXPECT_EQ(result.value(QStringLiteral("stderr")).toString().toUtf8().size(), 1024);
}

TEST(CommandSandboxContractTest, RunKillsTimedOutChildAndReportsTerminalTimeout)
{
    const QString executable = QCoreApplication::applicationFilePath();
    ASSERT_FALSE(executable.isEmpty());
    ASSERT_TRUE(QFile::exists(executable));

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({executable});
    sandbox.setWorkingDirectory(QCoreApplication::applicationDirPath());
    sandbox.setDefaultTimeoutMs(1500);

    const QVariantMap result = sandbox.run(executable, {
        QStringLiteral("--gtest_filter=CommandSandboxContractTest.ChildProcessSleepHelper")});

    EXPECT_TRUE(result.value(QStringLiteral("allowed")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("started")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("finished")).toBool());
    EXPECT_TRUE(result.value(QStringLiteral("timedOut")).toBool());
    EXPECT_FALSE(result.value(QStringLiteral("ok")).toBool());
}

TEST(CommandSandboxContractTest, RejectsEmptyMissingAndNonAllowlistedPrograms)
{
    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({QStringLiteral("allowed-tool.exe")});

    const QVariantMap empty = dryRun(sandbox, QStringLiteral("   "));
    EXPECT_FALSE(empty.value(QStringLiteral("allowed")).toBool());
    EXPECT_EQ(empty.value(QStringLiteral("error")).toString(),
              QStringLiteral("Command program is empty"));

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString missingPath = directory.filePath(
        QStringLiteral("missing/allowed-tool.exe"));
    const QVariantMap missing = dryRun(sandbox, missingPath);
    EXPECT_FALSE(missing.value(QStringLiteral("allowed")).toBool());
    EXPECT_TRUE(missing.value(QStringLiteral("error")).toString()
                    .startsWith(QStringLiteral("Executable does not exist:")));

    const QVariantMap denied = dryRun(sandbox, QStringLiteral("another-tool.exe"));
    EXPECT_FALSE(denied.value(QStringLiteral("allowed")).toBool());
    EXPECT_EQ(denied.value(QStringLiteral("error")).toString(),
              QStringLiteral("Program is not on the allowlist: another-tool.exe"));
}

TEST(CommandSandboxContractTest, EmptyAllowlistFailsClosedInsteadOfDisablingValidation)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString program = createProgramFile(directory, QStringLiteral("tool.exe"));
    ASSERT_FALSE(program.isEmpty());

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({});
    const QVariantMap result = dryRun(sandbox, program);

    EXPECT_FALSE(result.value(QStringLiteral("allowed")).toBool());
    EXPECT_FALSE(result.value(QStringLiteral("validationPassed")).toBool());
    EXPECT_FALSE(result.value(QStringLiteral("started")).toBool());
    EXPECT_EQ(result.value(QStringLiteral("error")).toString(),
              QStringLiteral("Program is not on the allowlist: %1").arg(program));
}

TEST(CommandSandboxContractTest, AbsoluteAllowlistDoesNotAuthorizeSameNamedProgramElsewhere)
{
    QTemporaryDir trustedDirectory;
    QTemporaryDir otherDirectory;
    ASSERT_TRUE(trustedDirectory.isValid());
    ASSERT_TRUE(otherDirectory.isValid());
    const QString trustedProgram = createProgramFile(
        trustedDirectory, QStringLiteral("tool.exe"));
    const QString otherProgram = createProgramFile(
        otherDirectory, QStringLiteral("tool.exe"));
    ASSERT_FALSE(trustedProgram.isEmpty());
    ASSERT_FALSE(otherProgram.isEmpty());
    ASSERT_NE(QDir::cleanPath(trustedProgram), QDir::cleanPath(otherProgram));

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({trustedProgram});

    const QVariantMap trusted = dryRun(sandbox, trustedProgram);
    const QVariantMap other = dryRun(sandbox, otherProgram);

    EXPECT_TRUE(trusted.value(QStringLiteral("allowed")).toBool());
    EXPECT_FALSE(other.value(QStringLiteral("allowed")).toBool());
    EXPECT_EQ(other.value(QStringLiteral("error")).toString(),
              QStringLiteral("Program is not on the allowlist: %1").arg(otherProgram));
}

TEST(CommandSandboxContractTest, AbsoluteAllowlistAndBlocklistPathMatchingIgnoresCase)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString program = createProgramFile(directory, QStringLiteral("tool.exe"));
    ASSERT_FALSE(program.isEmpty());

    QString alternateCasePath = program;
    for (qsizetype index = 0; index < alternateCasePath.size(); ++index) {
        const QChar character = alternateCasePath.at(index);
        if (character.isLetter()) {
            alternateCasePath[index] = character.isLower() ? character.toUpper()
                                                            : character.toLower();
            break;
        }
    }
    ASSERT_NE(alternateCasePath, program);

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({alternateCasePath});
    const QVariantMap allowedByCaseInsensitivePath = dryRun(sandbox, program);
    EXPECT_TRUE(allowedByCaseInsensitivePath.value(QStringLiteral("allowed")).toBool());

    sandbox.setBlockedPrograms({alternateCasePath});
    const QVariantMap blockedByCaseInsensitivePath = dryRun(sandbox, program);
    EXPECT_FALSE(blockedByCaseInsensitivePath.value(QStringLiteral("allowed")).toBool());
    EXPECT_EQ(blockedByCaseInsensitivePath.value(QStringLiteral("error")).toString(),
              QStringLiteral("Program is blocked by policy: %1").arg(program));
}

TEST(CommandSandboxContractTest, BlocklistOverridesAllowlistAndShellPolicyOverridesBoth)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString blockedProgram = createProgramFile(directory, QStringLiteral("tool.exe"));
    const QString shellProgram = createProgramFile(directory, QStringLiteral("pwsh.exe"));
    ASSERT_FALSE(blockedProgram.isEmpty());
    ASSERT_FALSE(shellProgram.isEmpty());

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({blockedProgram, shellProgram});
    sandbox.setBlockedPrograms({QStringLiteral("tool.exe")});

    const QVariantMap blocked = dryRun(sandbox, blockedProgram);
    EXPECT_FALSE(blocked.value(QStringLiteral("allowed")).toBool());
    EXPECT_EQ(blocked.value(QStringLiteral("error")).toString(),
              QStringLiteral("Program is blocked by policy: %1").arg(blockedProgram));

    const QVariantMap shellDenied = dryRun(sandbox, shellProgram);
    EXPECT_FALSE(shellDenied.value(QStringLiteral("allowed")).toBool());
    EXPECT_EQ(shellDenied.value(QStringLiteral("error")).toString(),
              QStringLiteral("Shell programs are disabled by policy: %1").arg(shellProgram));

    sandbox.setAllowShellPrograms(true);
    const QVariantMap shellAllowed = dryRun(sandbox, shellProgram);
    EXPECT_TRUE(shellAllowed.value(QStringLiteral("allowed")).toBool());
    EXPECT_TRUE(shellAllowed.value(QStringLiteral("validationPassed")).toBool());
}

TEST(CommandSandboxContractTest, WorkingDirectoryMustExistAndIsNormalized)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString program = createProgramFile(directory, QStringLiteral("tool.exe"));
    ASSERT_FALSE(program.isEmpty());

    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({program});
    sandbox.setWorkingDirectory(directory.filePath(QStringLiteral("missing")));
    const QVariantMap rejected = dryRun(sandbox, program);
    EXPECT_FALSE(rejected.value(QStringLiteral("allowed")).toBool());
    EXPECT_TRUE(rejected.value(QStringLiteral("error")).toString()
                    .startsWith(QStringLiteral("Working directory does not exist:")));

    sandbox.setWorkingDirectory(directory.path() + QStringLiteral("/."));
    const QVariantMap accepted = dryRun(sandbox, program);
    EXPECT_TRUE(accepted.value(QStringLiteral("allowed")).toBool());
    EXPECT_EQ(accepted.value(QStringLiteral("workingDirectory")).toString(),
              QDir::cleanPath(directory.path()));
}

TEST(CommandSandboxContractTest, PolicySettersSanitizeListsAndClampResourceLimits)
{
    CommandSandbox sandbox;
    sandbox.setAllowedPrograms({QStringLiteral(" tool "), QStringLiteral("tool"),
                                QStringLiteral(""), QStringLiteral("other")});
    sandbox.setBlockedPrograms({QStringLiteral(" denied "), QStringLiteral("denied")});
    sandbox.setAllowedEnvironmentKeys({QStringLiteral(" PATH "), QStringLiteral("PATH")});
    sandbox.setDefaultTimeoutMs(0);
    sandbox.setMaxOutputBytes(0);

    const QJsonObject policy = sandbox.policyJson();
    EXPECT_EQ(policy.value(QStringLiteral("allowedPrograms")).toArray().size(), 2);
    EXPECT_EQ(policy.value(QStringLiteral("blockedPrograms")).toArray().size(), 1);
    EXPECT_EQ(policy.value(QStringLiteral("allowedEnvironmentKeys")).toArray().size(), 1);
    EXPECT_EQ(policy.value(QStringLiteral("defaultTimeoutMs")).toInt(), 1);
    EXPECT_EQ(policy.value(QStringLiteral("maxOutputBytes")).toInt(), 1024);

    sandbox.resetPolicy();
    EXPECT_EQ(sandbox.policyJson().value(QStringLiteral("defaultTimeoutMs")).toInt(), 5000);
    EXPECT_EQ(sandbox.policyJson().value(QStringLiteral("maxOutputBytes")).toInt(), 131072);
    EXPECT_FALSE(sandbox.policyJson().value(QStringLiteral("blockedPrograms")).toArray().size());
}
