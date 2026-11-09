#include <QColor>

#include <gtest/gtest.h>

#include <limits>
#include <utility>

import Material.Material;

namespace {

using ArtifactCore::Material;
using ArtifactCore::MaterialAlphaMode;
using ArtifactCore::MaterialPresetKind;
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

TEST(MaterialContractTest, CopyConstructionAndAssignmentOwnIndependentState)
{
    Material source;
    source.setBaseColor(QColor(24, 96, 192));
    source.setMetallic(0.75f);
    source.setRoughness(0.2f);

    Material copy(source);
    source.setBaseColor(QColor(200, 40, 20));
    source.setMetallic(0.1f);

    EXPECT_EQ(copy.baseColor(), QColor(24, 96, 192));
    EXPECT_FLOAT_EQ(copy.metallic(), 0.75f);
    EXPECT_FLOAT_EQ(copy.roughness(), 0.2f);

    Material assigned;
    assigned = copy;
    copy.setRoughness(0.9f);

    EXPECT_EQ(assigned.baseColor(), QColor(24, 96, 192));
    EXPECT_FLOAT_EQ(assigned.metallic(), 0.75f);
    EXPECT_FLOAT_EQ(assigned.roughness(), 0.2f);
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
