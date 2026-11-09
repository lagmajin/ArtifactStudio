#include <QColor>
#include <QString>
#include <QTemporaryDir>
#include <QVector3D>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <utility>

import Material.Material;
import Utils.String.UniString;

namespace {

using ArtifactCore::Material;
using ArtifactCore::MaterialAlphaMode;
using ArtifactCore::MaterialPresetKind;
using ArtifactCore::MaterialType;
using ArtifactCore::MaterialTextureUvTransform;

TEST(MaterialContractTest, StudioPresetsKeepTheirPbrCharacter)
{
    const Material matte = Material::makeStudioPreset(MaterialPresetKind::Matte);
    const Material metal = Material::makeStudioPreset(MaterialPresetKind::Metal);
    const Material plastic =
        Material::makeStudioPreset(MaterialPresetKind::Plastic);
    const Material glass = Material::makeStudioPreset(MaterialPresetKind::Glass);

    EXPECT_FLOAT_EQ(matte.metallic(), 0.0f);
    EXPECT_FLOAT_EQ(matte.roughness(), 0.82f);
    EXPECT_FLOAT_EQ(metal.metallic(), 0.9f);
    EXPECT_FLOAT_EQ(metal.roughness(), 0.24f);
    EXPECT_FLOAT_EQ(plastic.metallic(), 0.0f);
    EXPECT_FLOAT_EQ(plastic.roughness(), 0.3f);
    EXPECT_FLOAT_EQ(glass.metallic(), 0.0f);
    EXPECT_FLOAT_EQ(glass.roughness(), 0.08f);
    EXPECT_FLOAT_EQ(glass.transmission(), 0.82f);
}

TEST(MaterialContractTest, PbrScalarsClampOutOfRangeAndNonFiniteInputs)
{
    Material material;
    material.setMetallic(2.0f);
    material.setRoughness(-1.0f);
    material.setTransmission(std::numeric_limits<float>::quiet_NaN());
    material.setIOR(std::numeric_limits<float>::infinity());
    material.setClearcoat(1.5f);
    material.setAnisotropy(-2.0f,
                           std::numeric_limits<float>::quiet_NaN());

    EXPECT_FLOAT_EQ(material.metallic(), 1.0f);
    EXPECT_FLOAT_EQ(material.roughness(), 0.0f);
    EXPECT_FLOAT_EQ(material.transmission(), 0.0f);
    EXPECT_FLOAT_EQ(material.ior(), 1.5f);
    EXPECT_FLOAT_EQ(material.clearcoat(), 1.0f);
    EXPECT_FLOAT_EQ(material.anisotropy(), -1.0f);
    EXPECT_FLOAT_EQ(material.anisotropyRotationDegrees(), 0.0f);
}

TEST(MaterialContractTest, RemainingPbrScalarsUseStableFallbacks)
{
    Material material;
    material.setDiffuseRoughness(2.0f);
    material.setSpecular(-1.0f);
    material.setClearcoat(std::numeric_limits<float>::quiet_NaN());
    material.setClearcoatRoughness(std::numeric_limits<float>::infinity());
    material.setOpacity(-1.0f);
    material.setAlphaCutoff(2.0f);
    material.setNormalStrength(-2.0f);
    material.setOcclusionStrength(-1.0f);
    material.setEmissionStrength(-1.0f);

    EXPECT_FLOAT_EQ(material.diffuseRoughness(), 1.0f);
    EXPECT_FLOAT_EQ(material.specular(), 0.0f);
    EXPECT_FLOAT_EQ(material.clearcoat(), 0.0f);
    EXPECT_FLOAT_EQ(material.clearcoatRoughness(), 0.03f);
    EXPECT_FLOAT_EQ(material.opacity(), 0.0f);
    EXPECT_FLOAT_EQ(material.alphaCutoff(), 1.0f);
    EXPECT_FLOAT_EQ(material.normalStrength(), 0.0f);
    EXPECT_FLOAT_EQ(material.occlusionStrength(), 0.0f);
    EXPECT_FLOAT_EQ(material.emissionStrength(), 0.0f);

    material.setDiffuseRoughness(std::numeric_limits<float>::quiet_NaN());
    material.setSpecular(std::numeric_limits<float>::infinity());
    material.setOpacity(std::numeric_limits<float>::quiet_NaN());
    material.setAlphaCutoff(std::numeric_limits<float>::infinity());
    material.setNormalStrength(std::numeric_limits<float>::quiet_NaN());
    material.setOcclusionStrength(std::numeric_limits<float>::quiet_NaN());
    material.setEmissionStrength(std::numeric_limits<float>::infinity());

    EXPECT_FLOAT_EQ(material.diffuseRoughness(), 0.0f);
    EXPECT_FLOAT_EQ(material.specular(), 0.5f);
    EXPECT_FLOAT_EQ(material.opacity(), 1.0f);
    EXPECT_FLOAT_EQ(material.alphaCutoff(), 0.5f);
    EXPECT_FLOAT_EQ(material.normalStrength(), 1.0f);
    EXPECT_FLOAT_EQ(material.occlusionStrength(), 1.0f);
    EXPECT_FLOAT_EQ(material.emissionStrength(), 0.0f);
}

TEST(MaterialContractTest, AlphaAndTransmissionControlsRemainIndependent)
{
    Material material;
    material.setOpacity(0.35f);
    material.setAlphaMode(MaterialAlphaMode::Masked);
    material.setAlphaCutoff(0.72f);
    material.setTransmission(0.8f);
    material.setThinWall(true);
    material.setThinFilm(420.0f, 1.8f);

    EXPECT_FLOAT_EQ(material.opacity(), 0.35f);
    EXPECT_EQ(material.alphaMode(), MaterialAlphaMode::Masked);
    EXPECT_FLOAT_EQ(material.alphaCutoff(), 0.72f);
    EXPECT_FLOAT_EQ(material.transmission(), 0.8f);
    EXPECT_TRUE(material.thinWall());
    EXPECT_FLOAT_EQ(material.thinFilmThickness(), 420.0f);
    EXPECT_FLOAT_EQ(material.thinFilmIOR(), 1.8f);
}

TEST(MaterialContractTest, ThinFilmParametersClampAndUseStableFallbacks)
{
    Material material;

    material.setThinFilm(-20.0f, 0.5f);
    EXPECT_FLOAT_EQ(material.thinFilmThickness(), 0.0f);
    EXPECT_FLOAT_EQ(material.thinFilmIOR(), 1.0f);

    material.setThinFilm(1200.0f, 3.0f);
    EXPECT_FLOAT_EQ(material.thinFilmThickness(), 1000.0f);
    EXPECT_FLOAT_EQ(material.thinFilmIOR(), 2.5f);

    material.setThinFilm(std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(material.thinFilmThickness(), 0.0f);
    EXPECT_FLOAT_EQ(material.thinFilmIOR(), 1.33f);
}

TEST(MaterialContractTest, PrincipledShadingControlsClampAndFallback)
{
    Material material;
    material.setClearcoatIOR(0.5f);
    material.setClearcoatNormalStrength(2.0f);
    material.setSheen(-0.25f);
    material.setSheenRoughness(std::numeric_limits<float>::quiet_NaN());
    material.setSubsurface(2.0f);
    material.setSubsurfaceIOR(std::numeric_limits<float>::infinity());
    material.setSubsurfaceAnisotropy(-2.0f);
    material.setSubsurfaceRadius(QVector3D(
        -1.0f, 20.0f, std::numeric_limits<float>::quiet_NaN()));
    material.setEnvironmentIntensity(8.0f);

    EXPECT_FLOAT_EQ(material.clearcoatIOR(), 1.0f);
    EXPECT_FLOAT_EQ(material.clearcoatNormalStrength(), 1.0f);
    EXPECT_FLOAT_EQ(material.sheen(), 0.0f);
    EXPECT_FLOAT_EQ(material.sheenRoughness(), 0.5f);
    EXPECT_FLOAT_EQ(material.subsurface(), 1.0f);
    EXPECT_FLOAT_EQ(material.subsurfaceIOR(), 1.4f);
    EXPECT_FLOAT_EQ(material.subsurfaceAnisotropy(), -0.9f);
    EXPECT_EQ(material.subsurfaceRadius(), QVector3D(0.0f, 10.0f, 0.1f));
    EXPECT_FLOAT_EQ(material.environmentIntensity(), 4.0f);

    material.setEnvironmentIntensity(
        std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(material.environmentIntensity(), 1.0f);
}

TEST(MaterialContractTest, ColorAndEmissionInputsUseStableFallbacks)
{
    Material material;
    material.setBaseColor(QColor{});
    material.setEmissionColor(QColor{});
    material.setEmissionStrength(-1.0f);

    EXPECT_EQ(material.baseColor(), QColor(255, 255, 255));
    EXPECT_EQ(material.emissionColor(), QColor(0, 0, 0));
    EXPECT_FLOAT_EQ(material.emissionStrength(), 0.0f);

    material.setEmissionColor(QColor(20, 80, 160));
    material.setEmissionStrength(4.5f);
    EXPECT_EQ(material.emissionColor(), QColor(20, 80, 160));
    EXPECT_FLOAT_EQ(material.emissionStrength(), 4.5f);
}

TEST(MaterialContractTest, MaterialGraphJsonPresenceIgnoresWhitespaceOnlyText)
{
    Material material;
    EXPECT_FALSE(material.hasMaterialGraphJson());

    material.setMaterialGraphJson(
        ArtifactCore::UniString(QStringLiteral(" \n\t ")));
    EXPECT_FALSE(material.hasMaterialGraphJson());
    EXPECT_EQ(material.materialGraphJson().toQString(),
              QStringLiteral(" \n\t "));

    const QString graph = QStringLiteral(
        "{\"nodes\":[{\"type\":\"principled\"}],\"links\":[]}");
    material.setMaterialGraphJson(ArtifactCore::UniString(graph));
    EXPECT_TRUE(material.hasMaterialGraphJson());
    EXPECT_EQ(material.materialGraphJson().toQString(), graph);

    material.setMaterialGraphJson(ArtifactCore::UniString{});
    EXPECT_FALSE(material.hasMaterialGraphJson());
}

TEST(MaterialContractTest, MaterialXDocumentRoundTripsUtf8AndSetsMaterialType)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString document = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<materialx version=\"1.38\"><material name=\"陶器\"/></materialx>");
    const QString path = temporaryDirectory.filePath(
        QStringLiteral("test-material.mtlx"));
    Material source;
    source.setMaterialXDocument(ArtifactCore::UniString(document));

    EXPECT_FALSE(source.saveMaterialXDocument(QString{}));
    ASSERT_TRUE(source.saveMaterialXDocument(path));

    Material loaded;
    EXPECT_FALSE(loaded.loadMaterialXDocument(QString{}));
    EXPECT_FALSE(loaded.loadMaterialXDocument(
        temporaryDirectory.filePath(QStringLiteral("missing.mtlx"))));
    ASSERT_TRUE(loaded.loadMaterialXDocument(path));
    EXPECT_EQ(loaded.materialXDocument().toQString(), document);
    EXPECT_EQ(loaded.type(), MaterialType::MaterialX);
}

TEST(MaterialContractTest, TextureUvTransformsAreIndependentAndSanitized)
{
    Material material;
    const MaterialTextureUvTransform anisotropyInput{
        -2.0f,
        std::numeric_limits<float>::infinity(),
        450.0f,
        std::numeric_limits<float>::quiet_NaN(),
        2000000.0f,
        1,
    };
    const MaterialTextureUvTransform sheenInput{
        2.0f,
        3.0f,
        -90.0f,
        17.0f,
        -23.0f,
        0,
    };

    material.setAnisotropyTextureUvTransform(anisotropyInput);
    material.setSheenColorTextureUvTransform(sheenInput);

    const auto anisotropy = material.anisotropyTextureUvTransform();
    EXPECT_FLOAT_EQ(anisotropy.scaleU, 0.0001f);
    EXPECT_FLOAT_EQ(anisotropy.scaleV, 1.0f);
    EXPECT_FLOAT_EQ(anisotropy.rotationDegrees, 360.0f);
    EXPECT_FLOAT_EQ(anisotropy.offsetU, 0.0f);
    EXPECT_FLOAT_EQ(anisotropy.offsetV, 1000000.0f);
    EXPECT_EQ(anisotropy.uvSet, 1);

    const auto sheen = material.sheenColorTextureUvTransform();
    EXPECT_FLOAT_EQ(sheen.scaleU, 2.0f);
    EXPECT_FLOAT_EQ(sheen.scaleV, 3.0f);
    EXPECT_FLOAT_EQ(sheen.rotationDegrees, -90.0f);
    EXPECT_FLOAT_EQ(sheen.offsetU, 17.0f);
    EXPECT_FLOAT_EQ(sheen.offsetV, -23.0f);
    EXPECT_EQ(sheen.uvSet, 0);
}

TEST(MaterialContractTest, TextureMapPathsAndPresenceFlagsRemainIndependent)
{
    Material material;
    material.setBaseColorTexture(
        ArtifactCore::UniString(QStringLiteral("maps/base-color.png")));
    material.setMetallicRoughnessTexture(
        ArtifactCore::UniString(QStringLiteral("maps/packed-mr.png")));
    material.setMetallicTexture(
        ArtifactCore::UniString(QStringLiteral("maps/metallic.png")));
    material.setRoughnessTexture(
        ArtifactCore::UniString(QStringLiteral("maps/roughness.png")));
    material.setNormalTexture(
        ArtifactCore::UniString(QStringLiteral("maps/normal.png")));
    material.setOcclusionTexture(
        ArtifactCore::UniString(QStringLiteral("maps/occlusion.png")));
    material.setEmissionTexture(
        ArtifactCore::UniString(QStringLiteral("maps/emission.png")));
    material.setOpacityTexture(
        ArtifactCore::UniString(QStringLiteral("maps/opacity.png")));
    material.setClearcoatTexture(
        ArtifactCore::UniString(QStringLiteral("maps/clearcoat.png")));

    EXPECT_TRUE(material.hasBaseColorTexture());
    EXPECT_TRUE(material.hasMetallicRoughnessTexture());
    EXPECT_TRUE(material.hasMetallicTexture());
    EXPECT_TRUE(material.hasRoughnessTexture());
    EXPECT_TRUE(material.hasNormalTexture());
    EXPECT_TRUE(material.hasOcclusionTexture());
    EXPECT_TRUE(material.hasEmissionTexture());
    EXPECT_TRUE(material.hasOpacityTexture());
    EXPECT_TRUE(material.hasClearcoatTexture());
    EXPECT_EQ(material.baseColorTexture().toQString(),
              QStringLiteral("maps/base-color.png"));
    EXPECT_EQ(material.metallicRoughnessTexture().toQString(),
              QStringLiteral("maps/packed-mr.png"));
    EXPECT_EQ(material.metallicTexture().toQString(),
              QStringLiteral("maps/metallic.png"));
    EXPECT_EQ(material.roughnessTexture().toQString(),
              QStringLiteral("maps/roughness.png"));
    EXPECT_EQ(material.normalTexture().toQString(),
              QStringLiteral("maps/normal.png"));
    EXPECT_EQ(material.occlusionTexture().toQString(),
              QStringLiteral("maps/occlusion.png"));
    EXPECT_EQ(material.emissionTexture().toQString(),
              QStringLiteral("maps/emission.png"));
    EXPECT_EQ(material.opacityTexture().toQString(),
              QStringLiteral("maps/opacity.png"));
    EXPECT_EQ(material.clearcoatTexture().toQString(),
              QStringLiteral("maps/clearcoat.png"));

    material.setMetallicTexture(ArtifactCore::UniString(QStringLiteral("  ")));
    EXPECT_FALSE(material.hasMetallicTexture());
    EXPECT_TRUE(material.hasMetallicRoughnessTexture());
    EXPECT_TRUE(material.hasRoughnessTexture());

    const ArtifactCore::UniString whitespacePath(QStringLiteral(" \t "));
    material.setBaseColorTexture(whitespacePath);
    material.setNormalTexture(whitespacePath);
    material.setOcclusionTexture(whitespacePath);
    material.setEmissionTexture(whitespacePath);
    material.setOpacityTexture(whitespacePath);
    material.setClearcoatTexture(whitespacePath);
    EXPECT_FALSE(material.hasBaseColorTexture());
    EXPECT_FALSE(material.hasNormalTexture());
    EXPECT_FALSE(material.hasOcclusionTexture());
    EXPECT_FALSE(material.hasEmissionTexture());
    EXPECT_FALSE(material.hasOpacityTexture());
    EXPECT_FALSE(material.hasClearcoatTexture());
    EXPECT_EQ(material.baseColorTexture().toQString(),
              whitespacePath.toQString());
    EXPECT_TRUE(material.hasMetallicRoughnessTexture());
    EXPECT_TRUE(material.hasRoughnessTexture());
}

TEST(MaterialContractTest, IndependentMetallicAndRoughnessMapsKeepTheirTransforms)
{
    Material material;
    material.setMetallicTextureUvTransform(
        MaterialTextureUvTransform{2.0f, 0.5f, 45.0f, 1.0f, -2.0f, 1});
    material.setRoughnessTextureUvTransform(
        MaterialTextureUvTransform{0.25f, 3.0f, -90.0f, -4.0f, 5.0f, 0});

    const auto metallic = material.metallicTextureUvTransform();
    EXPECT_FLOAT_EQ(metallic.scaleU, 2.0f);
    EXPECT_FLOAT_EQ(metallic.scaleV, 0.5f);
    EXPECT_FLOAT_EQ(metallic.rotationDegrees, 45.0f);
    EXPECT_FLOAT_EQ(metallic.offsetU, 1.0f);
    EXPECT_FLOAT_EQ(metallic.offsetV, -2.0f);
    EXPECT_EQ(metallic.uvSet, 1);

    const auto roughness = material.roughnessTextureUvTransform();
    EXPECT_FLOAT_EQ(roughness.scaleU, 0.25f);
    EXPECT_FLOAT_EQ(roughness.scaleV, 3.0f);
    EXPECT_FLOAT_EQ(roughness.rotationDegrees, -90.0f);
    EXPECT_FLOAT_EQ(roughness.offsetU, -4.0f);
    EXPECT_FLOAT_EQ(roughness.offsetV, 5.0f);
    EXPECT_EQ(roughness.uvSet, 0);

    material.setMetallicTextureUvTransform(MaterialTextureUvTransform{
        std::numeric_limits<float>::quiet_NaN(), -1.0f,
        std::numeric_limits<float>::infinity(), 20.0f, -20.0f, 2});
    const auto sanitizedMetallic = material.metallicTextureUvTransform();
    EXPECT_FLOAT_EQ(sanitizedMetallic.scaleU, 1.0f);
    EXPECT_FLOAT_EQ(sanitizedMetallic.scaleV, 0.01f);
    EXPECT_FLOAT_EQ(sanitizedMetallic.rotationDegrees, 0.0f);
    EXPECT_FLOAT_EQ(sanitizedMetallic.offsetU, 10.0f);
    EXPECT_FLOAT_EQ(sanitizedMetallic.offsetV, -10.0f);
    EXPECT_EQ(sanitizedMetallic.uvSet, 0);
}

TEST(MaterialContractTest, TextureUvSetSelectorsStayPerMapAndLimitToUv0OrUv1)
{
    Material material;
    material.setBaseColorUvSet(1);
    material.setMetallicRoughnessUvSet(0);
    material.setNormalUvSet(1);
    material.setOcclusionTextureUvSet(0);
    material.setEmissionTextureUvSet(1);
    material.setOpacityTextureUvSet(0);
    material.setClearcoatTextureUvSet(1);
    material.setClearcoatRoughnessTextureUvSet(0);
    material.setClearcoatNormalUvSet(1);

    EXPECT_EQ(material.baseColorUvSet(), 1);
    EXPECT_EQ(material.metallicRoughnessUvSet(), 0);
    EXPECT_EQ(material.normalUvSet(), 1);
    EXPECT_EQ(material.occlusionTextureUvSet(), 0);
    EXPECT_EQ(material.emissionTextureUvSet(), 1);
    EXPECT_EQ(material.opacityTextureUvSet(), 0);
    EXPECT_EQ(material.clearcoatTextureUvSet(), 1);
    EXPECT_EQ(material.clearcoatRoughnessTextureUvSet(), 0);
    EXPECT_EQ(material.clearcoatNormalUvSet(), 1);

    material.setBaseColorUvSet(2);
    material.setNormalUvSet(-1);
    material.setClearcoatTextureUvSet(7);

    EXPECT_EQ(material.baseColorUvSet(), 0);
    EXPECT_EQ(material.normalUvSet(), 0);
    EXPECT_EQ(material.clearcoatTextureUvSet(), 0);
}

TEST(MaterialContractTest, ScalarTextureUvTransformsRemainIndependent)
{
    Material material;
    material.setBaseColorUvScale(2.0f);
    material.setBaseColorUvOffsetU(0.1f);
    material.setBaseColorUvOffsetV(0.2f);
    material.setBaseColorUvRotationDegrees(10.0f);
    material.setBaseColorUvSet(1);
    material.setMetallicRoughnessUvScale(3.0f);
    material.setMetallicRoughnessUvOffsetU(0.3f);
    material.setMetallicRoughnessUvOffsetV(0.4f);
    material.setMetallicRoughnessUvRotationDegrees(20.0f);
    material.setMetallicRoughnessUvSet(0);
    material.setNormalUvScale(4.0f);
    material.setNormalUvOffsetU(0.5f);
    material.setNormalUvOffsetV(0.6f);
    material.setNormalUvRotationDegrees(30.0f);
    material.setNormalUvSet(1);
    material.setOcclusionTextureUvScale(5.0f);
    material.setOcclusionTextureUvOffsetU(0.7f);
    material.setOcclusionTextureUvOffsetV(0.8f);
    material.setOcclusionTextureUvRotationDegrees(40.0f);
    material.setOcclusionTextureUvSet(0);
    material.setEmissionTextureUvScale(6.0f);
    material.setEmissionTextureUvOffsetU(0.9f);
    material.setEmissionTextureUvOffsetV(1.0f);
    material.setEmissionTextureUvRotationDegrees(50.0f);
    material.setEmissionTextureUvSet(1);
    material.setOpacityTextureUvScale(7.0f);
    material.setOpacityTextureUvOffsetU(1.1f);
    material.setOpacityTextureUvOffsetV(1.2f);
    material.setOpacityTextureUvRotationDegrees(60.0f);
    material.setOpacityTextureUvSet(0);
    material.setClearcoatTextureUvScale(8.0f);
    material.setClearcoatTextureUvOffsetU(1.3f);
    material.setClearcoatTextureUvOffsetV(1.4f);
    material.setClearcoatTextureUvRotationDegrees(70.0f);
    material.setClearcoatTextureUvSet(1);
    material.setClearcoatRoughnessTextureUvScale(9.0f);
    material.setClearcoatRoughnessTextureUvOffsetU(1.5f);
    material.setClearcoatRoughnessTextureUvOffsetV(1.6f);
    material.setClearcoatRoughnessTextureUvRotationDegrees(80.0f);
    material.setClearcoatRoughnessTextureUvSet(0);
    material.setClearcoatNormalUvScale(10.0f);
    material.setClearcoatNormalUvOffsetU(1.7f);
    material.setClearcoatNormalUvOffsetV(1.8f);
    material.setClearcoatNormalUvRotationDegrees(90.0f);
    material.setClearcoatNormalUvSet(1);

    struct UvSnapshot {
        const char* name;
        float scale;
        float offsetU;
        float offsetV;
        float rotationDegrees;
        int uvSet;
    };
    const std::array<UvSnapshot, 9> expected{{
        {"base color", 2.0f, 0.1f, 0.2f, 10.0f, 1},
        {"metallic-roughness", 3.0f, 0.3f, 0.4f, 20.0f, 0},
        {"normal", 4.0f, 0.5f, 0.6f, 30.0f, 1},
        {"occlusion", 5.0f, 0.7f, 0.8f, 40.0f, 0},
        {"emission", 6.0f, 0.9f, 1.0f, 50.0f, 1},
        {"opacity", 7.0f, 1.1f, 1.2f, 60.0f, 0},
        {"clearcoat", 8.0f, 1.3f, 1.4f, 70.0f, 1},
        {"clearcoat roughness", 9.0f, 1.5f, 1.6f, 80.0f, 0},
        {"clearcoat normal", 10.0f, 1.7f, 1.8f, 90.0f, 1},
    }};
    const std::array<UvSnapshot, 9> actual{{
        {"base color", material.baseColorUvScale(),
         material.baseColorUvOffsetU(), material.baseColorUvOffsetV(),
         material.baseColorUvRotationDegrees(), material.baseColorUvSet()},
        {"metallic-roughness", material.metallicRoughnessUvScale(),
         material.metallicRoughnessUvOffsetU(),
         material.metallicRoughnessUvOffsetV(),
         material.metallicRoughnessUvRotationDegrees(),
         material.metallicRoughnessUvSet()},
        {"normal", material.normalUvScale(), material.normalUvOffsetU(),
         material.normalUvOffsetV(), material.normalUvRotationDegrees(),
         material.normalUvSet()},
        {"occlusion", material.occlusionTextureUvScale(),
         material.occlusionTextureUvOffsetU(),
         material.occlusionTextureUvOffsetV(),
         material.occlusionTextureUvRotationDegrees(),
         material.occlusionTextureUvSet()},
        {"emission", material.emissionTextureUvScale(),
         material.emissionTextureUvOffsetU(),
         material.emissionTextureUvOffsetV(),
         material.emissionTextureUvRotationDegrees(),
         material.emissionTextureUvSet()},
        {"opacity", material.opacityTextureUvScale(),
         material.opacityTextureUvOffsetU(), material.opacityTextureUvOffsetV(),
         material.opacityTextureUvRotationDegrees(),
         material.opacityTextureUvSet()},
        {"clearcoat", material.clearcoatTextureUvScale(),
         material.clearcoatTextureUvOffsetU(),
         material.clearcoatTextureUvOffsetV(),
         material.clearcoatTextureUvRotationDegrees(),
         material.clearcoatTextureUvSet()},
        {"clearcoat roughness", material.clearcoatRoughnessTextureUvScale(),
         material.clearcoatRoughnessTextureUvOffsetU(),
         material.clearcoatRoughnessTextureUvOffsetV(),
         material.clearcoatRoughnessTextureUvRotationDegrees(),
         material.clearcoatRoughnessTextureUvSet()},
        {"clearcoat normal", material.clearcoatNormalUvScale(),
         material.clearcoatNormalUvOffsetU(),
         material.clearcoatNormalUvOffsetV(),
         material.clearcoatNormalUvRotationDegrees(),
         material.clearcoatNormalUvSet()},
    }};

    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_FLOAT_EQ(actual[index].scale, expected[index].scale)
            << actual[index].name << " scale";
        EXPECT_FLOAT_EQ(actual[index].offsetU, expected[index].offsetU)
            << actual[index].name << " U offset";
        EXPECT_FLOAT_EQ(actual[index].offsetV, expected[index].offsetV)
            << actual[index].name << " V offset";
        EXPECT_FLOAT_EQ(actual[index].rotationDegrees,
                        expected[index].rotationDegrees)
            << actual[index].name << " rotation";
        EXPECT_EQ(actual[index].uvSet, expected[index].uvSet)
            << actual[index].name << " UV set";
    }
}

