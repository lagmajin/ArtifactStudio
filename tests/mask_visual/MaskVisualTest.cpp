#include <opencv2/core.hpp>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <opencv2/imgcodecs.hpp>

import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;

using namespace Artifact;

namespace {
constexpr int kWidth = 256;
constexpr int kHeight = 192;

MaskPath rectangle(float left, float top, float right, float bottom,
                   MaskMode mode = MaskMode::Add, float opacity = 1.0f,
                   float feather = 0.0f, float expansion = 0.0f,
                   bool inverted = false)
{
    MaskPath path;
    path.addVertex({QPointF(left, top), {}, {}});
    path.addVertex({QPointF(right, top), {}, {}});
    path.addVertex({QPointF(right, bottom), {}, {}});
    path.addVertex({QPointF(left, bottom), {}, {}});
    path.setClosed(true);
    path.setMode(mode);
    path.setOpacity(opacity);
    path.setFeather({feather});
    path.setExpansion({expansion});
    path.setInverted(inverted);
    return path;
}

bool writePng(const std::filesystem::path& path, const cv::Mat& alpha)
{
    if (alpha.type() != CV_32FC1 || alpha.cols != kWidth || alpha.rows != kHeight) {
        return false;
    }
    cv::Mat grayscale8;
    alpha.convertTo(grayscale8, CV_8UC1, 255.0);
    if (!cv::imwrite(path.string(), grayscale8)) {
        std::cerr << "failed to write PNG: " << path.string() << '\n';
        return false;
    }
    return true;
}

float at(const cv::Mat& alpha, int x, int y)
{
    return alpha.at<float>(y, x);
}

bool expectNear(const char* label, float actual, float expected, float tolerance = 0.04f)
{
    if (std::abs(actual - expected) <= tolerance) return true;
    std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
    return false;
}

bool renderCase(const std::filesystem::path& directory, const char* name,
                const LayerMask& mask, bool& passed)
{
    cv::Mat alpha;
    mask.compositeAlphaMask(kWidth, kHeight, &alpha);
    if (alpha.empty() || alpha.type() != CV_32FC1) {
        std::cerr << name << ": compositor returned an invalid alpha plane\n";
        passed = false;
        return false;
    }
    if (!writePng(directory / (std::string(name) + ".png"), alpha)) {
        std::cerr << name << ": failed to write PNG output\n";
        passed = false;
        return false;
    }
    return true;
}
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: MaskVisualTest <output-directory>\n";
        return 2;
    }
    const std::filesystem::path outputDirectory(argv[1]);
    std::filesystem::create_directories(outputDirectory);
    bool passed = true;

    LayerMask base;
    base.addMaskPath(rectangle(32, 28, 150, 156));
    cv::Mat baseline;
    base.compositeAlphaMask(kWidth, kHeight, &baseline);
    passed &= expectNear("single mask inside", at(baseline, 80, 80), 1.0f);
    passed &= expectNear("single mask outside", at(baseline, 12, 12), 0.0f);
    renderCase(outputDirectory, "01_single_base", base, passed);

    LayerMask opacity = base;
    opacity.addMaskPath(rectangle(96, 44, 218, 168, MaskMode::Add, 0.35f));
    cv::Mat alpha;
    opacity.compositeAlphaMask(kWidth, kHeight, &alpha);
    passed &= expectNear("add opacity only", at(alpha, 188, 100), 0.35f);
    passed &= expectNear("add union overlap", at(alpha, 120, 100), 1.0f);
    renderCase(outputDirectory, "02_add_opacity", opacity, passed);

    LayerMask subtract = base;
    subtract.addMaskPath(rectangle(76, 62, 132, 124, MaskMode::Subtract));
    subtract.compositeAlphaMask(kWidth, kHeight, &alpha);
    passed &= expectNear("subtract hole", at(alpha, 100, 90), 0.0f);
    passed &= expectNear("subtract exterior remains", at(alpha, 55, 90), 1.0f);
    renderCase(outputDirectory, "03_subtract", subtract, passed);

    LayerMask intersect = base;
    intersect.addMaskPath(rectangle(104, 40, 204, 136, MaskMode::Intersect));
    intersect.compositeAlphaMask(kWidth, kHeight, &alpha);
    passed &= expectNear("intersect shared area", at(alpha, 120, 80), 1.0f);
    passed &= expectNear("intersect removed area", at(alpha, 70, 80), 0.0f);
    renderCase(outputDirectory, "04_intersect", intersect, passed);

    LayerMask difference = base;
    difference.addMaskPath(rectangle(92, 44, 194, 146, MaskMode::Difference));
    difference.compositeAlphaMask(kWidth, kHeight, &alpha);
    passed &= expectNear("difference overlap", at(alpha, 120, 80), 0.0f);
    passed &= expectNear("difference base-only", at(alpha, 58, 80), 1.0f);
    passed &= expectNear("difference second-only", at(alpha, 174, 80), 1.0f);
    renderCase(outputDirectory, "05_difference", difference, passed);

    LayerMask expanded;
    expanded.addMaskPath(rectangle(72, 56, 164, 132, MaskMode::Add, 1.0f, 0.0f, 12.0f));
    expanded.compositeAlphaMask(kWidth, kHeight, &alpha);
    passed &= expectNear("expansion reaches border", at(alpha, 65, 90), 1.0f);
    passed &= expectNear("expansion leaves distant exterior", at(alpha, 28, 90), 0.0f);
    renderCase(outputDirectory, "06_expansion", expanded, passed);

    LayerMask contracted;
    contracted.addMaskPath(rectangle(72, 56, 164, 132, MaskMode::Add, 1.0f, 0.0f, -12.0f));
    contracted.compositeAlphaMask(kWidth, kHeight, &alpha);
    passed &= expectNear("negative expansion contracts", at(alpha, 72, 90), 0.0f);
    passed &= expectNear("negative expansion preserves center", at(alpha, 118, 90), 1.0f);
    renderCase(outputDirectory, "07_contraction", contracted, passed);

    LayerMask feathered;
    feathered.addMaskPath(rectangle(54, 42, 190, 150, MaskMode::Add, 0.8f, 10.0f));
    feathered.compositeAlphaMask(kWidth, kHeight, &alpha);
    passed &= expectNear("feathered core retains opacity", at(alpha, 120, 90), 0.8f);
    if (!(at(alpha, 51, 90) > 0.0f && at(alpha, 51, 90) < 0.8f)) {
        std::cerr << "feather boundary did not produce a partial alpha\n";
        passed = false;
    }
    renderCase(outputDirectory, "08_feather", feathered, passed);

    LayerMask inverted;
    inverted.addMaskPath(rectangle(80, 54, 176, 140, MaskMode::Add, 1.0f, 0.0f, 0.0f, true));
    inverted.compositeAlphaMask(kWidth, kHeight, &alpha);
    passed &= expectNear("inverted interior", at(alpha, 120, 90), 0.0f);
    passed &= expectNear("inverted exterior", at(alpha, 25, 90), 1.0f);
    renderCase(outputDirectory, "09_inverted", inverted, passed);

    LayerMask matrix;
    matrix.addMaskPath(rectangle(24, 25, 126, 165, MaskMode::Add, 0.9f, 3.0f, 0.0f));
    matrix.addMaskPath(rectangle(70, 45, 170, 150, MaskMode::Subtract, 0.7f, 7.0f, 4.0f));
    matrix.addMaskPath(rectangle(125, 35, 232, 158, MaskMode::Add, 0.45f, 12.0f, -3.0f));
    matrix.addMaskPath(rectangle(100, 74, 205, 132, MaskMode::Difference, 0.6f, 2.0f));
    matrix.compositeAlphaMask(kWidth, kHeight, &alpha);
    double minimum = 0.0;
    double maximum = 0.0;
    cv::minMaxLoc(alpha, &minimum, &maximum);
    if (!(cv::checkRange(alpha) && minimum >= 0.0 && maximum <= 1.0)) {
        std::cerr << "parameter matrix contains values outside [0, 1]\n";
        passed = false;
    }
    renderCase(outputDirectory, "10_stack_parameter_matrix", matrix, passed);

    std::cout << (passed ? "Mask visual contract passed" : "Mask visual contract failed")
              << "; outputs: " << outputDirectory.string() << '\n';
    return passed ? 0 : 1;
}
