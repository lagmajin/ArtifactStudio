#include <QGuiApplication>
#include <QImage>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>

#include <Buffer.h>
#include <DeviceContext.h>
#include <RenderDevice.h>
#include <Texture.h>
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
    struct DrawGlyph { ArtifactCore::GlyphRect rect; float x; float y; };
    std::vector<DrawGlyph> drawGlyphs;
    drawGlyphs.reserve(layout.size());
    float penX = 24.0f;
    int colorGlyphs = 0;
    for (const auto& glyph : layout) {
        const QString glyphText = QString::fromUcs4(&glyph.charCode, 1);
        const QFont font = ArtifactCore::FontManager::makeFont(textStyle, glyphText);
        ArtifactCore::GlyphKey key;
        key.codePoint = glyph.charCode; key.fontSize = textStyle.fontSize;
        key.fontFamily = font.family().toStdString(); key.renderMode = glyph.renderMode;
        const auto rect = coreAtlas.acquire(key, font);
        if (!rect.valid) continue;
        if (rect.colorPreserved) ++colorGlyphs;
        drawGlyphs.push_back({rect, penX + rect.bearingX, 128.0f - rect.bearingY});
        penX += rect.advance > 0.0f ? rect.advance : static_cast<float>(rect.width);
    }
    if (drawGlyphs.empty()) { std::fprintf(stderr, "glyph-smoke: atlas=0\n"); return 6; }
    std::fprintf(stderr, "glyph-smoke: layoutGlyphs=%zu colorGlyphs=%d atlas=%dx%d\n",
        drawGlyphs.size(), colorGlyphs, coreAtlas.width(), coreAtlas.height());
    const float aw = static_cast<float>(coreAtlas.width());
    const float ah = static_cast<float>(coreAtlas.height());
    std::vector<Vertex> vertices;
    vertices.reserve(drawGlyphs.size() * 4);
    for (const auto& glyph : drawGlyphs) {
        const auto& rect = glyph.rect;
        const float x0 = glyph.x, y0 = glyph.y;
        const float x1 = x0 + rect.width, y1 = y0 + rect.height;
        const float u0 = rect.u0(static_cast<int>(aw)), v0 = rect.v0(static_cast<int>(ah));
        const float u1 = rect.u1(static_cast<int>(aw)), v1 = rect.v1(static_cast<int>(ah));
        const float r = rect.colorPreserved ? 1.0f : 1.0f;
        const float g = rect.colorPreserved ? 1.0f : 1.0f;
        const float b = rect.colorPreserved ? 1.0f : 1.0f;
        const float alpha = rect.colorPreserved ? -1.0f : 1.0f;
        vertices.push_back({{x0,y0},{u0,v0},{r,g,b,alpha}});
        vertices.push_back({{x1,y0},{u1,v0},{r,g,b,alpha}});
        vertices.push_back({{x0,y1},{u0,v1},{r,g,b,alpha}});
        vertices.push_back({{x1,y1},{u1,v1},{r,g,b,alpha}});
    }
    Diligent::BufferDesc vbDesc; vbDesc.Name = "GlyphSmokeVB";
    vbDesc.Size = static_cast<Diligent::Uint32>(vertices.size() * sizeof(Vertex)); vbDesc.Usage = Diligent::USAGE_IMMUTABLE;
    vbDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    Diligent::BufferData vbData{vertices.data(), vbDesc.Size};
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vb;
    gpu.device()->CreateBuffer(vbDesc, &vbData, &vb);

    Transform transform{{0, 0}, {1, 1}, {1000, 180}};
    Diligent::BufferDesc cbDesc; cbDesc.Name = "GlyphSmokeTransform";
    cbDesc.Size = sizeof(transform); cbDesc.Usage = Diligent::USAGE_IMMUTABLE;
    cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    Diligent::BufferData cbData{&transform, sizeof(transform)};
    Diligent::RefCntAutoPtr<Diligent::IBuffer> cb;
    gpu.device()->CreateBuffer(cbDesc, &cbData, &cb);

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
    const bool apiSaved = apiRead && apiImage.save(output);
    std::fprintf(stderr, "glyph-smoke: submitter-api=1 image=%dx%d saved=%d path=%s\n",
        apiImage.width(), apiImage.height(), apiSaved ? 1 : 0, output.toLocal8Bit().constData());
    submitter.destroy(); target.destroy(); gpu.destroy();
    return apiSaved ? 0 : 9;
}
