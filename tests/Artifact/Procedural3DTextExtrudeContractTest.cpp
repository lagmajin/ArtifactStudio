#include <gtest/gtest.h>

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

import Procedural3DGenerators;

// Contract coverage for text extrusion mesh generation.

namespace {

class Procedural3DTextExtrudeContractTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        static int argc = 1;
        static char applicationName[] = "Procedural3DTextExtrudeContractTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
    }

    inline static QGuiApplication* application_ = nullptr;
};

} // namespace

TEST_F(Procedural3DTextExtrudeContractTest,
       GeneratesCenteredRenderableMeshWithRequestedDepth)
{
    ArtifactCore::TextExtrudeSettings settings;
    settings.text = QStringLiteral("3D TEXT");
    settings.fontFamily = QStringLiteral("Arial");
    settings.fontSize = 96;
    settings.depth = 32.0f;
    settings.bevelWidth = 0.0f;

    const char* capturePath = std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_OBJ");
    const char* captureProfile = std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_PROFILE");
    const char* captureText = std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_TEXT");
    const char* captureFontPath = std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_PATH");
    if (capturePath != nullptr && capturePath[0] != '\0'
        && captureProfile != nullptr) {
        const std::string profile(captureProfile);
        if (profile == "han") {
            settings.text = QStringLiteral("漢字仮名");
        } else if (profile == "arabic") {
            settings.text = QStringLiteral("مرحبا بالعالم");
        } else if (profile == "devanagari") {
            settings.text = QStringLiteral("नमस्ते दुनिया");
        } else if (profile == "thai") {
            settings.text = QStringLiteral("ภาษาไทย");
        } else if (profile == "hebrew") {
            settings.text = QStringLiteral("שלום עולם");
        } else if (profile == "hangul") {
            settings.text = QStringLiteral("한글 테스트");
        } else if (profile == "arabic-mixed") {
            settings.text = QStringLiteral("ABC — مَرْحَبًا بِالْعَالَمِينَ (123)");
        } else if (profile == "devanagari-conjuncts") {
            settings.text = QStringLiteral("क्षेत्रज्ञ कर्मण्येवाधिकारस्ते");
        } else if (profile == "bengali-conjuncts") {
            settings.text = QStringLiteral("বাংলা ভাষা: ক্ষুদ্র");
        } else if (profile == "tamil-conjuncts") {
            settings.text = QStringLiteral("தமிழ் மொழி: வணக்கம்");
        } else if (profile == "gujarati-conjuncts") {
            settings.text = QStringLiteral("ગુજરાતી ભાષા: ક્ષમતા");
        } else if (profile == "tibetan") {
            settings.text = QStringLiteral("བོད་ཡིག་རིགས");
        } else if (profile == "ethiopic") {
            settings.text = QStringLiteral("እንደምን አላችሁ");
        } else if (profile == "hangul-jamo") {
            settings.text = QStringLiteral("한글 테스트");
        } else if (profile == "ligatures-marks") {
            settings.text = QStringLiteral("office affinity ffi e\u0301");
        }
    }
    if (capturePath != nullptr && capturePath[0] != '\0'
        && captureText != nullptr && captureText[0] != '\0') {
        settings.text = QString::fromLocal8Bit(captureText);
    }
    if (capturePath != nullptr && capturePath[0] != '\0'
        && captureFontPath != nullptr && captureFontPath[0] != '\0') {
        const int fontId = QFontDatabase::addApplicationFont(
            QString::fromLocal8Bit(captureFontPath));
        ASSERT_GE(fontId, 0) << "Could not load capture font: " << captureFontPath;
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        ASSERT_FALSE(families.isEmpty()) << "Capture font has no registered family: "
                                         << captureFontPath;
        settings.fontFamily = families.front();
    }

    const ArtifactCore::Procedural3DMeshData mesh =
        ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings);

    if (capturePath != nullptr && capturePath[0] != '\0') {
        QFont font(settings.fontFamily);
        std::ofstream capture(capturePath, std::ios::out | std::ios::trunc);
        ASSERT_TRUE(capture.is_open()) << "Could not open text extrusion capture mesh: "
                                       << capturePath;
        capture << "# resolved-font "
                << QFontInfo(font).family().toStdString() << '\n';
        for (const ArtifactCore::Procedural3DVertex& vertex : mesh.vertices) {
            capture << "v " << vertex.px << ' ' << vertex.py << ' ' << vertex.pz << '\n';
        }
        for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
            capture << "f " << mesh.indices[index] + 1 << ' '
                    << mesh.indices[index + 1] + 1 << ' '
                    << mesh.indices[index + 2] + 1 << '\n';
        }
        ASSERT_TRUE(capture.good()) << "Could not write text extrusion capture mesh: "
                                    << capturePath;
    }

    ASSERT_FALSE(mesh.vertices.empty());
    ASSERT_GE(mesh.indices.size(), 3u);
    EXPECT_EQ(mesh.indices.size() % 3u, 0u);

    float minX = mesh.vertices.front().px;
    float maxX = minX;
    float minY = mesh.vertices.front().py;
    float maxY = minY;
    float minZ = mesh.vertices.front().pz;
    float maxZ = minZ;
    for (const ArtifactCore::Procedural3DVertex& vertex : mesh.vertices) {
        minX = std::min(minX, vertex.px);
        maxX = std::max(maxX, vertex.px);
        minY = std::min(minY, vertex.py);
        maxY = std::max(maxY, vertex.py);
        minZ = std::min(minZ, vertex.pz);
        maxZ = std::max(maxZ, vertex.pz);
        EXPECT_TRUE(std::isfinite(vertex.nx));
        EXPECT_TRUE(std::isfinite(vertex.ny));
        EXPECT_TRUE(std::isfinite(vertex.nz));
    }

    for (const std::uint32_t index : mesh.indices) {
        EXPECT_LT(index, mesh.vertices.size());
    }

    EXPECT_NEAR((minX + maxX) * 0.5f, 0.0f, 1.0e-3f);
    EXPECT_NEAR((minY + maxY) * 0.5f, 0.0f, 1.0e-3f);
    EXPECT_FLOAT_EQ(minZ, -16.0f);
    EXPECT_FLOAT_EQ(maxZ, 16.0f);
}

