#include <gtest/gtest.h>

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QtGlobal>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

import Procedural3DGenerators;
import Font.FreeFont;
import Core.ArtifactArray;

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

void expectAdvancedExtrudedMesh(const QString& text, const QString& fontFamily)
{
    SCOPED_TRACE(text.toStdString());
    ArtifactCore::TextExtrudeSettings settings;
    settings.text = text;
    settings.fontFamily = fontFamily;
    settings.fontSize = 96;
    settings.depth = 32.0f;
    settings.bevelWidth = 0.0f;
    ArtifactCore::TextExtrudeDiagnostics diagnostics;
    const ArtifactCore::Procedural3DMeshData mesh =
        ArtifactCore::Procedural3DGenerators::generateTextExtrude(
            settings, 0.0f, &diagnostics);
    const char* strictCoverageValue =
        std::getenv("ARTIFACT_TEXT_EXTRUDE_REQUIRE_SCRIPT_COVERAGE");
    const bool requireScriptCoverage = strictCoverageValue != nullptr
        && strictCoverageValue[0] != '\0'
        && std::string(strictCoverageValue) != "0";
    bool requestedFontUsed = false;
    for (const QString& resolvedFamily : diagnostics.resolvedFontFamilies) {
        if (resolvedFamily.compare(fontFamily, Qt::CaseInsensitive) == 0) {
            requestedFontUsed = true;
            break;
        }
    }
    if (!requestedFontUsed) {
        const std::string message =
            "Requested font family was not used by any shaped run: "
            + fontFamily.toStdString() + "; resolved: "
            + diagnostics.resolvedFontFamilies.join(QStringLiteral(", ")).toStdString();
        if (requireScriptCoverage) {
            FAIL() << message;
        } else {
            GTEST_SKIP() << message;
        }
    }
    if (diagnostics.missingGlyphCount > 0u) {
        const std::string message =
            "Requested script is not fully covered by the resolved font(s): "
            + diagnostics.resolvedFontFamilies.join(QStringLiteral(", ")).toStdString()
            + "; " + std::to_string(diagnostics.missingGlyphCount)
            + " missing shaped glyph(s)";
        if (requireScriptCoverage) {
            FAIL() << message;
        } else {
            GTEST_SKIP() << message;
        }
    }
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
        EXPECT_TRUE(std::isfinite(vertex.u));
        EXPECT_TRUE(std::isfinite(vertex.v));
        const float normalLength = std::sqrt(
            vertex.nx * vertex.nx + vertex.ny * vertex.ny + vertex.nz * vertex.nz);
        EXPECT_NEAR(normalLength, 1.0f, 1.0e-3f);
        minZ = std::min(minZ, vertex.pz);
        maxZ = std::max(maxZ, vertex.pz);
    }
    for (const std::uint32_t index : mesh.indices) {
        ASSERT_LT(index, mesh.vertices.size());
    }
    for (std::size_t index = 0; index + 2u < mesh.indices.size(); index += 3u) {
        const auto& a = mesh.vertices[mesh.indices[index]];
        const auto& b = mesh.vertices[mesh.indices[index + 1u]];
        const auto& c = mesh.vertices[mesh.indices[index + 2u]];
        const float abx = b.px - a.px;
        const float aby = b.py - a.py;
        const float abz = b.pz - a.pz;
        const float acx = c.px - a.px;
        const float acy = c.py - a.py;
        const float acz = c.pz - a.pz;
        const float areaX = aby * acz - abz * acy;
        const float areaY = abz * acx - abx * acz;
        const float areaZ = abx * acy - aby * acx;
        const float areaLengthSquared = areaX * areaX + areaY * areaY + areaZ * areaZ;
        EXPECT_TRUE(std::isfinite(areaLengthSquared));
        EXPECT_GT(areaLengthSquared, 1.0e-16f);
    }
    EXPECT_FLOAT_EQ(minZ, -16.0f);
    EXPECT_FLOAT_EQ(maxZ, 16.0f);
}

std::array<std::int64_t, 3> weldedPositionKey(
    const ArtifactCore::Procedural3DVertex& vertex)
{
    constexpr double scale = 100000.0;
    return {static_cast<std::int64_t>(std::llround(vertex.px * scale)),
            static_cast<std::int64_t>(std::llround(vertex.py * scale)),
            static_cast<std::int64_t>(std::llround(vertex.pz * scale))};
}

