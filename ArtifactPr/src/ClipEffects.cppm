module;

#include <QMap>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

module ArtifactPr.ClipEffects;

import Image.ImageF32x4_RGBA;

namespace ArtifactPr {

namespace {

// RGB -> HSV (h: 0-360, s/v: 0-1)。全成分 0-1 前提。
void rgbToHsv(float r, float g, float b, float& h, float& s, float& v)
{
    const float maxC = std::max({r, g, b});
    const float minC = std::min({r, g, b});
    const float delta = maxC - minC;
    v = maxC;
    s = maxC > 0.0f ? delta / maxC : 0.0f;
    if (delta <= 0.0f) {
        h = 0.0f;
        return;
    }
    if (maxC == r) {
        h = 60.0f * std::fmod((g - b) / delta, 6.0f);
    } else if (maxC == g) {
        h = 60.0f * ((b - r) / delta + 2.0f);
    } else {
        h = 60.0f * ((r - g) / delta + 4.0f);
    }
    if (h < 0.0f) h += 360.0f;
}

void hsvToRgb(float h, float s, float v, float& r, float& g, float& b)
{
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    const float c = v * s;
    const float hp = h / 60.0f;
    const float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float r1 = 0.0f;
    float g1 = 0.0f;
    float b1 = 0.0f;
    if (hp < 1.0f)      { r1 = c; g1 = x; }
    else if (hp < 2.0f) { r1 = x; g1 = c; }
    else if (hp < 3.0f) { g1 = c; b1 = x; }
    else if (hp < 4.0f) { g1 = x; b1 = c; }
    else if (hp < 5.0f) { r1 = x; b1 = c; }
    else                { r1 = c; b1 = x; }
    const float m = v - c;
    r = r1 + m;
    g = g1 + m;
    b = b1 + m;
}

// RBJ cookbook 設計。戻り値は a0 正規化済みの 5 係数。
void designLowShelf(double fc, double gainDb, double fs,
                    double& b0, double& b1, double& b2, double& a1, double& a2)
{
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double S = 1.0;
    const double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
    const double sqA = 2.0 * std::sqrt(A) * alpha;
    const double a0 = (A + 1.0) + (A - 1.0) * cw + sqA;
    b0 = A * ((A + 1.0) - (A - 1.0) * cw + sqA) / a0;
    b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw) / a0;
    b2 = A * ((A + 1.0) - (A - 1.0) * cw - sqA) / a0;
    a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw) / a0;
    a2 = ((A + 1.0) + (A - 1.0) * cw - sqA) / a0;
}

void designPeaking(double fc, double gainDb, double fs,
                   double& b0, double& b1, double& b2, double& a1, double& a2)
{
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double alpha = sw / 2.0;   // Q = 1.0
    const double a0 = 1.0 + alpha / A;
    b0 = (1.0 + alpha * A) / a0;
    b1 = -2.0 * cw / a0;
    b2 = (1.0 - alpha * A) / a0;
    a1 = -2.0 * cw / a0;
    a2 = (1.0 - alpha / A) / a0;
}

void designHighShelf(double fc, double gainDb, double fs,
                     double& b0, double& b1, double& b2, double& a1, double& a2)
{
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double S = 1.0;
    const double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
    const double sqA = 2.0 * std::sqrt(A) * alpha;
    const double a0 = (A + 1.0) - (A - 1.0) * cw + sqA;
    b0 = A * ((A + 1.0) + (A - 1.0) * cw + sqA) / a0;
    b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0;
    b2 = A * ((A + 1.0) + (A - 1.0) * cw - sqA) / a0;
    a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0;
    a2 = ((A + 1.0) - (A - 1.0) * cw - sqA) / a0;
}

} // namespace

// =====================================================================
// トランジション
// =====================================================================

double transitionOpacityFactor(const QVector<TransitionFadeSpan>& spans,
                               FramePosition frame)
{
    double factor = 1.0;
    for (const auto& span : spans) {
        if (span.duration <= 0) continue;
        const FramePosition spanEnd = span.startFrame + span.duration;
        if (frame < span.startFrame || frame >= spanEnd) continue;

        // 区間内進捗 0..1
        const double t = static_cast<double>(frame - span.startFrame)
            / static_cast<double>(span.duration);

        double local = 1.0;
        if (span.curve == TransitionFadeCurve::DipToBlack) {
            local = span.isLeft ? 1.0 - 2.0 * t : 2.0 * t - 1.0;
            local = std::clamp(local, 0.0, 1.0);
        } else {
            local = span.isLeft ? t : 1.0 - t;
        }
        factor = std::min(factor, local);
    }
    return factor;
}

