#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwrite_3.h>
#include <wrl/client.h>

#include <cstdio>
#include <cwchar>
#include <fstream>
#include <algorithm>
#include <vector>

using Microsoft::WRL::ComPtr;

struct AlphaLayer {
    RECT bounds{};
    std::vector<BYTE> alpha;
    DWRITE_COLOR_F color{};
};

int wmain() {
    ComPtr<IDWriteFactory> factory;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                     __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        std::printf("directwrite: factory-failed hr=0x%08X\n", static_cast<unsigned>(hr));
        return 1;
    }

    ComPtr<IDWriteFontCollection> collection;
    if (FAILED(factory->GetSystemFontCollection(&collection))) {
        std::puts("directwrite: font-collection-failed");
        return 1;
    }

    UINT32 familyIndex = 0;
    BOOL exists = FALSE;
    collection->FindFamilyName(L"Segoe UI Emoji", &familyIndex, &exists);
    if (!exists) {
        std::puts("directwrite: Segoe UI Emoji not-installed");
        return 2;
    }

    ComPtr<IDWriteFontFamily> family;
    if (FAILED(collection->GetFontFamily(familyIndex, &family))) return 3;
    ComPtr<IDWriteFont> font;
    if (FAILED(family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                            DWRITE_FONT_STRETCH_NORMAL,
                                            DWRITE_FONT_STYLE_NORMAL,
                                            &font))) return 4;
    ComPtr<IDWriteFontFace> face;
    if (FAILED(font->CreateFontFace(&face))) return 5;

    UINT16 glyph = 0;
    const UINT32 codePoint = 0x1F9EA;
    if (FAILED(face->GetGlyphIndices(&codePoint, 1, &glyph))) return 6;

    ComPtr<IDWriteFactory2> factory2;
    factory.As(&factory2);
    if (!factory2) {
        std::puts("directwrite: factory2-unavailable");
        return 7;
    }
    ComPtr<IDWriteFactory3> factory3;
    factory.As(&factory3);
    if (!factory3) {
        std::puts("directwrite: factory3-unavailable");
        return 7;
    }

    DWRITE_GLYPH_RUN glyphRun{};
    glyphRun.fontFace = face.Get();
    glyphRun.fontEmSize = 96.0f;
    glyphRun.glyphCount = 1;
    glyphRun.glyphIndices = &glyph;

    ComPtr<IDWriteColorGlyphRunEnumerator> runs;
    hr = factory2->TranslateColorGlyphRun(0.0f, 96.0f, &glyphRun, nullptr,
                                          DWRITE_MEASURING_MODE_NATURAL, nullptr,
                                          0, &runs);
    if (FAILED(hr) || !runs) {
        std::printf("directwrite: color-run-enumerator-failed hr=0x%08X glyph=%u\n",
                    static_cast<unsigned>(hr), glyph);
        return 8;
    }

    UINT32 count = 0;
    UINT32 alphaPixels = 0;
    std::vector<AlphaLayer> layers;
    BOOL hasRun = FALSE;
    while (SUCCEEDED(runs->MoveNext(&hasRun)) && hasRun) {
        const DWRITE_COLOR_GLYPH_RUN* run = nullptr;
        if (FAILED(runs->GetCurrentRun(&run)) || !run) return 9;
        ++count;
        ComPtr<IDWriteColorGlyphRunEnumerator1> runs1;
        runs.As(&runs1);
        DWRITE_GLYPH_IMAGE_FORMATS imageFormats = DWRITE_GLYPH_IMAGE_FORMATS_NONE;
        if (runs1) {
            const DWRITE_COLOR_GLYPH_RUN1* run1 = nullptr;
            if (SUCCEEDED(runs1->GetCurrentRun(&run1)) && run1) {
                imageFormats = run1->glyphImageFormat;
            }
        }
        ComPtr<IDWriteGlyphRunAnalysis> analysis;
        if (SUCCEEDED(factory3->CreateGlyphRunAnalysis(
                &run->glyphRun, nullptr, DWRITE_RENDERING_MODE1_NATURAL,
                DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DISABLED,
                DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, 96.0f,
                &analysis))) {
            RECT bounds{};
            if (SUCCEEDED(analysis->GetAlphaTextureBounds(
                    DWRITE_TEXTURE_ALIASED_1x1, &bounds))) {
                const int width = bounds.right - bounds.left;
                const int height = bounds.bottom - bounds.top;
                if (width > 0 && height > 0) {
                    std::vector<BYTE> alpha(static_cast<size_t>(width) * height);
                    if (SUCCEEDED(analysis->CreateAlphaTexture(
                            DWRITE_TEXTURE_ALIASED_1x1, &bounds, alpha.data(),
                            static_cast<UINT32>(alpha.size())))) {
                        for (const BYTE value : alpha) {
                            alphaPixels += value != 0 ? 1u : 0u;
                        }
                        layers.push_back({bounds, std::move(alpha), run->runColor});
                    }
                }
            }
        }
        std::printf("directwrite: color-run=%u paletteIndex=%u color=(%.3f,%.3f,%.3f,%.3f) glyphCount=%u imageFormats=0x%X\n",
                    count, run->paletteIndex, run->runColor.r, run->runColor.g,
                    run->runColor.b, run->runColor.a, run->glyphRun.glyphCount,
                    static_cast<unsigned>(imageFormats));
    }

    if (!layers.empty()) {
        int minX = layers.front().bounds.left;
        int minY = layers.front().bounds.top;
        int maxX = layers.front().bounds.right;
        int maxY = layers.front().bounds.bottom;
        for (const auto& layer : layers) {
            minX = std::min(minX, static_cast<int>(layer.bounds.left));
            minY = std::min(minY, static_cast<int>(layer.bounds.top));
            maxX = std::max(maxX, static_cast<int>(layer.bounds.right));
            maxY = std::max(maxY, static_cast<int>(layer.bounds.bottom));
        }
        const int width = maxX - minX;
        const int height = maxY - minY;
        std::vector<float> rgba(static_cast<size_t>(width) * height * 4u, 0.0f);
        for (const auto& layer : layers) {
            const int layerWidth = layer.bounds.right - layer.bounds.left;
            const int layerHeight = layer.bounds.bottom - layer.bounds.top;
            for (int y = 0; y < layerHeight; ++y) {
                for (int x = 0; x < layerWidth; ++x) {
                    const float sourceAlpha = layer.alpha[static_cast<size_t>(y) * layerWidth + x] / 255.0f;
                    if (sourceAlpha <= 0.0f) continue;
                    const int dx = layer.bounds.left - minX + x;
                    const int dy = layer.bounds.top - minY + y;
                    float* dst = &rgba[(static_cast<size_t>(dy) * width + dx) * 4u];
                    const float sourceA = sourceAlpha * layer.color.a;
                    const float oneMinusA = 1.0f - sourceA;
                    dst[0] = layer.color.r * sourceA + dst[0] * oneMinusA;
                    dst[1] = layer.color.g * sourceA + dst[1] * oneMinusA;
                    dst[2] = layer.color.b * sourceA + dst[2] * oneMinusA;
                    dst[3] = sourceA + dst[3] * oneMinusA;
                }
            }
        }
        std::ofstream ppm("directwrite_color_glyph.ppm", std::ios::binary);
        ppm << "P6\n" << width << ' ' << height << "\n255\n";
        for (size_t i = 0; i < rgba.size(); i += 4) {
            const auto byte = [](float value) -> unsigned char {
                return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            const unsigned char rgb[3] = {byte(rgba[i]), byte(rgba[i + 1]), byte(rgba[i + 2])};
            ppm.write(reinterpret_cast<const char*>(rgb), 3);
        }
        std::printf("directwrite: rgbaComposite=%dx%d path=directwrite_color_glyph.ppm\n",
                    width, height);
    }

    std::printf("directwrite: codepoint=U+1F9EA glyph=%u colorRuns=%u alphaPixels=%u\n",
                glyph, count, alphaPixels);
    return count > 0 && alphaPixels > 0 ? 0 : 10;
}