TEST_F(Procedural3DTextExtrudeContractTest,
       GeneratesComplexB8AmpersandTextMesh)
{
    ArtifactCore::TextExtrudeSettings settings;
    settings.text = QStringLiteral("B8&");
    settings.fontFamily = QStringLiteral("Arial");
    settings.fontSize = 96;
    settings.depth = 32.0f;
    settings.bevelWidth = 0.0f;

    const ArtifactCore::Procedural3DMeshData mesh =
        ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings);

    ASSERT_FALSE(mesh.vertices.empty());
    ASSERT_GE(mesh.indices.size(), 3u);
    EXPECT_EQ(mesh.indices.size() % 3u, 0u);
    for (const std::uint32_t index : mesh.indices) {
        EXPECT_LT(index, mesh.vertices.size());
    }
    for (const ArtifactCore::Procedural3DVertex& vertex : mesh.vertices) {
        EXPECT_TRUE(std::isfinite(vertex.px));
        EXPECT_TRUE(std::isfinite(vertex.py));
        EXPECT_TRUE(std::isfinite(vertex.pz));
        EXPECT_TRUE(std::isfinite(vertex.nx));
        EXPECT_TRUE(std::isfinite(vertex.ny));
        EXPECT_TRUE(std::isfinite(vertex.nz));
    }
}

TEST_F(Procedural3DTextExtrudeContractTest,
       GeneratesMeshesForComplexWritingSystems)
{
    const QString samples[] = {
        QStringLiteral("漢字仮名"),
        QStringLiteral("مرحبا بالعالم"),
        QStringLiteral("नमस्ते दुनिया"),
        QStringLiteral("ภาษาไทย"),
        QStringLiteral("שלום עולם"),
        QStringLiteral("한글 테스트")};

    for (const QString& sample : samples) {
        SCOPED_TRACE(sample.toStdString());
        ArtifactCore::TextExtrudeSettings settings;
        settings.text = sample;
        settings.fontSize = 96;
        settings.depth = 32.0f;
        settings.bevelWidth = 0.0f;
        const ArtifactCore::Procedural3DMeshData mesh =
            ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings);

        ASSERT_FALSE(mesh.vertices.empty());
        ASSERT_GE(mesh.indices.size(), 3u);
        EXPECT_EQ(mesh.indices.size() % 3u, 0u);
        for (const std::uint32_t index : mesh.indices) {
            EXPECT_LT(index, mesh.vertices.size());
        }
        for (const ArtifactCore::Procedural3DVertex& vertex : mesh.vertices) {
            EXPECT_TRUE(std::isfinite(vertex.px));
            EXPECT_TRUE(std::isfinite(vertex.py));
            EXPECT_TRUE(std::isfinite(vertex.pz));
            EXPECT_TRUE(std::isfinite(vertex.nx));
            EXPECT_TRUE(std::isfinite(vertex.ny));
            EXPECT_TRUE(std::isfinite(vertex.nz));
        }
    }
}