TEST(MaterialContractTest, CopyConstructionAndAssignmentOwnIndependentState)
{
    Material source;
    source.setBaseColor(QColor(24, 96, 192));
    source.setBaseColorTexture(
        ArtifactCore::UniString(QStringLiteral("maps/base-color.png")));
    source.setMetallic(0.75f);
    source.setRoughness(0.2f);
    source.setAlphaMode(MaterialAlphaMode::Blended);
    source.setThinWall(true);
    source.setAnisotropyTexture(
        ArtifactCore::UniString(QStringLiteral("maps/anisotropy.png")));
    source.setAnisotropyTextureUvTransform(
        MaterialTextureUvTransform{2.0f, 0.5f, 45.0f, 0.1f, 0.2f, 1});
    source.setMetallicTexture(
        ArtifactCore::UniString(QStringLiteral("maps/metallic.png")));
    source.setMetallicTextureUvTransform(
        MaterialTextureUvTransform{1.5f, 0.75f, 30.0f, 0.2f, -0.3f, 1});
    source.setRoughnessTexture(
        ArtifactCore::UniString(QStringLiteral("maps/roughness.png")));
    source.setRoughnessTextureUvTransform(
        MaterialTextureUvTransform{0.5f, 2.0f, -15.0f, -0.4f, 0.6f, 0});

    Material copy(source);
    source.setBaseColor(QColor(200, 40, 20));
    source.setBaseColorTexture(ArtifactCore::UniString{});
    source.setMetallic(0.1f);
    source.setAlphaMode(MaterialAlphaMode::Opaque);
    source.setThinWall(false);
    source.setAnisotropyTexture(ArtifactCore::UniString{});
    source.setAnisotropyTextureUvTransform(MaterialTextureUvTransform{});
    source.setMetallicTexture(ArtifactCore::UniString{});
    source.setMetallicTextureUvTransform(MaterialTextureUvTransform{});
    source.setRoughnessTexture(ArtifactCore::UniString{});
    source.setRoughnessTextureUvTransform(MaterialTextureUvTransform{});

    EXPECT_EQ(copy.baseColor(), QColor(24, 96, 192));
    EXPECT_TRUE(copy.hasBaseColorTexture());
    EXPECT_EQ(copy.baseColorTexture().toQString(),
              QStringLiteral("maps/base-color.png"));
    EXPECT_FLOAT_EQ(copy.metallic(), 0.75f);
    EXPECT_FLOAT_EQ(copy.roughness(), 0.2f);
    EXPECT_EQ(copy.alphaMode(), MaterialAlphaMode::Blended);
    EXPECT_TRUE(copy.thinWall());
    EXPECT_EQ(copy.anisotropyTexture().toQString(),
              QStringLiteral("maps/anisotropy.png"));
    EXPECT_FLOAT_EQ(copy.anisotropyTextureUvTransform().scaleU, 2.0f);
    EXPECT_FLOAT_EQ(copy.anisotropyTextureUvTransform().scaleV, 0.5f);
    EXPECT_EQ(copy.anisotropyTextureUvTransform().uvSet, 1);
    EXPECT_EQ(copy.metallicTexture().toQString(),
              QStringLiteral("maps/metallic.png"));
    EXPECT_FLOAT_EQ(copy.metallicTextureUvTransform().scaleU, 1.5f);
    EXPECT_FLOAT_EQ(copy.metallicTextureUvTransform().rotationDegrees, 30.0f);
    EXPECT_FLOAT_EQ(copy.metallicTextureUvTransform().offsetV, -0.3f);
    EXPECT_EQ(copy.metallicTextureUvTransform().uvSet, 1);
    EXPECT_EQ(copy.roughnessTexture().toQString(),
              QStringLiteral("maps/roughness.png"));
    EXPECT_FLOAT_EQ(copy.roughnessTextureUvTransform().scaleV, 2.0f);
    EXPECT_FLOAT_EQ(copy.roughnessTextureUvTransform().rotationDegrees, -15.0f);
    EXPECT_FLOAT_EQ(copy.roughnessTextureUvTransform().offsetU, -0.4f);
    EXPECT_EQ(copy.roughnessTextureUvTransform().uvSet, 0);

    Material assigned;
    assigned = copy;
    copy.setRoughness(0.9f);
    copy.setAnisotropyTexture(
        ArtifactCore::UniString(QStringLiteral("maps/other-anisotropy.png")));

    EXPECT_EQ(assigned.baseColor(), QColor(24, 96, 192));
    EXPECT_TRUE(assigned.hasBaseColorTexture());
    EXPECT_FLOAT_EQ(assigned.metallic(), 0.75f);
    EXPECT_FLOAT_EQ(assigned.roughness(), 0.2f);
    EXPECT_EQ(assigned.alphaMode(), MaterialAlphaMode::Blended);
    EXPECT_TRUE(assigned.thinWall());
    EXPECT_EQ(assigned.anisotropyTexture().toQString(),
              QStringLiteral("maps/anisotropy.png"));
    EXPECT_FLOAT_EQ(assigned.anisotropyTextureUvTransform().rotationDegrees,
                    45.0f);
    EXPECT_EQ(assigned.metallicTexture().toQString(),
              QStringLiteral("maps/metallic.png"));
    EXPECT_FLOAT_EQ(assigned.metallicTextureUvTransform().scaleV, 0.75f);
    EXPECT_FLOAT_EQ(assigned.metallicTextureUvTransform().offsetU, 0.2f);
    EXPECT_EQ(assigned.roughnessTexture().toQString(),
              QStringLiteral("maps/roughness.png"));
    EXPECT_FLOAT_EQ(assigned.roughnessTextureUvTransform().scaleU, 0.5f);
    EXPECT_FLOAT_EQ(assigned.roughnessTextureUvTransform().offsetV, 0.6f);
}

