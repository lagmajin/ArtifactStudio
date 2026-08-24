module;

#include <QSize>
#include <QVector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

export module ArtifactPr.SequenceCompositor;

import ArtifactPr.SequenceCompositor;
import Image.ImageF32x4_RGBA;
import FloatRGBA;

namespace ArtifactPr {

namespace {

/// 1 枚のレイヤーを canvas サイズへ fit (aspect keep + 中央配置) させる。
/// 返す Mat は CV_32FC4 canonical RGBA。
cv::Mat fitFrameToCanvas(const ArtifactCore::ImageF32x4_RGBA& frame,
                         const QSize& canvasSize)
{
    cv::Mat canvas(canvasSize.height(), canvasSize.width(),
                   CV_32FC4,
                   cv::Scalar(0.0f, 0.0f, 0.0f, 0.0f));
    if (frame.isEmpty() || canvasSize.isEmpty()) {
        return canvas;
    }

    const cv::Mat src = frame.toCanonicalRGBA32FC4();
    if (src.empty()) {
        return canvas;
    }

    const double scaleX = static_cast<double>(canvasSize.width())
        / static_cast<double>(src.cols);
    const double scaleY = static_cast<double>(canvasSize.height())
        / static_cast<double>(src.rows);
    const double scale = std::min(scaleX, scaleY);

    const int dstW = std::max(1, static_cast<int>(std::lround(src.cols * scale)));
    const int dstH = std::max(1, static_cast<int>(std::lround(src.rows * scale)));

    cv::Mat resized;
    cv::resize(src, resized, cv::Size(dstW, dstH), 0.0, 0.0, cv::INTER_LINEAR);

    const int offsetX = (canvasSize.width() - dstW) / 2;
    const int offsetY = (canvasSize.height() - dstH) / 2;
    resized.copyTo(canvas(cv::Rect(offsetX, offsetY, dstW, dstH)));
    return canvas;
}

} // namespace

ArtifactCore::ImageF32x4_RGBA composeSequenceLayers(
    const QSize& canvasSize,
    const QVector<CompositeLayer>& layers,
    const ArtifactCore::FloatRGBA& background)
{
    ArtifactCore::ImageF32x4_RGBA canvas;
    canvas.resize(canvasSize.width(), canvasSize.height());
    canvas.fill(background);

    for (const CompositeLayer& layer : layers) {
        if (layer.frame.isEmpty() || layer.opacity <= 0.0) {
            continue;
        }

        cv::Mat fitted = fitFrameToCanvas(layer.frame, canvasSize);
        if (fitted.empty()) {
            continue;
        }

        ArtifactCore::ImageF32x4_RGBA overlay;
        overlay.setFromCVMat(fitted);
        canvas.alphaBlend(overlay, layer.opacity);
    }

    return canvas;
}

} // namespace ArtifactPr