std::array<std::int64_t, 6> weldedEdgeKey(
    const ArtifactCore::Procedural3DVertex& lhs,
    const ArtifactCore::Procedural3DVertex& rhs)
{
    const auto a = weldedPositionKey(lhs);
    const auto b = weldedPositionKey(rhs);
    const auto& first = a < b ? a : b;
    const auto& second = a < b ? b : a;
    return {first[0], first[1], first[2], second[0], second[1], second[2]};
}

void expectClosedTriangleMesh(const ArtifactCore::Procedural3DMeshData& mesh)
{
    ASSERT_EQ(mesh.indices.size() % 3u, 0u);
    for (const std::uint32_t index : mesh.indices) {
        ASSERT_LT(index, mesh.vertices.size());
    }
    ArtifactCore::ArtifactArray<std::array<std::int64_t, 6>> edges;
    edges.reserve(mesh.indices.size());
    for (std::size_t index = 0; index < mesh.indices.size(); index += 3u) {
        const auto& a = mesh.vertices[mesh.indices[index]];
        const auto& b = mesh.vertices[mesh.indices[index + 1u]];
        const auto& c = mesh.vertices[mesh.indices[index + 2u]];
        const double abx = static_cast<double>(b.px) - a.px;
        const double aby = static_cast<double>(b.py) - a.py;
        const double abz = static_cast<double>(b.pz) - a.pz;
        const double acx = static_cast<double>(c.px) - a.px;
        const double acy = static_cast<double>(c.py) - a.py;
        const double acz = static_cast<double>(c.pz) - a.pz;
        const double areaX = aby * acz - abz * acy;
        const double areaY = abz * acx - abx * acz;
        const double areaZ = abx * acy - aby * acx;
        const double areaLengthSquared = areaX * areaX + areaY * areaY + areaZ * areaZ;
        EXPECT_TRUE(std::isfinite(areaLengthSquared));
        EXPECT_GT(areaLengthSquared, 1.0e-16);
        edges.append(weldedEdgeKey(a, b));
        edges.append(weldedEdgeKey(b, c));
        edges.append(weldedEdgeKey(c, a));
    }
    std::sort(edges.begin(), edges.end());
    for (std::size_t begin = 0; begin < edges.size();) {
        std::size_t end = begin + 1u;
        while (end < edges.size() && edges[end] == edges[begin]) ++end;
        EXPECT_EQ(end - begin, 2u) << "A welded mesh edge must have two incident triangles";
        begin = end;
    }
}

} // namespace

