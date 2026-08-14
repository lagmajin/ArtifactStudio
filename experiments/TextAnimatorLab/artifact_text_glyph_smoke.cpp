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

struct Vertex { float pos[2]; float uv[2]; float color[4]; };
struct Transform { float offset[2]; float scale[2]; float screenSize[2]; };

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

    const QImage atlas = coreAtlas.atlasImage();
    Diligent::TextureDesc atlasDesc; atlasDesc.Name = "GlyphSmokeAtlas";
    atlasDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    atlasDesc.Width = static_cast<Diligent::Uint32>(atlas.width());
    atlasDesc.Height = static_cast<Diligent::Uint32>(atlas.height());
    atlasDesc.MipLevels = 1; atlasDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    atlasDesc.Usage = Diligent::USAGE_IMMUTABLE; atlasDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData sub{atlas.constBits(), static_cast<Diligent::Uint32>(atlas.bytesPerLine())};
    Diligent::TextureData texData{&sub, 1};
    Diligent::RefCntAutoPtr<Diligent::ITexture> atlasTexture;
    gpu.device()->CreateTexture(atlasDesc, &texData, &atlasTexture);
    auto* atlasView = atlasTexture ? atlasTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE) : nullptr;
    if (!vb || !cb || !atlasView) { std::fprintf(stderr, "glyph-smoke: resources=0\n"); return 7; }

    srb->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "TransformCB")->Set(cb);
    srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_texture")->Set(atlasView);
    srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_sampler")->Set(sampler);
    target.clear(gpu.context(), 0, 0, 0, 0);
    auto* rtv = target.renderTargetView();
    gpu.context()->SetRenderTargets(1, &rtv, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    gpu.context()->SetPipelineState(pso);
    gpu.context()->CommitShaderResources(srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::IBuffer* buffers[] = {vb}; Diligent::Uint64 offsets[] = {0};
    gpu.context()->SetVertexBuffers(0, 1, buffers, offsets,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    for (Diligent::Uint32 i = 0; i < drawGlyphs.size(); ++i)
        gpu.context()->Draw(Diligent::DrawAttribs{4, Diligent::DRAW_FLAG_VERIFY_ALL, 1, i * 4});
    gpu.context()->Flush(); gpu.context()->WaitForIdle();

    QImage image; const bool read = target.readback(gpu.context(), image);
    const bool saved = read && image.save(output);
    int nonZeroAlpha = 0; int fullAlpha = 0; int minAlpha = 255; int maxAlpha = 0;
    if (read) {
        for (int y = 30; y < 150; ++y) for (int x = 80; x < 560; ++x) {
            const int a = image.pixelColor(x, y).alpha();
            nonZeroAlpha += a > 0 ? 1 : 0; fullAlpha += a == 255 ? 1 : 0;
            minAlpha = std::min(minAlpha, a); maxAlpha = std::max(maxAlpha, a);
        }
    }
    std::fprintf(stderr, "glyph-smoke: device=1 pipelines=1 glyphQuad=1 layout=Text Sample1-emoji image=%dx%d saved=%d path=%s\n",
        image.width(), image.height(), saved ? 1 : 0, output.toLocal8Bit().constData());
    std::fprintf(stderr, "glyph-smoke: gpu-alpha nonzero=%d full=%d min=%d max=%d\n",
        nonZeroAlpha, fullAlpha, minAlpha, maxAlpha);
    target.destroy(); gpu.destroy();
    return saved ? 0 : 7;
}