// =====================================================================
// パラメータ読み取り
// =====================================================================

bool effectEnabled(const QMap<QString, QVariant>& effects, const QString& effectId)
{
    const QString v = effects.value(QStringLiteral("fx.%1.enabled").arg(effectId)).toString();
    return v == QLatin1String("1")
        || v.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

double effectParam(const QMap<QString, QVariant>& effects, const QString& effectId,
                   const QString& param, double fallback)
{
    const QString v = effects.value(
        QStringLiteral("fx.%1.%2").arg(effectId, param)).toString();
    bool ok = false;
    const double value = v.toDouble(&ok);
    return ok ? value : fallback;
}

bool hasEnabledImageEffects(const QMap<QString, QVariant>& effects)
{
    static const QString kImageEffects[] = {
        QStringLiteral("brightnessContrast"),
        QStringLiteral("hueSaturation"),
        QStringLiteral("colorWheels"),
        QStringLiteral("gaussianBlur"),
        QStringLiteral("boxBlur"),
        QStringLiteral("unsharpMask"),
        QStringLiteral("glow"),
        QStringLiteral("posterize"),
        QStringLiteral("scale"),
        QStringLiteral("rotate"),
    };
    for (const QString& id : kImageEffects) {
        if (effectEnabled(effects, id)) return true;
    }
    return false;
}

double audioGainFactor(const QMap<QString, QVariant>& effects)
{
    if (!effectEnabled(effects, QStringLiteral("audioGain"))) return 1.0;
    const double gainDb = effectParam(effects, QStringLiteral("audioGain"),
                                     QStringLiteral("gainDb"), 0.0);
    return std::pow(10.0, gainDb / 20.0);
}

void audioEqualizerDb(const QMap<QString, QVariant>& effects,
                      double& lowDb, double& midDb, double& highDb)
{
    const QString id = QStringLiteral("audioEqualizer");
    if (!effectEnabled(effects, id)) {
        lowDb = 0.0;
        midDb = 0.0;
        highDb = 0.0;
        return;
    }
    lowDb = effectParam(effects, id, QStringLiteral("lowDb"), 0.0);
    midDb = effectParam(effects, id, QStringLiteral("midDb"), 0.0);
    highDb = effectParam(effects, id, QStringLiteral("highDb"), 0.0);
}

bool audioEqualizerActive(double lowDb, double midDb, double highDb)
{
    return std::abs(lowDb) > 0.01 || std::abs(midDb) > 0.01 || std::abs(highDb) > 0.01;
}

// =====================================================================
// 映像エフェクト評価
// =====================================================================

bool applyClipEffects(ArtifactCore::ImageF32x4_RGBA& image,
                      const QMap<QString, QVariant>& effects)
{
    if (image.isEmpty()) return false;
    if (!hasEnabledImageEffects(effects)) return false;

    // 正規化済み連続 CV_32FC4 (RGBA 順) のコピーへ落としてから処理する。
    cv::Mat mat = image.toCanonicalRGBA32FC4();
    if (mat.empty()) return false;

    const int totalPixels = mat.rows * mat.cols;
    float* data = reinterpret_cast<float*>(mat.data);

    // ---- 1) 色調補正 (straight alpha 域) ----
    const bool hasBc = effectEnabled(effects, QStringLiteral("brightnessContrast"));
    const bool hasHs = effectEnabled(effects, QStringLiteral("hueSaturation"));
    const bool hasCw = effectEnabled(effects, QStringLiteral("colorWheels"));
    const bool hasPz = effectEnabled(effects, QStringLiteral("posterize"));
    if (hasBc || hasHs || hasCw || hasPz) {
        const double bcBrightness = hasBc
            ? effectParam(effects, QStringLiteral("brightnessContrast"),
                          QStringLiteral("brightness"), 0.0) : 0.0;
        const double bcContrast = hasBc
            ? effectParam(effects, QStringLiteral("brightnessContrast"),
                          QStringLiteral("contrast"), 1.0) : 1.0;
        const double hue = hasHs
            ? effectParam(effects, QStringLiteral("hueSaturation"),
                          QStringLiteral("hue"), 0.0) : 0.0;
        const double saturation = hasHs
            ? effectParam(effects, QStringLiteral("hueSaturation"),
                          QStringLiteral("saturation"), 1.0) : 1.0;
        const double lightness = hasHs
            ? effectParam(effects, QStringLiteral("hueSaturation"),
                          QStringLiteral("lightness"), 0.0) : 0.0;
        const QString cwId = QStringLiteral("colorWheels");
        const double shR = hasCw ? effectParam(effects, cwId, QStringLiteral("shadowsR"), 0.0) : 0.0;
        const double shG = hasCw ? effectParam(effects, cwId, QStringLiteral("shadowsG"), 0.0) : 0.0;
        const double shB = hasCw ? effectParam(effects, cwId, QStringLiteral("shadowsB"), 0.0) : 0.0;
        const double mdR = hasCw ? effectParam(effects, cwId, QStringLiteral("midtonesR"), 0.0) : 0.0;
        const double mdG = hasCw ? effectParam(effects, cwId, QStringLiteral("midtonesG"), 0.0) : 0.0;
        const double mdB = hasCw ? effectParam(effects, cwId, QStringLiteral("midtonesB"), 0.0) : 0.0;
        const double hiR = hasCw ? effectParam(effects, cwId, QStringLiteral("highlightsR"), 0.0) : 0.0;
        const double hiG = hasCw ? effectParam(effects, cwId, QStringLiteral("highlightsG"), 0.0) : 0.0;
        const double hiB = hasCw ? effectParam(effects, cwId, QStringLiteral("highlightsB"), 0.0) : 0.0;
        const int levels = hasPz
            ? std::max(2, static_cast<int>(effectParam(effects, QStringLiteral("posterize"),
                                                       QStringLiteral("levels"), 8.0)))
            : 0;
        const float posterStep = hasPz ? 1.0f / static_cast<float>(levels - 1) : 0.0f;

        for (int i = 0; i < totalPixels; ++i) {
            float* px = data + i * 4;
            float r = px[0];
            float g = px[1];
            float b = px[2];

            if (hasBc) {
                r = static_cast<float>((r - 0.5) * bcContrast + 0.5 + bcBrightness);
                g = static_cast<float>((g - 0.5) * bcContrast + 0.5 + bcBrightness);
                b = static_cast<float>((b - 0.5) * bcContrast + 0.5 + bcBrightness);
            }
            if (hasHs) {
                float h = 0.0f;
                float s = 0.0f;
                float v = 0.0f;
                rgbToHsv(r, g, b, h, s, v);
                h = std::fmod(h + 360.0f + static_cast<float>(hue), 360.0f);
                s = std::clamp(s * static_cast<float>(saturation), 0.0f, 1.0f);
                v = std::clamp(v * static_cast<float>(1.0 + lightness), 0.0f, 1.0f);
                hsvToRgb(h, s, v, r, g, b);
            }
            if (hasCw) {
                const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                const float shW = std::clamp(1.0f - luma * 2.0f, 0.0f, 1.0f);
                const float hiW = std::clamp(luma * 2.0f - 1.0f, 0.0f, 1.0f);
                const float mdW = std::max(0.0f, 1.0f - shW - hiW);
                r += static_cast<float>(shR * shW + mdR * mdW + hiR * hiW);
                g += static_cast<float>(shG * shW + mdG * mdW + hiG * hiW);
                b += static_cast<float>(shB * shW + mdB * mdW + hiB * hiW);
            }
            if (hasPz) {
                r = std::round(r / posterStep) * posterStep;
                g = std::round(g / posterStep) * posterStep;
                b = std::round(b / posterStep) * posterStep;
            }

            px[0] = std::clamp(r, 0.0f, 1.0f);
            px[1] = std::clamp(g, 0.0f, 1.0f);
            px[2] = std::clamp(b, 0.0f, 1.0f);
        }
    }

    // ---- 2) geometry (scale / rotate: 中心基準・同サイズ出力・透明パディング) ----
    const bool hasScale = effectEnabled(effects, QStringLiteral("scale"));
    const bool hasRotate = effectEnabled(effects, QStringLiteral("rotate"));
    if (hasScale || hasRotate) {
        const cv::Point2f center(mat.cols * 0.5f, mat.rows * 0.5f);
        cv::Mat transform = cv::Mat::eye(3, 3, CV_64F);
        if (hasScale) {
            const double percent = std::clamp(
                effectParam(effects, QStringLiteral("scale"),
                            QStringLiteral("percent"), 100.0), 10.0, 400.0) / 100.0;
            cv::Mat scaleM = cv::Mat::eye(3, 3, CV_64F);
            scaleM.at<double>(0, 0) = percent;
            scaleM.at<double>(1, 1) = percent;
            scaleM.at<double>(0, 2) = center.x * (1.0 - percent);
            scaleM.at<double>(1, 2) = center.y * (1.0 - percent);
            transform = scaleM * transform;
        }
        if (hasRotate) {
            const double angle = std::clamp(
                effectParam(effects, QStringLiteral("rotate"),
                            QStringLiteral("angle"), 0.0), -180.0, 180.0);
            cv::Mat rot2x3 = cv::getRotationMatrix2D(center, angle, 1.0);
            cv::Mat rot3x3 = cv::Mat::eye(3, 3, CV_64F);
            rot2x3.copyTo(rot3x3.rowRange(0, 2));
            transform = rot3x3 * transform;
        }
        cv::Mat mapped;
        cv::warpAffine(mat, mapped, transform.rowRange(0, 2), mat.size(),
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                       cv::Scalar(0.0, 0.0, 0.0, 0.0));
        mat = mapped;
    }

    // ---- 3) blur 系 (premultiply → 処理 → unpremultiply) ----
    const bool hasGauss = effectEnabled(effects, QStringLiteral("gaussianBlur"));
    const bool hasBox = effectEnabled(effects, QStringLiteral("boxBlur"));
    const bool hasUnsharp = effectEnabled(effects, QStringLiteral("unsharpMask"));
    const bool hasGlow = effectEnabled(effects, QStringLiteral("glow"));
    if (hasGauss || hasBox || hasUnsharp || hasGlow) {
        const int total = mat.rows * mat.cols;
        float* pixels = reinterpret_cast<float*>(mat.data);
        for (int i = 0; i < total; ++i) {
            float* px = pixels + i * 4;
            const float a = px[3];
            px[0] *= a;
            px[1] *= a;
            px[2] *= a;
        }

        if (hasGauss) {
            const double radius = std::max(0.0,
                effectParam(effects, QStringLiteral("gaussianBlur"),
                            QStringLiteral("radius"), 8.0));
            if (radius > 0.01) {
                const double sigma = std::max(0.3, radius * 0.5);
                cv::GaussianBlur(mat, mat, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
            }
        }
        if (hasBox) {
            const double radius = std::max(0.0,
                effectParam(effects, QStringLiteral("boxBlur"),
                            QStringLiteral("radius"), 8.0));
            if (radius > 0.01) {
                const int k = 2 * std::max(1, static_cast<int>(radius)) + 1;
                cv::boxFilter(mat, mat, -1, cv::Size(k, k), cv::Point(-1, -1),
                              true, cv::BORDER_REPLICATE);
            }
        }
        if (hasUnsharp) {
            const double amount = std::clamp(
                effectParam(effects, QStringLiteral("unsharpMask"),
                            QStringLiteral("amount"), 1.0), 0.0, 3.0);
            const double radius = std::max(0.5,
                effectParam(effects, QStringLiteral("unsharpMask"),
                            QStringLiteral("radius"), 3.0));
            if (amount > 0.001) {
                cv::Mat blurred;
                const double sigma = std::max(0.3, radius * 0.5);
                cv::GaussianBlur(mat, blurred, cv::Size(), sigma, sigma,
                                 cv::BORDER_REPLICATE);
                const float* blurredData = reinterpret_cast<const float*>(blurred.data);
                for (int i = 0; i < total; ++i) {
                    float* px = pixels + i * 4;
                    const float* bp = blurredData + i * 4;
                    px[0] += static_cast<float>(amount) * (px[0] - bp[0]);
                    px[1] += static_cast<float>(amount) * (px[1] - bp[1]);
                    px[2] += static_cast<float>(amount) * (px[2] - bp[2]);
                }
            }
        }
        if (hasGlow) {
            const double intensity = std::clamp(
                effectParam(effects, QStringLiteral("glow"),
                            QStringLiteral("intensity"), 0.8), 0.0, 2.0);
            const double radius = std::max(0.0,
                effectParam(effects, QStringLiteral("glow"),
                            QStringLiteral("radius"), 12.0));
            const double threshold = std::clamp(
                effectParam(effects, QStringLiteral("glow"),
                            QStringLiteral("threshold"), 0.6), 0.0, 0.99);
            if (intensity > 0.001 && radius > 0.01) {
                // 輝度の閾値超部分を抽出し、プレマルチ域でぼかして加算。
                cv::Mat glow = cv::Mat::zeros(mat.size(), CV_32FC4);
                float* glowData = reinterpret_cast<float*>(glow.data);
                for (int i = 0; i < total; ++i) {
                    const float* px = pixels + i * 4;
                    float* gx = glowData + i * 4;
                    const float luma = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
                    if (luma <= threshold) continue;
                    const float m = (luma - static_cast<float>(threshold))
                        / (1.0f - static_cast<float>(threshold));
                    gx[0] = px[0] * m;
                    gx[1] = px[1] * m;
                    gx[2] = px[2] * m;
                }
                const double sigma = std::max(0.3, radius * 0.5);
                cv::GaussianBlur(glow, glow, cv::Size(), sigma, sigma,
                                 cv::BORDER_REPLICATE);
                const float* glowBlurred = reinterpret_cast<const float*>(glow.data);
                for (int i = 0; i < total; ++i) {
                    float* px = pixels + i * 4;
                    const float* gx = glowBlurred + i * 4;
                    px[0] += static_cast<float>(intensity) * gx[0];
                    px[1] += static_cast<float>(intensity) * gx[1];
                    px[2] += static_cast<float>(intensity) * gx[2];
                }
            }
        }

        // unpremultiply + clamp
        for (int i = 0; i < total; ++i) {
            float* px = pixels + i * 4;
            px[3] = std::clamp(px[3], 0.0f, 1.0f);
            const float a = px[3];
            if (a > 1.0e-4f) {
                px[0] = std::clamp(px[0] / a, 0.0f, 1.0f);
                px[1] = std::clamp(px[1] / a, 0.0f, 1.0f);
                px[2] = std::clamp(px[2] / a, 0.0f, 1.0f);
            } else {
                px[0] = 0.0f;
                px[1] = 0.0f;
                px[2] = 0.0f;
            }
        }
    }

    image.setFromCVMat(mat);
    return true;
}

// =====================================================================
// ClipEqualizer
// =====================================================================

void ClipEqualizer::configure()
{
    active = audioEqualizerActive(lowDb, midDb, highDb);
    reset();
    if (!active) return;

    const double fs = sampleRate > 0 ? sampleRate : 48000.0;
    designLowShelf(200.0, lowDb, fs, b0_[0], b1_[0], b2_[0], a1_[0], a2_[0]);
    designPeaking(1000.0, midDb, fs, b0_[1], b1_[1], b2_[1], a1_[1], a2_[1]);
    designHighShelf(4000.0, highDb, fs, b0_[2], b1_[2], b2_[2], a1_[2], a2_[2]);
}

void ClipEqualizer::reset()
{
    for (int band = 0; band < 3; ++band) {
        x1_[band] = 0.0;
        x2_[band] = 0.0;
        y1_[band] = 0.0;
        y2_[band] = 0.0;
    }
}

void ClipEqualizer::process(float* samples, int frameCount)
{
    if (!active || samples == nullptr || frameCount <= 0) return;
    for (int i = 0; i < frameCount; ++i) {
        double y = static_cast<double>(samples[i]);
        for (int band = 0; band < 3; ++band) {
            const double out = b0_[band] * y + b1_[band] * x1_[band] + b2_[band] * x2_[band]
                - a1_[band] * y1_[band] - a2_[band] * y2_[band];
            x2_[band] = x1_[band];
            x1_[band] = y;
            y2_[band] = y1_[band];
            y1_[band] = out;
            y = out;
        }
        samples[i] = static_cast<float>(y);
    }
}

} // namespace ArtifactPr
