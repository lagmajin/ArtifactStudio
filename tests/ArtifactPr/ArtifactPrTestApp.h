// Shared QApplication bootstrap for the ArtifactPr contract tests.
//
// The timeline panel, the marker dialog and EditorEngine's save path are all
// QWidget/QObject based, so every ArtifactPr test target needs a QApplication
// with the offscreen platform plugin (set through the CTest ENVIRONMENT).
//
// Each test translation unit registers the environment with its own
// namespace-scope variable, matching the existing
// tests/Artifact/ArtifactScriptLayerIntegrationTest.cpp idiom.
#pragma once

#include <gtest/gtest.h>

#include <QApplication>
#include <QStandardPaths>
#include <QString>

namespace ArtifactPrTest {

inline QApplication* application()
{
    static int argc = 1;
    static char applicationName[] = "ArtifactPrContractTest";
    static char* argv[] = {applicationName, nullptr};

    static QApplication* instance = [] {
        auto* app = new QApplication(argc, argv);
        QApplication::setApplicationName(QStringLiteral("ArtifactPrContractTest"));
        QApplication::setOrganizationName(QStringLiteral("ArtifactStudio"));
        QStandardPaths::setTestModeEnabled(true);
        return app;
    }();
    return instance;
}

class ApplicationEnvironment final : public testing::Environment
{
public:
    void SetUp() override { (void)application(); }
};

inline bool registerApplicationEnvironment()
{
    static auto* environment = new ApplicationEnvironment();
    static const bool registered = [] {
        testing::AddGlobalTestEnvironment(environment);
        return true;
    }();
    return registered;
}

} // namespace ArtifactPrTest
