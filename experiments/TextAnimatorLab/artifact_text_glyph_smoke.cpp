#include <QGuiApplication>
#include <QColor>
#include <QImage>
#include <cstdio>
#include <vector>

#include <RenderDevice.h>
#include <RefCntAutoPtr.hpp>

import Artifact.Render.TextGpuDevice;
import Artifact.Render.TextRenderTarget;
import Artifact.Render.TextGlyphShaderSources;
import Artifact.Render.TextGlyphSubmitter.Contract;
import Text.GlyphAtlas;
import Text.GlyphLayout;
import Text.Style;
import Font.FreeFont;
import Utils.String.UniString;

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    const QString sample = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("Text Sample1 🧪");
    const QString output = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                    : QStringLiteral("artifact_text_glyph_smoke.png");

    Artifact::ArtifactTextGpuDevice gpu;
    if (!gpu.initialize()) { std::fprintf(stderr, "glyph-smoke: device=0\n"); return 2; }
    Artifact::ArtifactTextRenderTarget target;
    if (!target.create(gpu.device(), 1000, 180)) { std::fprintf(stderr, "glyph-smoke: target=0\n"); return 3; }

    Diligent::RefCntAutoPtr<Diligent::IShader> ps, vs, tvs;
    if (!Artifact::createArtifactTextGlyphShaders(gpu.device(), ps, vs, tvs)) {
        std::fprintf(stderr, "glyph-smoke: shaders=0\n"); return 4;
    }
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso, tpso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb, tsrb;
    Diligent::RefCntAutoPtr<Diligent::ISampler> sampler;
    if (!Artifact::createArtifactTextGlyphPipelines(gpu.device(), Diligent::TEX_FORMAT_RGBA8_UNORM,
            vs, ps, tvs, pso, srb, tpso, tsrb, sampler)) {
        std::fprintf(stderr, "glyph-smoke: pipelines=0\n"); return 5;
    }

    ArtifactCore::GlyphAtlas coreAtlas;
    ArtifactCore::TextStyle textStyle;
    textStyle.fontSize = 64.0f; textStyle.pixelSize = 64.0f;
    std::vector<ArtifactCore::GlyphItem> layout = ArtifactCore::TextLayoutEngine::layout(
        ArtifactCore::UniString(sample), textStyle, ArtifactCore::ParagraphStyle{});
    const bool noTransform = qEnvironmentVariableIsSet("ARTIFACT_TEXT_SMOKE_NO_TRANSFORM");
    for (size_t i = 0; i < layout.size(); ++i) {
        layout[i].offsetPosition += QPointF(120.0, 0.0);
        if (layout[i].isEmojiSequence) {
            // Keep composite-cluster verification away from the target edge;
            // transform clipping is tested separately by the regular glyphs.
        }
        layout[i].offsetRotation = noTransform ? 0.0f : (static_cast<float>(i) - 5.5f) * 4.0f;
        layout[i].offsetScale = noTransform ? 1.0f : 1.0f + ((i % 3) == 0 ? 0.08f : 0.0f);
        layout[i].offsetOpacity = noTransform ? 1.0f : 0.88f + 0.12f * static_cast<float>(i % 4) / 3.0f;
    }
    std::size_t atlasGlyphs = 0;
    int colorGlyphs = 0;
    for (const auto& glyph : layout) {
        const QString glyphText = QString::fromUcs4(&glyph.charCode, 1);
        const QFont font = ArtifactCore::FontManager::makeFont(textStyle, glyphText);
        ArtifactCore::GlyphKey key;
        key.codePoint = glyph.charCode; key.fontSize = textStyle.fontSize;
        key.fontFamily = font.family().toStdString(); key.renderMode = glyph.renderMode;
        const auto rect = coreAtlas.acquire(key, font);
        if (!rect.valid) continue;
        ++atlasGlyphs;
        if (rect.colorPreserved) ++colorGlyphs;
    }
    if (atlasGlyphs == 0) { std::fprintf(stderr, "glyph-smoke: atlas=0\n"); return 6; }
    std::fprintf(stderr, "glyph-smoke: layoutGlyphs=%zu colorGlyphs=%d atlas=%dx%d\n",
        atlasGlyphs, colorGlyphs, coreAtlas.width(), coreAtlas.height());

    Artifact::ArtifactTextGlyphSubmitter submitter;
    Artifact::ArtifactTextGlyphPipelineProvider provider{
        pso.RawPtr(), srb.RawPtr(), tpso.RawPtr(), tsrb.RawPtr(), sampler.RawPtr()};
    if (!submitter.initialize(Diligent::RefCntAutoPtr<Diligent::IRenderDevice>(gpu.device()),
                              Diligent::TEX_FORMAT_RGBA8_UNORM, provider)) {
        std::fprintf(stderr, "glyph-smoke: submitter-init=0\n"); return 7;
    }
    target.clear(gpu.context(), 0, 0, 0, 0);
    if (!submitter.submit(gpu.context(), target.renderTargetView(), layout, textStyle,
                          ArtifactCore::FloatColor(1, 1, 1, 1), 1.0f)) {
        std::fprintf(stderr, "glyph-smoke: submitter-submit=0\n"); return 8;
    }
    submitter.flush(gpu.context()); gpu.context()->WaitForIdle();
    QImage apiImage; const bool apiRead = target.readback(gpu.context(), apiImage);
    int nonZeroAlpha = 0;
    int colorPixels = 0;
    if (apiRead) {
        for (int y = 0; y < apiImage.height(); ++y) {
            for (int x = 0; x < apiImage.width(); ++x) {
                const QColor pixel = apiImage.pixelColor(x, y);
                if (pixel.alpha() > 0) {
                    ++nonZeroAlpha;
                    if (pixel.red() != pixel.green() || pixel.green() != pixel.blue()) {
                        ++colorPixels;
                    }
                }
            }
        }
    }
    const bool apiHasPixels = nonZeroAlpha > 0;
    const bool apiColorPreserved = colorGlyphs == 0 || colorPixels > 0;
    const bool apiSaved = apiRead && apiHasPixels && apiColorPreserved && apiImage.save(output);
    std::fprintf(stderr, "glyph-smoke: submitter-api=1 image=%dx%d nonzeroAlpha=%d colorPixels=%d colorPreserved=%d saved=%d path=%s\n",
        apiImage.width(), apiImage.height(), nonZeroAlpha, colorPixels,
        apiColorPreserved ? 1 : 0, apiSaved ? 1 : 0,
        output.toLocal8Bit().constData());
    submitter.destroy(); target.destroy(); gpu.destroy();
    return apiSaved ? 0 : 9;
}
