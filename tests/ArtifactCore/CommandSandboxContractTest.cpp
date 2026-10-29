#include <gtest/gtest.h>

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