TEST_F(Procedural3DTextExtrudeContractTest,
       GeneratesCenteredRenderableMeshWithRequestedDepth)
{
    const QDir sourceDirectory(
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    const QString bundledFontPath = sourceDirectory.filePath(QStringLiteral(
        "../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    ASSERT_TRUE(QFileInfo::exists(bundledFontPath));
    ASSERT_TRUE(ArtifactCore::FontManager::loadFontFromFile(bundledFontPath));

    ArtifactCore::TextExtrudeSettings settings;
    settings.text = QStringLiteral("3D TEXT");
    settings.fontFamily = QStringLiteral("Roboto");
    settings.fontSize = 96;
    settings.depth = 32.0f;
    settings.bevelWidth = 0.0f;

    const char* capturePath = std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_OBJ");
    const char* captureProfile = std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_PROFILE");
    const char* captureText = std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_TEXT");
    const char* captureFontPath = std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_PATH");
    const char* captureFontLicense =
        std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_LICENSE");
    const char* captureFontLicensePath =
        std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_LICENSE_PATH");
    const char* captureSourceCommit =
        std::getenv("ARTIFACT_TEXT_EXTRUDE_CAPTURE_SOURCE_COMMIT");
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
        } else if (profile == "hebrew-niqqud") {
            settings.text = QStringLiteral("בְּרֵאשִׁית בָּרָא אֱלֹהִים");
            settings.fontFamily = QStringLiteral("Tahoma");
        } else if (profile == "hebrew-mixed-bidi") {
            settings.text = QStringLiteral("דו״ח Q4 (2026): גרסה 2.5 — אושר!");
            settings.fontFamily = QStringLiteral("Tahoma");
        } else if (profile == "hebrew-isolates") {
            settings.text = QStringLiteral("\u2067עברית 123 ABC\u2069");
            settings.fontFamily = QStringLiteral("Tahoma");
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
        } else {
            FAIL() << "Unknown ARTIFACT_TEXT_EXTRUDE_CAPTURE_PROFILE: "
                   << profile;
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

    ArtifactCore::TextExtrudeDiagnostics diagnostics;
    const ArtifactCore::Procedural3DMeshData mesh =
        ArtifactCore::Procedural3DGenerators::generateTextExtrude(
            settings, 0.0f, &diagnostics);

    if (capturePath != nullptr && capturePath[0] != '\0') {
        ASSERT_TRUE(diagnostics.resolvedFontFamilies.contains(
            settings.fontFamily, Qt::CaseInsensitive))
            << "Requested capture font was not used by shaped glyph runs: "
            << settings.fontFamily.toStdString() << "; resolved: "
            << diagnostics.resolvedFontFamilies.join(QStringLiteral(", "))
                   .toStdString();
        ASSERT_EQ(diagnostics.missingGlyphCount, 0u)
            << "Refusing a geometry capture with missing glyphs: "
            << diagnostics.missingGlyphLocations.join(QStringLiteral(", ")).toStdString();
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
        capture.close();
        ASSERT_TRUE(capture.good()) << "Could not write text extrusion capture mesh: "
                                    << capturePath;

        const QString artifactPath = QString::fromLocal8Bit(capturePath);
        QFile artifactFile(artifactPath);
        ASSERT_TRUE(artifactFile.open(QIODevice::ReadOnly))
            << "Could not read back text extrusion capture mesh: " << capturePath;
        const QByteArray artifactBytes = artifactFile.readAll();
        ASSERT_FALSE(artifactBytes.isEmpty());
        const QString artifactSha256 = QString::fromLatin1(
            QCryptographicHash::hash(artifactBytes, QCryptographicHash::Sha256).toHex());

        QJsonArray fontFiles;
        QJsonArray fontHashes;
        QJsonArray unrecordedFields;
        QString sourceFontPath;
        QString fontSha256;
        QString fontLicense = QStringLiteral("unrecorded");
        QString fontLicensePath = QStringLiteral("unrecorded");
        QString fontLicenseSha256 = QStringLiteral("unrecorded");
        QString sourceCommit = QStringLiteral("unrecorded");
        if (captureFontLicense != nullptr && captureFontLicense[0] != '\0') {
            fontLicense = QString::fromLocal8Bit(captureFontLicense);
        }
        if (captureFontLicensePath != nullptr && captureFontLicensePath[0] != '\0') {
            fontLicensePath = QString::fromLocal8Bit(captureFontLicensePath);
            QFile licenseFile(fontLicensePath);
            ASSERT_TRUE(licenseFile.open(QIODevice::ReadOnly))
                << "Could not read capture font license file: "
                << captureFontLicensePath;
            const QByteArray licenseBytes = licenseFile.readAll();
            ASSERT_FALSE(licenseBytes.isEmpty())
                << "Capture font license file is empty: "
                << captureFontLicensePath;
            fontLicenseSha256 = QString::fromLatin1(QCryptographicHash::hash(
                licenseBytes, QCryptographicHash::Sha256).toHex());
        }
        if (captureSourceCommit != nullptr && captureSourceCommit[0] != '\0') {
            sourceCommit = QString::fromLocal8Bit(captureSourceCommit);
        }
        if (captureFontPath != nullptr && captureFontPath[0] != '\0') {
            sourceFontPath = QString::fromLocal8Bit(captureFontPath);
            QFile fontFile(sourceFontPath);
            ASSERT_TRUE(fontFile.open(QIODevice::ReadOnly))
                << "Could not read capture font file: " << captureFontPath;
            const QByteArray fontBytes = fontFile.readAll();
            ASSERT_FALSE(fontBytes.isEmpty())
                << "Capture font file is empty: " << captureFontPath;
            fontSha256 = QString::fromLatin1(QCryptographicHash::hash(
                fontBytes, QCryptographicHash::Sha256).toHex());
            QJsonObject fontFileMetadata{
                {QStringLiteral("path"), sourceFontPath},
                {QStringLiteral("sha256"), fontSha256},
                {QStringLiteral("license"), fontLicense},
                {QStringLiteral("licenseFile"), fontLicensePath},
                {QStringLiteral("licenseSha256"), fontLicenseSha256}};
            fontFiles.push_back(fontFileMetadata);
            fontHashes.push_back(fontSha256);
        } else {
            unrecordedFields.push_back(QStringLiteral("fontSourcePath"));
            unrecordedFields.push_back(QStringLiteral("fontFiles"));
            unrecordedFields.push_back(QStringLiteral("fontSha256"));
        }
        if (captureFontLicense == nullptr || captureFontLicense[0] == '\0') {
            unrecordedFields.push_back(QStringLiteral("fontLicense"));
        }
        if (captureFontLicensePath == nullptr || captureFontLicensePath[0] == '\0') {
            unrecordedFields.push_back(QStringLiteral("fontLicensePath"));
        }
        if (captureSourceCommit == nullptr || captureSourceCommit[0] == '\0') {
            unrecordedFields.push_back(QStringLiteral("sourceCommit"));
        }
        const QString sidecarPath = artifactPath + QStringLiteral(".json");
        QJsonObject metadata{
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("artifactType"), QStringLiteral("wavefront-obj-geometry")},
            {QStringLiteral("id"), captureProfile != nullptr
                 ? QString::fromLocal8Bit(captureProfile)
                 : artifactPath.fileName()},
            {QStringLiteral("inputText"), settings.text},
            {QStringLiteral("settings"), QJsonObject{
                 {QStringLiteral("fontFamily"), settings.fontFamily},
                 {QStringLiteral("fontSize"), settings.fontSize},
                 {QStringLiteral("bold"), settings.bold},
                 {QStringLiteral("italic"), settings.italic},
                 {QStringLiteral("letterSpacing"), settings.letterSpacing},
                 {QStringLiteral("lineSpacing"), settings.lineSpacing},
                 {QStringLiteral("alignment"), static_cast<int>(settings.alignment)},
                 {QStringLiteral("depth"), settings.depth},
                 {QStringLiteral("bevelWidth"), settings.bevelWidth},
                 {QStringLiteral("bevelSegments"), settings.bevelSegments},
                 {QStringLiteral("quality"), static_cast<int>(settings.quality)}}},
            {QStringLiteral("fontFamilies"), QJsonArray::fromStringList(
                 diagnostics.resolvedFontFamilies)},
            {QStringLiteral("fontFiles"), fontFiles},
            {QStringLiteral("fontLicense"), fontLicense},
            {QStringLiteral("fontSourcePath"), sourceFontPath.isEmpty()
                 ? QStringLiteral("unrecorded") : sourceFontPath},
            {QStringLiteral("fontLicensePath"), fontLicensePath},
            {QStringLiteral("fontLicenseSha256"), fontLicenseSha256},
            {QStringLiteral("fontSha256"), fontHashes},
            {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
            {QStringLiteral("os"), QSysInfo::prettyProductName()},
            {QStringLiteral("view"), QStringLiteral("geometry-only")},
            {QStringLiteral("resolution"), QStringLiteral("not-applicable")},
            {QStringLiteral("colorManagement"), QStringLiteral("not-applicable")},
            {QStringLiteral("captureDate"), QDateTime::currentDateTimeUtc()
                 .toString(Qt::ISODateWithMs)},
            {QStringLiteral("sourceCommit"), sourceCommit},
            {QStringLiteral("artifactSha256"), artifactSha256},
            {QStringLiteral("requestedFontFamily"), settings.fontFamily},
            {QStringLiteral("resolvedFontFingerprints"),
             QJsonArray::fromStringList(diagnostics.resolvedFontFingerprints)},
            {QStringLiteral("missingGlyphCount"),
             static_cast<int>(diagnostics.missingGlyphCount)},
            {QStringLiteral("unrecordedFields"), unrecordedFields}};
        QFile sidecar(sidecarPath);
        ASSERT_TRUE(sidecar.open(QIODevice::WriteOnly | QIODevice::Truncate))
            << "Could not open geometry capture metadata: "
            << sidecarPath.toStdString();
        const QByteArray metadataBytes = QJsonDocument(metadata).toJson(
            QJsonDocument::Indented);
        ASSERT_EQ(sidecar.write(metadataBytes), metadataBytes.size())
            << "Could not write geometry capture metadata: "
            << sidecarPath.toStdString();
    }

    ASSERT_FALSE(mesh.vertices.empty());
    ASSERT_GE(mesh.indices.size(), 3u);
    EXPECT_EQ(mesh.indices.size() % 3u, 0u);
    EXPECT_EQ(diagnostics.missingGlyphCount, 0u);
    EXPECT_TRUE(diagnostics.resolvedFontFamilies.contains(
        settings.fontFamily, Qt::CaseInsensitive));

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
    expectAdvancedExtrudedMesh(QStringLiteral("B8&"), QStringLiteral("Arial"));
}

#define TEST_ADVANCED_SHAPING_CASE(name, sample, family) \
    TEST_F(Procedural3DTextExtrudeContractTest, name) \
    { \
        expectAdvancedExtrudedMesh(QStringLiteral(sample), QStringLiteral(family)); \
    }

TEST_ADVANCED_SHAPING_CASE(ShapesJapaneseCjkText, "漢字仮名", "Noto Sans JP")
TEST_ADVANCED_SHAPING_CASE(ShapesArabicBasicText, "مرحبا بالعالم", "Tahoma")
TEST_ADVANCED_SHAPING_CASE(ShapesDevanagariBasicText, "नमस्ते दुनिया", "Nirmala UI")
TEST_ADVANCED_SHAPING_CASE(ShapesThaiText, "ภาษาไทย", "Leelawadee UI")
TEST_ADVANCED_SHAPING_CASE(ShapesHebrewBasicText, "שלום עולם", "Tahoma")
TEST_ADVANCED_SHAPING_CASE(ShapesHangulBasicText, "한글 테스트", "Malgun Gothic")
TEST_ADVANCED_SHAPING_CASE(ShapesMixedArabicText, "ABC — مَرْحَبًا بِالْعَالَمِينَ (123)", "Tahoma")
TEST_ADVANCED_SHAPING_CASE(ShapesHebrewNiqqud, "בְּרֵאשִׁית בָּרָא אֱלֹהִים", "Tahoma")
TEST_ADVANCED_SHAPING_CASE(ShapesHebrewMixedBidi, "דו״ח Q4 (2026): גרסה 2.5 — אושר!", "Tahoma")
TEST_ADVANCED_SHAPING_CASE(ShapesHebrewRtlIsolates, "\u2067עברית 123 ABC\u2069", "Tahoma")
TEST_ADVANCED_SHAPING_CASE(ShapesPersianZwnj, "می\u200Cخواهم فارسی بخوانم", "Tahoma")
TEST_ADVANCED_SHAPING_CASE(ShapesDevanagariConjuncts, "क्षेत्रज्ञ कर्मण्येवाधिकारस्ते", "Nirmala UI")
TEST_ADVANCED_SHAPING_CASE(ShapesBengaliConjuncts, "বাংলা ভাষা: ক্ষুদ্র", "Nirmala UI")
TEST_ADVANCED_SHAPING_CASE(ShapesTamilConjuncts, "தமிழ் மொழி: வணக்கம்", "Nirmala UI")
TEST_ADVANCED_SHAPING_CASE(ShapesGujaratiConjuncts, "ગુજરાતી ભાષા: ક્ષમતા", "Nirmala UI")
TEST_ADVANCED_SHAPING_CASE(ShapesTibetanText, "བོད་ཡིག་རིགས", "Microsoft Himalaya")
TEST_ADVANCED_SHAPING_CASE(ShapesEthiopicText, "እንደምን አላችሁ", "Ebrima")
TEST_ADVANCED_SHAPING_CASE(ShapesDecomposedHangulJamo, "한글 테스트", "Malgun Gothic")
TEST_ADVANCED_SHAPING_CASE(ShapesLatinLigaturesAndMarks, "office affinity ffi e\u0301", "Arial")
TEST_ADVANCED_SHAPING_CASE(ShapesDecomposedJapaneseDakuten, "か\u3099く\u3099", "Noto Sans JP")

#undef TEST_ADVANCED_SHAPING_CASE

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

TEST_F(Procedural3DTextExtrudeContractTest,
       CounterAndNestedCounterGlyphsProduceClosedManifoldExtrusions)
{
    const QDir sourceDirectory(
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    const QString bundledFontPath = sourceDirectory.filePath(QStringLiteral(
        "../../libs/openusd/pxr/usdImaging/usdviewq/fonts/Roboto/Roboto-Regular.ttf"));
    ASSERT_TRUE(QFileInfo::exists(bundledFontPath));
    ASSERT_TRUE(ArtifactCore::FontManager::loadFontFromFile(bundledFontPath));

    ArtifactCore::TextExtrudeSettings settings;
    settings.fontFamily = QStringLiteral("Roboto");
    settings.fontSize = 128;
    settings.depth = 32.0f;
    settings.bevelWidth = 0.0f;

    // O exercises one counter, B two nested counters, and 8/& stress
    // compound contours and overlapping outline components.
    for (const QString& text : {QStringLiteral("O"), QStringLiteral("B"),
                                QStringLiteral("8"), QStringLiteral("&")}) {
        SCOPED_TRACE(text.toStdString());
        settings.text = text;
        const auto mesh =
            ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings);

        ASSERT_FALSE(mesh.vertices.empty());
        ASSERT_GE(mesh.indices.size(), 3u);
        expectClosedTriangleMesh(mesh);
    }

    settings.text = QStringLiteral("O");
    settings.bevelWidth = 2.0f;
    settings.bevelSegments = 3;
    const auto beveledCounter =
        ArtifactCore::Procedural3DGenerators::generateTextExtrude(settings);
    ASSERT_FALSE(beveledCounter.vertices.empty());
    expectClosedTriangleMesh(beveledCounter);
}
