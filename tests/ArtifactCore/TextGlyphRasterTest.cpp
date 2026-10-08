#include <gtest/gtest.h>

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPointF>
#include <QString>

#include <cstdint>
#include <vector>

import Font.FreeFont;
import Text.GlyphAtlas;
import Text.Animator;
import Text.ShapingBackend;
import Text.Style;

using namespace ArtifactCore;

namespace {

class TextGlyphRasterEnvironment final : public testing::Environment
{
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactCoreTextGlyphRasterTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QGuiApplication* application_ = nullptr;
};

testing::Environment* const guiApplication =
    testing::AddGlobalTestEnvironment(new TextGlyphRasterEnvironment());

} // namespace

TEST(TextGlyphRasterTest, GlyphAtlasContainsRasterizedCoveragePixels)
{
    const QString family = FontManager::defaultSansSerifFamily();
    ASSERT_FALSE(family.isEmpty());
    QFont font(family);
    font.setPixelSize(48);

    GlyphKey key;
    key.codePoint = U'A';
    key.fontSize = 48.0f;
    key.fontFamily = family.toStdString();

    GlyphAtlas atlas;
    const GlyphRect glyph = atlas.acquire(key, font);
    ASSERT_TRUE(glyph.valid);
    ASSERT_GT(glyph.width, 0);
    ASSERT_GT(glyph.height, 0);

    const QImage& image = atlas.atlasImage();
    ASSERT_FALSE(image.isNull());
    ASSERT_GE(glyph.atlasX, 0);
    ASSERT_GE(glyph.atlasY, 0);
    ASSERT_LE(glyph.atlasX + glyph.width, image.width());
    ASSERT_LE(glyph.atlasY + glyph.height, image.height());

    std::uint64_t coveredPixels = 0;
    for (int y = glyph.atlasY; y < glyph.atlasY + glyph.height; ++y) {
        for (int x = glyph.atlasX; x < glyph.atlasX + glyph.width; ++x) {
            coveredPixels += image.pixelColor(x, y).alpha() > 0 ? 1u : 0u;
        }
    }
    EXPECT_GT(coveredPixels, 0u);

    atlas.clearDirty();
    const GlyphRect cached = atlas.acquire(key, font);
    EXPECT_EQ(cached.atlasX, glyph.atlasX);
    EXPECT_EQ(cached.atlasY, glyph.atlasY);
    EXPECT_EQ(cached.width, glyph.width);
    EXPECT_EQ(cached.height, glyph.height);
    EXPECT_FALSE(atlas.isDirty());
}

TEST(TextGlyphRasterTest, ShapedAnimatorSelectionStillRasterizesEachGlyph)
{
    const QString family = FontManager::defaultSansSerifFamily();
    ASSERT_FALSE(family.isEmpty());
    TextStyle style;
    style.fontFamily = UniString(family);
    style.fontSize = 48.0f;
    style.pixelSize = 48.0f;
    QFont font(family);
    font.setPixelSize(48);

    TextShapingRequest request;
    request.text = QStringLiteral("AB");
    request.style = style;
    QtShapingBackend backend;
    auto shaped = backend.shape(request);
    ASSERT_EQ(shaped.glyphs.size(), 2u);

    AnimatorSelectorSet animator;
    animator.range.units = SelectorUnits::Index;
    animator.range.start = 0.0f;
    animator.range.end = 0.0f;
    animator.range.shape = SelectorShape::Square;
    animator.properties.position = QPointF(12.0, -3.0);
    const std::vector<AnimatorSelectorSet> animators{animator};
    TextAnimatorEngine::applyAnimatorSets(
        shaped.glyphs, animators, 0.0f, request.text);

    EXPECT_EQ(shaped.glyphs[0].offsetPosition, QPointF(12.0, -3.0));
    EXPECT_EQ(shaped.glyphs[1].offsetPosition, QPointF(0.0, 0.0));

    GlyphAtlas atlas;
    for (const GlyphItem& glyph : shaped.glyphs) {
        GlyphKey key;
        key.codePoint = static_cast<char32_t>(glyph.charCode);
        key.fontSize = style.pixelSize;
        key.fontFamily = family.toStdString();
        const GlyphRect raster = atlas.acquire(key, font);
        ASSERT_TRUE(raster.valid)
            << "codePoint=" << static_cast<std::uint32_t>(glyph.charCode);
        ASSERT_GT(raster.width, 0);
        ASSERT_GT(raster.height, 0);
        const QImage& image = atlas.atlasImage();
        std::uint64_t coveredPixels = 0;
        for (int y = raster.atlasY; y < raster.atlasY + raster.height; ++y) {
            for (int x = raster.atlasX; x < raster.atlasX + raster.width; ++x) {
                coveredPixels += image.pixelColor(x, y).alpha() > 0 ? 1u : 0u;
            }
        }
        EXPECT_GT(coveredPixels, 0u)
            << "codePoint=" << static_cast<std::uint32_t>(glyph.charCode);
    }
}