TEST_F(Procedural3DTextExtrudeContractTest,
       GeneratesMeshesForAdvancedShapingSequences)
{
    struct Sample {
        QString text;
        QString fontFamily;
    };
    const Sample samples[] = {
        {QStringLiteral("ABC — مَرْحَبًا بِالْعَالَمِينَ (123)"),
         QStringLiteral("Tahoma")},
        {QStringLiteral("می\u200Cخواهم فارسی بخوانم"), QStringLiteral("Tahoma")},
        {QStringLiteral("क्षेत्रज्ञ कर्मण्येवाधिकारस्ते"), QStringLiteral("Nirmala UI")},
        {QStringLiteral("বাংলা ভাষা: ক্ষুদ্র"), QStringLiteral("Nirmala UI")},
        {QStringLiteral("தமிழ் மொழி: வணக்கம்"), QStringLiteral("Nirmala UI")},
        {QStringLiteral("ગુજરાતી ભાષા: ક્ષમતા"), QStringLiteral("Nirmala UI")},
        {QStringLiteral("བོད་ཡིག་རིགས"), QStringLiteral("Microsoft Himalaya")},
        {QStringLiteral("እንደምን አላችሁ"), QStringLiteral("Ebrima")},
        {QStringLiteral("한글 테스트"), QStringLiteral("Malgun Gothic")},
        {QStringLiteral("office affinity ffi e\u0301"), QStringLiteral("Arial")},
        {QStringLiteral("か\u3099く\u3099"), QStringLiteral("Noto Sans JP")},
    };

    for (const Sample& sample : samples) {
        SCOPED_TRACE(sample.text.toStdString());
        ArtifactCore::TextExtrudeSettings settings;
        settings.text = sample.text;
        settings.fontFamily = sample.fontFamily;
        settings.fontSize = 96;
        settings.depth = 32.0f;
        settings.bevelWidth = 0.0f;

        const ArtifactCore::Procedural3DMeshData mesh =
            ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings);

        ASSERT_FALSE(mesh.vertices.empty());
        ASSERT_GE(mesh.indices.size(), 3u);
        EXPECT_EQ(mesh.indices.size() % 3u, 0u);
        float minZ = mesh.vertices.front().pz;
        float maxZ = minZ;
        for (const ArtifactCore::Procedural3DVertex& vertex : mesh.vertices) {
            EXPECT_TRUE(std::isfinite(vertex.px));
            EXPECT_TRUE(std::isfinite(vertex.py));
            EXPECT_TRUE(std::isfinite(vertex.pz));
            EXPECT_TRUE(std::isfinite(vertex.nx));
            EXPECT_TRUE(std::isfinite(vertex.ny));
            EXPECT_TRUE(std::isfinite(vertex.nz));
            minZ = std::min(minZ, vertex.pz);
            maxZ = std::max(maxZ, vertex.pz);
        }
        for (const std::uint32_t index : mesh.indices) {
            EXPECT_LT(index, mesh.vertices.size());
        }
        EXPECT_FLOAT_EQ(minZ, -16.0f);
        EXPECT_FLOAT_EQ(maxZ, 16.0f);
    }
}

TEST_F(Procedural3DTextExtrudeContractTest,
     EmptyOrWhitespaceTextProducesNoMesh)
{
    ArtifactCore::TextExtrudeSettings settings;
    settings.depth = 24.0f;

    settings.text.clear();
    EXPECT_TRUE(ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings)
                    .vertices.empty());

    settings.text = QStringLiteral("   ");
    EXPECT_TRUE(ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings)
                    .vertices.empty());
}

TEST_F(Procedural3DTextExtrudeContractTest,
     BevelAddsMeshGeometry)
{
    ArtifactCore::TextExtrudeSettings settings;
    settings.text = QStringLiteral("A");
    settings.fontSize = 96;
    settings.depth = 24.0f;
    settings.bevelWidth = 0.0f;
    const auto straight =
        ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings);

    settings.bevelWidth = 2.0f;
    settings.bevelSegments = 2;
    const auto beveled =
        ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings);

    ASSERT_FALSE(straight.vertices.empty());
    ASSERT_FALSE(beveled.vertices.empty());
    EXPECT_GT(beveled.vertices.size(), straight.vertices.size());
    EXPECT_EQ(beveled.indices.size() % 3u, 0u);
}
