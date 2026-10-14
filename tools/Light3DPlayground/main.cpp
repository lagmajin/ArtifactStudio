#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QTextStream>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

import Artifact.Layer.Light;
import Artifact.Layers.Model3D;
import Artifact.Render.IRenderer;
import Core.Light;
import Math.Vec;

namespace {

constexpr int Width = 640;
constexpr int Height = 480;

struct ImageMetrics {
    std::uint64_t visiblePixels = 0;
    std::uint64_t litPixels = 0;
    double meanLuma = 0.0;
    double meanRed = 0.0;
    double meanGreen = 0.0;
    double meanBlue = 0.0;
    bool finite = true;
};

ImageMetrics inspect(const QImage& image)
{
    ImageMetrics metrics;
    if (image.isNull() || image.width() != Width || image.height() != Height) {
        metrics.finite = false;
        return metrics;
    }
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            const double r = pixel.redF();
            const double g = pixel.greenF();
            const double b = pixel.blueF();
            if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) {
                metrics.finite = false;
                continue;
            }
            const double luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            if (luma > 0.035) {
                ++metrics.visiblePixels;
                red += r;
                green += g;
                blue += b;
                metrics.meanLuma += luma;
                if (luma > 0.08) ++metrics.litPixels;
            }
        }
    }
    if (metrics.visiblePixels != 0) {
        const double count = static_cast<double>(metrics.visiblePixels);
        metrics.meanLuma /= count;
        metrics.meanRed = red / count;
        metrics.meanGreen = green / count;
        metrics.meanBlue = blue / count;
    }
    return metrics;
}

ArtifactCore::Light makeCoreLight(const Artifact::ArtifactLightLayer& source)
{
    ArtifactCore::Light light;
    switch (source.lightType()) {
    case Artifact::LightType::Point: light.setType(ArtifactCore::LightType::Point); break;
    case Artifact::LightType::Spot: light.setType(ArtifactCore::LightType::Spot); break;
    case Artifact::LightType::Parallel: light.setType(ArtifactCore::LightType::Directional); break;
    case Artifact::LightType::Ambient: light.setType(ArtifactCore::LightType::Ambient); break;
    case Artifact::LightType::Area: light.setType(ArtifactCore::LightType::Area); break;
    }
    const auto color = source.color();
    light.setColor({color.r(), color.g(), color.b()});
    light.setIntensity(source.intensity().value / 100.0f);
    const QMatrix4x4 global = source.getGlobalTransform4x4();
    const QVector3D position = global.map(QVector3D(0.0f, 0.0f, 0.0f));
    QVector3D direction = global.mapVector(QVector3D(0.0f, 0.0f, 1.0f));
    if (direction.lengthSquared() > 1.0e-6f) direction.normalize();
    light.setPosition(ArtifactCore::Coordinates::worldPoint3FromQVector3D(position));
    light.setDirection(ArtifactCore::Coordinates::worldVectorFromQVector3D(direction));
    if (source.lightType() == Artifact::LightType::Point) {
        light.setRange(source.range());
    } else if (source.lightType() == Artifact::LightType::Spot) {
        light.setRange(source.coneLength());
        const float outer = source.coneAngle().value * 0.5f;
        const float inner = std::max(0.0f, source.coneAngle().value -
                                               source.coneFeather().value) * 0.5f;
        light.setCutoff(ArtifactCore::Units::Degrees{inner},
                        ArtifactCore::Units::Degrees{outer});
    } else if (source.lightType() == Artifact::LightType::Area) {
        light.setAreaSize(source.areaWidth(), source.areaHeight());
        light.setAreaShape(static_cast<int>(source.areaShape()));
    }
    // This capture isolates direct-light response from the separate shadow-map path.
    light.setCastsShadows(false);
    return light;
}