TEST(MaterialContractTest, AdvancedPbrStateCopiesAcrossConstructionAndAssignment)
{
    Material source;
    source.setThinFilm(520.0f, 2.1f);
    source.setIOR(1.7f);
    source.setTransmission(0.8f);
    source.setThinWall(true);
    source.setClearcoat(0.75f);
    source.setClearcoatRoughness(0.35f);
    source.setClearcoatIOR(1.8f);
    source.setClearcoatNormalStrength(0.6f);
    source.setClearcoatTexture(
        ArtifactCore::UniString(QStringLiteral("maps/coat.png")));
    source.setClearcoatRoughnessTexture(
        ArtifactCore::UniString(QStringLiteral("maps/coat-roughness.png")));
    source.setClearcoatNormalTexture(
        ArtifactCore::UniString(QStringLiteral("maps/coat-normal.png")));
    source.setSheen(0.65f);
    source.setSheenRoughness(0.25f);
    source.setSheenTint(QColor(180, 120, 80));
    source.setSheenColorTexture(
        ArtifactCore::UniString(QStringLiteral("maps/sheen-color.png")));
    source.setSheenColorTextureUvTransform(
        MaterialTextureUvTransform{2.0f, 0.5f, 35.0f, 0.1f, 0.2f, 1});
    source.setSheenRoughnessTexture(
        ArtifactCore::UniString(QStringLiteral("maps/sheen-roughness.png")));
    source.setSheenRoughnessTextureUvTransform(
        MaterialTextureUvTransform{0.75f, 1.5f, -25.0f, -0.3f, 0.4f, 0});
    source.setSubsurface(0.4f);
    source.setSubsurfaceIOR(1.6f);
    source.setSubsurfaceAnisotropy(-0.3f);
    source.setSubsurfaceColor(QColor(210, 90, 60));
    source.setSubsurfaceRadius(QVector3D(2.0f, 0.8f, 0.2f));
    source.setNormalTexture(
        ArtifactCore::UniString(QStringLiteral("maps/normal.png")));
    source.setOcclusionTexture(
        ArtifactCore::UniString(QStringLiteral("maps/occlusion.png")));
    source.setOcclusionStrength(0.45f);
    source.setEmissionTexture(
        ArtifactCore::UniString(QStringLiteral("maps/emission.png")));
    source.setEmissionStrength(2.5f);
    source.setOpacityTexture(
        ArtifactCore::UniString(QStringLiteral("maps/opacity.png")));
    source.setOpacity(0.7f);
    source.setAlphaMode(MaterialAlphaMode::Masked);
    source.setAlphaCutoff(0.42f);

    Material copy(source);
    source = Material{};

    EXPECT_FLOAT_EQ(copy.thinFilmThickness(), 520.0f);
    EXPECT_FLOAT_EQ(copy.thinFilmIOR(), 2.1f);
    EXPECT_FLOAT_EQ(copy.ior(), 1.7f);
    EXPECT_FLOAT_EQ(copy.transmission(), 0.8f);
    EXPECT_TRUE(copy.thinWall());
    EXPECT_FLOAT_EQ(copy.clearcoat(), 0.75f);
    EXPECT_FLOAT_EQ(copy.clearcoatRoughness(), 0.35f);
    EXPECT_FLOAT_EQ(copy.clearcoatIOR(), 1.8f);
    EXPECT_FLOAT_EQ(copy.clearcoatNormalStrength(), 0.6f);
    EXPECT_EQ(copy.clearcoatTexture().toQString(),
              QStringLiteral("maps/coat.png"));
    EXPECT_EQ(copy.clearcoatRoughnessTexture().toQString(),
              QStringLiteral("maps/coat-roughness.png"));
    EXPECT_EQ(copy.clearcoatNormalTexture().toQString(),
              QStringLiteral("maps/coat-normal.png"));
    EXPECT_FLOAT_EQ(copy.sheen(), 0.65f);
    EXPECT_FLOAT_EQ(copy.sheenRoughness(), 0.25f);
    EXPECT_EQ(copy.sheenTint(), QColor(180, 120, 80));
    EXPECT_EQ(copy.sheenColorTexture().toQString(),
              QStringLiteral("maps/sheen-color.png"));
    EXPECT_FLOAT_EQ(copy.sheenColorTextureUvTransform().scaleU, 2.0f);
    EXPECT_EQ(copy.sheenColorTextureUvTransform().uvSet, 1);
    EXPECT_EQ(copy.sheenRoughnessTexture().toQString(),
              QStringLiteral("maps/sheen-roughness.png"));
    EXPECT_FLOAT_EQ(copy.sheenRoughnessTextureUvTransform().rotationDegrees,
                    -25.0f);
    EXPECT_EQ(copy.sheenRoughnessTextureUvTransform().uvSet, 0);
    EXPECT_FLOAT_EQ(copy.subsurface(), 0.4f);
    EXPECT_FLOAT_EQ(copy.subsurfaceIOR(), 1.6f);
    EXPECT_FLOAT_EQ(copy.subsurfaceAnisotropy(), -0.3f);
    EXPECT_EQ(copy.subsurfaceColor(), QColor(210, 90, 60));
    EXPECT_EQ(copy.subsurfaceRadius(), QVector3D(2.0f, 0.8f, 0.2f));
    EXPECT_EQ(copy.normalTexture().toQString(), QStringLiteral("maps/normal.png"));
    EXPECT_EQ(copy.occlusionTexture().toQString(),
              QStringLiteral("maps/occlusion.png"));
    EXPECT_FLOAT_EQ(copy.occlusionStrength(), 0.45f);
    EXPECT_EQ(copy.emissionTexture().toQString(),
              QStringLiteral("maps/emission.png"));
    EXPECT_FLOAT_EQ(copy.emissionStrength(), 2.5f);
    EXPECT_EQ(copy.opacityTexture().toQString(),
              QStringLiteral("maps/opacity.png"));
    EXPECT_FLOAT_EQ(copy.opacity(), 0.7f);
    EXPECT_EQ(copy.alphaMode(), MaterialAlphaMode::Masked);
    EXPECT_FLOAT_EQ(copy.alphaCutoff(), 0.42f);

    Material assigned;
    assigned = copy;
    copy = Material{};
    EXPECT_EQ(assigned.clearcoatTexture().toQString(),
              QStringLiteral("maps/coat.png"));
    EXPECT_FLOAT_EQ(assigned.sheen(), 0.65f);
    EXPECT_EQ(assigned.sheenColorTexture().toQString(),
              QStringLiteral("maps/sheen-color.png"));
    EXPECT_FLOAT_EQ(assigned.subsurface(), 0.4f);
    EXPECT_EQ(assigned.subsurfaceRadius(), QVector3D(2.0f, 0.8f, 0.2f));
    EXPECT_EQ(assigned.opacityTexture().toQString(),
              QStringLiteral("maps/opacity.png"));
}

TEST(MaterialContractTest, MoveTransfersStateAndMovedFromObjectCanBeReassigned)
{
    Material source;
    source.setMetallic(0.6f);
    source.setRoughness(0.15f);

    Material moved(std::move(source));
    EXPECT_FLOAT_EQ(moved.metallic(), 0.6f);
    EXPECT_FLOAT_EQ(moved.roughness(), 0.15f);

    Material destination;
    destination = std::move(moved);
    EXPECT_FLOAT_EQ(destination.metallic(), 0.6f);
    EXPECT_FLOAT_EQ(destination.roughness(), 0.15f);

    source = Material{};
    moved = Material{};
    EXPECT_FLOAT_EQ(source.metallic(), 0.0f);
    EXPECT_FLOAT_EQ(moved.roughness(), 0.5f);
}

} // namespace