bool renderCase(Artifact::ArtifactIRenderer& renderer,
                Artifact::Artifact3DLayer& object,
                Artifact::ArtifactLightLayer* lightLayer,
                const QImage* controlImage,
                const QString& outputPath,
                QFile& report)
{
    std::vector<ArtifactCore::Light> lights;
    if (lightLayer) lights.push_back(makeCoreLight(*lightLayer));
    renderer.setSceneLights(lights);
    renderer.clear();
    object.draw(&renderer);
    renderer.flushAndWait();
    const QImage image = renderer.readbackToImage();
    if (image.isNull() || !image.save(outputPath, "PNG")) return false;

    const ImageMetrics metrics = inspect(image);
    std::uint64_t differingPixels = 0;
    if (controlImage && !controlImage->isNull() && controlImage->size() == image.size()) {
        const QImage actual = image.convertToFormat(QImage::Format_ARGB32);
        const QImage control = controlImage->convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < actual.height(); ++y) {
            for (int x = 0; x < actual.width(); ++x) {
                if (actual.pixel(x, y) != control.pixel(x, y)) ++differingPixels;
            }
        }
    }
    report.write(QStringLiteral(
        "%1 visible=%2 lit=%3 mean_luma=%4 mean_rgb=%5,%6,%7 "
        "control_diff_pixels=%8 finite=%9\n")
        .arg(QDir::toNativeSeparators(outputPath))
        .arg(static_cast<qulonglong>(metrics.visiblePixels))
        .arg(static_cast<qulonglong>(metrics.litPixels))
        .arg(metrics.meanLuma, 0, 'f', 5)
        .arg(metrics.meanRed, 0, 'f', 5)
        .arg(metrics.meanGreen, 0, 'f', 5)
        .arg(metrics.meanBlue, 0, 'f', 5)
        .arg(static_cast<qulonglong>(differingPixels))
        .arg(metrics.finite ? QStringLiteral("true") : QStringLiteral("false"))
        .toUtf8());
    if (!metrics.finite) return false;
    if (!lightLayer) return true;
    return metrics.visiblePixels > 300 && metrics.litPixels > 150 &&
           differingPixels > 200;
}

int capture(const QString& outputPath)
{
    QDir output(outputPath);
    if (!output.mkpath(QStringLiteral("."))) return 2;
    QFile report(output.filePath(QStringLiteral("light_3d_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 3;

    Artifact::ArtifactIRenderer renderer;
    renderer.initializeHeadless(Width, Height);
    if (!renderer.isInitialized()) {
        qCritical("Could not initialize the headless GPU renderer");
        return 4;
    }
    renderer.setCanvasSize(640.0f, 480.0f);
    renderer.setViewportSize(static_cast<float>(Width), static_cast<float>(Height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.012f, 0.016f, 0.025f, 1.0f));
    QMatrix4x4 view;
    view.lookAt(QVector3D(260.0f, -300.0f, 430.0f), QVector3D(),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(42.0f, static_cast<float>(Width) / Height,
                           10.0f, 3000.0f);
    renderer.set3DCameraMatrices(view, projection);

    Artifact::Artifact3DLayer object(Artifact::FixedGeometry3D::Sphere);
    object.setAffectedByLights(true);

    const QString controlPath = output.filePath(QStringLiteral("no_light_frame_0001.png"));
    bool passed = renderCase(renderer, object, nullptr, nullptr, controlPath, report);
    const QImage controlImage(controlPath);
    if (controlImage.isNull()) {
        renderer.destroy();
        qCritical("Could not load the no-light control frame");
        return 5;
    }

    const std::array<Artifact::LightType, 5> types = {
        Artifact::LightType::Point, Artifact::LightType::Spot,
        Artifact::LightType::Parallel, Artifact::LightType::Ambient,
        Artifact::LightType::Area};
    const std::array<QString, 5> names = {
        QStringLiteral("point"), QStringLiteral("spot"),
        QStringLiteral("parallel"), QStringLiteral("ambient"),
        QStringLiteral("area")};
    for (std::size_t index = 0; index < types.size(); ++index) {
        Artifact::ArtifactLightLayer light;
        light.setLightType(types[index]);
        light.setPosition3D({0.0f, 40.0f, 300.0f});
        light.setColor(ArtifactCore::FloatColor(
            index == 0 ? 1.0f : 0.28f,
            index == 1 ? 1.0f : 0.48f,
            index == 2 ? 1.0f : 0.82f, 1.0f));
        light.setIntensity(ArtifactCore::Units::Percent{100.0f});
        light.setRange(ArtifactCore::Units::Pixels{700.0f});
        light.setConeLength(ArtifactCore::Units::Pixels{700.0f});
        light.setConeAngle(ArtifactCore::Units::Degrees{70.0f});
        light.setConeFeather(ArtifactCore::Units::Degrees{24.0f});
        light.setAreaSize(ArtifactCore::Units::Pixels{260.0f},
                          ArtifactCore::Units::Pixels{180.0f});
        if (types[index] == Artifact::LightType::Spot ||
            types[index] == Artifact::LightType::Parallel) {
            light.setRotation3D(QVector3D(0.0f, 180.0f, 0.0f));
        }
        const QString file = output.filePath(names[index] + QStringLiteral("_frame_0001.png"));
        passed = renderCase(renderer, object, &light, &controlImage, file, report) && passed;
    }

    renderer.destroy();
    if (!passed) {
        qCritical("One or more 3D light image checks failed; see light_3d_report.txt");
        return 5;
    }
    qInfo("3D object and light layer captures passed pixel sanity checks");
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture")) {
        return capture(arguments[2]);
    }
    qCritical("Usage: ArtifactLight3DPlayground --capture <output-directory>");
    return 1;
}
