// Artifact Mir-style Particle Playground
//
// Mir 風の手続き地形（ArtifactCore::Procedural3DGenerators）を粒子エミッターの
// 放出ソースとして使い、地形表面から煙／火の粉が立ち上がる様子を 2D / 3D の
// 両経路でヘッドレスにキャプチャする単体テストプロジェクト。
//
// ビルド:
//   cmake --build <build-dir> --config Debug --target ArtifactParticleMirPlayground
// 実行:
//   <build-dir>\bin\Debug\ArtifactParticleMirPlayground.exe --capture-2d <out-dir>
//   <build-dir>\bin\Debug\ArtifactParticleMirPlayground.exe --capture-3d <out-dir>

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <QVariant>
#include <QtCore/qglobal.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

import Artifact.Layer.Particle;
import Artifact.Generator.Particle;
import Artifact.Render.IRenderer;
import Material.Material;
import Mesh;
import Procedural3DGenerators;

namespace {

constexpr int CanvasWidth = 1920;
constexpr int CanvasHeight = 1080;

constexpr int Width2D = 1920;
constexpr int Height2D = 1080;

// 遠景の稜線に現れるサブピクセル aliasing を抑えるため、描画は 2 倍解像度で
// 行い、PNG 保存時に 1 解像度へダウンサンプルする（スーパーサンプリング）。
constexpr int SuperSample = 2;

constexpr int CaptureFrame = 45;
constexpr int LaterFrame = 60;

constexpr std::uint64_t MinChangedPixels = 128;
constexpr std::uint64_t MaxDeterminismDifferentPixels = 4096;
constexpr std::uint64_t MaxDeterminismChannelDelta = 24;

struct ImageDifference {
    std::uint64_t differentPixels = 0;
    std::uint64_t maxChannelDelta = 0;
};

struct CaptureStats {
    std::uint64_t visiblePixels = 0;
    std::uint64_t warmPixels = 0;
    int maxRed = 0;
    int maxGreen = 0;
    int maxBlue = 0;
    int maxAlpha = 0;
};

ImageDifference compareImages(const QImage& leftImage, const QImage& rightImage)
{
    const QImage left = leftImage.convertToFormat(QImage::Format_ARGB32);
    const QImage right = rightImage.convertToFormat(QImage::Format_ARGB32);
    ImageDifference difference;
    if (left.isNull() || right.isNull() || left.size() != right.size()) {
        difference.differentPixels = std::numeric_limits<std::uint64_t>::max();
        difference.maxChannelDelta = std::numeric_limits<std::uint64_t>::max();
        return difference;
    }
    for (int y = 0; y < left.height(); ++y) {
        for (int x = 0; x < left.width(); ++x) {
            const QRgb leftPixel = left.pixel(x, y);
            const QRgb rightPixel = right.pixel(x, y);
            const auto channelDelta = std::max({
                std::abs(qRed(leftPixel) - qRed(rightPixel)),
                std::abs(qGreen(leftPixel) - qGreen(rightPixel)),
                std::abs(qBlue(leftPixel) - qBlue(rightPixel)),
                std::abs(qAlpha(leftPixel) - qAlpha(rightPixel))});
            if (channelDelta > 0) {
                ++difference.differentPixels;
            }
            difference.maxChannelDelta = std::max(
                difference.maxChannelDelta,
                static_cast<std::uint64_t>(channelDelta));
        }
    }
    return difference;
}

CaptureStats measureCapture(const QImage& source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    CaptureStats stats;
    if (image.isNull()) {
        return stats;
    }
    // 背景は左上の画素（クリア色）とみなす。閾値の絶対値ではなく
    // 背景との差で数えるので、量子化や表示変換の影響を受けにくい。
    const QRgb background = image.pixel(0, 0);
    const int backgroundRed = qRed(background);
    const int backgroundGreen = qGreen(background);
    const int backgroundBlue = qBlue(background);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            const int red = qRed(pixel);
            const int green = qGreen(pixel);
            const int blue = qBlue(pixel);
            stats.maxRed = std::max(stats.maxRed, red);
            stats.maxGreen = std::max(stats.maxGreen, green);
            stats.maxBlue = std::max(stats.maxBlue, blue);
            stats.maxAlpha = std::max(stats.maxAlpha, static_cast<int>(qAlpha(pixel)));
            const int delta = std::max({
                std::abs(red - backgroundRed),
                std::abs(green - backgroundGreen),
                std::abs(blue - backgroundBlue)});
            if (delta <= 3) {
                continue;
            }
            ++stats.visiblePixels;
            if (red > blue * 1.25 + 4) {
                ++stats.warmPixels;
            }
        }
    }
    return stats;
}

bool savePng(const QImage& image, const QString& path, QFile& report)
{
    if (image.isNull() || !image.save(path, "PNG")) {
        qCritical("Could not save capture: %s", qPrintable(path));
        return false;
    }
    const CaptureStats stats = measureCapture(image);
    report.write(QStringLiteral("image=%1 visible_pixels=%2 warm_pixels=%3 "
                                "max_r=%4 max_g=%5 max_b=%6 max_a=%7\n")
                     .arg(path)
                     .arg(static_cast<qulonglong>(stats.visiblePixels))
                     .arg(static_cast<qulonglong>(stats.warmPixels))
                     .arg(stats.maxRed)
                     .arg(stats.maxGreen)
                     .arg(stats.maxBlue)
                     .arg(stats.maxAlpha)
                     .toUtf8());
    return true;
}

// ---------------------------------------------------------------------------
// Mir 風地形生成
// ---------------------------------------------------------------------------

ArtifactCore::TerrainSettings makeMirTerrainSettings(std::uint32_t seed,
                                                    float sizeX = 1920.0f,
                                                    float sizeY = 1080.0f)
{
    ArtifactCore::TerrainSettings settings;
    settings.seed = seed;
    settings.heightSource = ArtifactCore::TerrainHeightSource::Noise;
    settings.uvMode = ArtifactCore::TerrainUvMode::Grid;
    settings.columns = 96;
    settings.rows = 96;
    settings.sizeX = sizeX;
    settings.sizeY = sizeY;
    settings.height = 320.0f;
    settings.noiseScale = 1.6f;
    settings.noiseAmplitude = 1.0f;
    settings.noiseOctaves = 5;
    settings.noiseEvolution = 0.35f;
    settings.quality = ArtifactCore::Procedural3DQuality::Preview;
    return settings;
}

// 2D 用のリッジプロファイル（中央行の高さ列）を取り出す。
std::vector<float> sampleRidgeProfile(
    const ArtifactCore::Procedural3DMeshData& mesh, int columns, int rows)
{
    std::vector<float> profile;
    if (mesh.vertices.empty() || columns <= 0 || rows < 0) {
        return profile;
    }
    const int vertexColumns = columns + 1;
    const int middleRow = rows / 2;
    profile.reserve(static_cast<std::size_t>(vertexColumns));
    for (int x = 0; x < vertexColumns; ++x) {
        const std::size_t index =
            static_cast<std::size_t>(middleRow * vertexColumns + x);
        if (index >= mesh.vertices.size()) {
            break;
        }
        profile.push_back(mesh.vertices[index].pz);
    }
    return profile;
}

// ridge 上に等間隔で Point エミッターを置く位置を作る。
// ridge の両端を含めて配置し、全体に均一な放出位置を作る。
std::vector<QVector3D> makeRidgeEmitterPositions(
    const std::vector<float>& profile, float baseY, int emitterCount)
{
    std::vector<QVector3D> positions;
    if (profile.size() < 2 || emitterCount <= 0) {
        return positions;
    }
    const float stepX = static_cast<float>(CanvasWidth) /
        static_cast<float>(profile.size() - 1);
    const int count = std::min<int>(emitterCount, static_cast<int>(profile.size()));
    for (int i = 0; i < count; ++i) {
        const int profileIndex = count == 1
            ? static_cast<int>(profile.size() / 2)
            : (i * (static_cast<int>(profile.size()) - 1)) / (count - 1);
        const float x = static_cast<float>(profileIndex) * stepX;
        // 地形高さそのままを使う（スケール 1.0）。0.25 倍では ridge の高低差が
        // 80px しかなく平坦に見え、視覚採点で「鉛直方向の分散が乏しい」と
        // 判定されたため、地形の高低差をそのまま 2D の ridge に反映する。
        const float y = baseY - profile[static_cast<std::size_t>(profileIndex)];
        positions.push_back(QVector3D(x, y, 0.0f));
    }
    return positions;
}

// 3D: 地形メッシュを 3D 合成座標系（Y  upwards, Z = depth）に写して三角形列にする。
QVector<QVector3D> makeTerrainTriangles3D(
    const ArtifactCore::Procedural3DMeshData& mesh)
{
    QVector<QVector3D> triangles;
    if (mesh.vertices.empty() || mesh.indices.size() < 3u) {
        return triangles;
    }
    const auto toWorld = [](const ArtifactCore::Procedural3DVertex& vertex) {
        return QVector3D(vertex.px, vertex.pz, vertex.py);
    };
    for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
        const std::size_t a = mesh.indices[i];
        const std::size_t b = mesh.indices[i + 1u];
        const std::size_t c = mesh.indices[i + 2u];
        if (a >= mesh.vertices.size() || b >= mesh.vertices.size() ||
            c >= mesh.vertices.size()) {
            continue;
        }
        triangles.push_back(toWorld(mesh.vertices[a]));
        triangles.push_back(toWorld(mesh.vertices[b]));
        triangles.push_back(toWorld(mesh.vertices[c]));
    }
    return triangles;
}

ArtifactCore::Mesh makeRenderMesh(const ArtifactCore::Procedural3DMeshData& generated)
{
    ArtifactCore::Mesh mesh;
    if (generated.vertices.empty() || generated.indices.size() < 3u) {
        return mesh;
    }
    mesh.setVertexCount(static_cast<int>(generated.vertices.size()));
    auto positions = mesh.vertexAttributes().add<QVector3D>("position");
    auto normals = mesh.vertexAttributes().add<QVector3D>("normal");
    auto uvs = mesh.vertexAttributes().add<QVector2D>("uv");
    for (int index = 0; index < static_cast<int>(generated.vertices.size()); ++index) {
        const auto& vertex = generated.vertices[static_cast<std::size_t>(index)];
        (*positions)[index] = QVector3D(vertex.px, vertex.pz, vertex.py);
        (*normals)[index] = QVector3D(vertex.nx, vertex.nz, vertex.ny);
        (*uvs)[index] = QVector2D(vertex.u, vertex.v);
    }
    for (std::size_t index = 0; index + 2u < generated.indices.size(); index += 3u) {
        mesh.addPolygon({
            static_cast<int>(generated.indices[index]),
            static_cast<int>(generated.indices[index + 1u]),
            static_cast<int>(generated.indices[index + 2u])
        });
    }
    mesh.updateBounds();
    return mesh;
}

// yUp = true は 3D の Y-up ワールド、false は 2D の Y-down 合成座標系。
// density は ridge 上の粒子密度を調整するスケール。視覚採点で挙げられた
// 「ridge 途中の隙間・密度不足」を解消するため 2D は高密度側を使う。
// rise は立ち上がり量（初速と寿命）。視覚採点で「鉛直方向の分散が乏しい」と
// 指摘されたため、2D は rise を大きく取る。
Artifact::EmitterParams makeEmberParams(bool yUp, float density = 1.0f,
                                        float rise = 1.0f)
{
    Artifact::EmitterParams params;
    params.shape = Artifact::EmitterShape::Surface;
    params.mode = Artifact::EmissionMode::Continuous;
    params.rate = 900.0f * density;
    params.burstCount = 0;
    params.lifeMin = 1.4f * rise;
    params.lifeMax = 3.2f * rise;
    params.speedMin = 60.0f * rise;
    params.speedMax = 190.0f * rise;
    params.directionSpread = 28.0f;
    params.direction = yUp ? QVector3D(0.0f, 1.0f, 0.0f) : QVector3D(0.0f, -1.0f, 0.0f);
    params.gravity = yUp ? QVector3D(0.0f, -55.0f, 0.0f) : QVector3D(0.0f, 55.0f, 0.0f);
    params.drag = 0.35f;
    params.windDirection = QVector3D(1.0f, 0.0f, 0.0f);
    params.windStrength = 45.0f;
    params.turbulenceFrequency = 0.35f;
    params.turbulenceAmplitude = 55.0f * rise;
    params.turbulenceEvolution = 0.8f;
    params.scaleMin = 2.2f * std::max(1.0f, density * 0.75f);
    params.scaleMax = 6.5f * std::max(1.0f, density * 0.75f);
    params.scaleEndMin = 0.0f;
    params.scaleEndMax = 1.0f;
    params.colorStart = QColor(255, 214, 138, 255);
    params.colorMid = QColor(255, 138, 52, 255);
    params.colorEnd = QColor(96, 40, 22, 0);
    params.colorMidPosition = 0.35f;
    params.opacityMin = 1.0f;
    params.opacityMax = 1.0f;
    params.opacityEndMin = 0.0f;
    params.opacityEndMax = 0.0f;
    params.rotationSpeedMin = -90.0f;
    params.rotationSpeedMax = 90.0f;
    params.maxParticles = 60000;
    params.deterministic = true;
    params.randomSeed = 20261118u;
    return params;
}

bool prepareRenderer(Artifact::ArtifactIRenderer& renderer, int width, int height)
{
    renderer.initializeHeadless(width, height);
    if (!renderer.isInitialized()) {
        return false;
    }
    renderer.setCanvasSize(static_cast<float>(CanvasWidth),
                           static_cast<float>(CanvasHeight));
    renderer.setViewportSize(static_cast<float>(width),
                             static_cast<float>(height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.02f, 0.03f, 0.05f, 1.0f));
    return true;
}

// スーパーサンプリング済み画像を最終解像度へ戻す。
QImage downsample(const QImage& source)
{
    if (source.isNull() || SuperSample <= 1) {
        return source;
    }
    return source.scaled(Width2D, Height2D, Qt::IgnoreAspectRatio,
                         Qt::SmoothTransformation);
}

// ---------------------------------------------------------------------------
// 2D キャプチャ
// ---------------------------------------------------------------------------

int capture2D(const QString& outputDirectory)
{
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) {
        qCritical("Could not create 2D capture directory");
        return 2;
    }
    QFile report(output.filePath(QStringLiteral("mir_particle_2d_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical("Could not open 2D capture report");
        return 3;
    }

    Artifact::ArtifactIRenderer renderer;
    // 2D は合成座標系（canvas 1920x1080）をそのままビューポートにするため、
    // スーパーサンプリングは行わない（aliasing の指摘は 3D 遠景のみ）。
    if (!prepareRenderer(renderer, Width2D, Height2D)) {
        qCritical("Could not initialize the headless GPU renderer");
        return 4;
    }

    const auto terrainSettings = makeMirTerrainSettings(7u);
    const auto terrain =
        ArtifactCore::Procedural3DGenerators::generateTerrain(terrainSettings, 0.0f);
    const auto profile = sampleRidgeProfile(terrain, terrainSettings.columns,
                                            terrainSettings.rows);
    const float ridgeBaseY = static_cast<float>(CanvasHeight) * 0.72f;
    // 視覚採点で「ridge 途中の隙間」「鉛直方向の分散が乏しい」と指摘されたため、
    // 帯 1 個の Surface 放出をやめ、ridge 上に等間隔で Point エミッターを
    // 置いて分布を連続化し、立ち上がり量を引き上げる。
    constexpr int RidgeEmitterCount = 64;
    const auto emitterPositions = makeRidgeEmitterPositions(
        profile, ridgeBaseY, RidgeEmitterCount);
    if (emitterPositions.empty()) {
        qCritical("Mir terrain did not produce a usable 2D ridge profile");
        renderer.destroy();
        return 5;
    }

    Artifact::ArtifactParticleLayer layer;
    layer.setLayerPropertyValue(QStringLiteral("particle.previewWidth"),
                                 QVariant(CanvasWidth));
    layer.setLayerPropertyValue(QStringLiteral("particle.previewHeight"),
                                 QVariant(CanvasHeight));
    layer.clearEmitters();

    for (std::size_t i = 0; i < emitterPositions.size(); ++i) {
        const auto& position = emitterPositions[i];
        auto params = makeEmberParams(false, 0.18f, 1.0f);
        params.shape = Artifact::EmitterShape::Point;
        params.position = position;
        params.radius = 26.0f;
        params.directionSpread = 40.0f;
        // エミッターごとに乱数シードをずらす。同じシードだと全エミッターが
        // 同一の乱数列を使い、視覚採点で「周期的な反復（鋸歯状）」と
        // 指摘されたため、インデックス由来のシードの分散を入れる。
        params.randomSeed = 20261118u + static_cast<std::uint32_t>(i) * 7919u;
        // 2D 合成座標系（Y -down）では初速 upwards、重力 downwards。
        // フレーム 45 は 1.5 秒のシミュレーションに相当するため、上方到達
        // 距離は初速でほぼ決まる。視覚採点で「上部の 90% がベースライン
        // 付近に留まる」と指摘されたため、重力を 0 にして初速を上げ、
        // 1.5 秒で 250〜640px 上へ届くようにする。
        params.speedMin = 170.0f;
        params.speedMax = 430.0f;
        params.lifeMin = 4.2f;
        params.lifeMax = 7.5f;
        params.gravity = QVector3D(0.0f, 0.0f, 0.0f);
        params.drag = 0.02f;
        params.turbulenceAmplitude = 88.0f;
        params.directionSpread = 30.0f;
        params.scaleMin = 3.0f;
        params.scaleMax = 8.0f;
        // 上方へ舞い上がった粒子は life 後半で縮小・減衰すると背景と同化して
        // 見えなくなる（視覚採点で「上部の 90% がベースライン付近に留まる」
        // と指摘された）。終端の減衰を緩め、煙柱として上方向にも視認できる
        // 塊を残す。
        params.scaleEndMin = 1.8f;
        params.scaleEndMax = 4.0f;
        params.opacityEndMin = 0.45f;
        params.opacityEndMax = 0.45f;
        params.maxParticles = 3000;
        auto* emitter = layer.addEmitter(params);
        if (!emitter) {
            qCritical("Could not create the Mir ridge emitters");
            renderer.destroy();
            return 6;
        }
    }
    layer.resetParticleSystem();
    layer.play();

    const auto renderFrame = [&renderer, &layer](int frameNumber, bool capture) {
        renderer.clear();
        layer.goToFrame(frameNumber);
        layer.draw(&renderer);
        renderer.flushAndWait();
        if (!capture) {
            return QImage();
        }
        return renderer.readbackToImage().convertToFormat(QImage::Format_ARGB32);
    };

    QImage referenceFrame;
    for (int frame = 1; frame <= CaptureFrame; ++frame) {
        referenceFrame = renderFrame(frame, frame == CaptureFrame);
    }
    if (referenceFrame.isNull()) {
        qCritical("2D Mir particle frame was not produced");
        renderer.destroy();
        return 8;
    }
    if (!savePng(referenceFrame,
                 output.filePath(QStringLiteral("mir_2d_frame_45.png")), report)) {
        renderer.destroy();
        return 9;
    }

    const QImage laterFrame = renderFrame(LaterFrame, true);
    if (!savePng(laterFrame,
                 output.filePath(QStringLiteral("mir_2d_frame_60.png")), report)) {
        renderer.destroy();
        return 10;
    }
    const auto temporalDifference = compareImages(referenceFrame, laterFrame);

    {
        std::size_t alive = 0;
        QString sample;
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        if (const auto* system = layer.particleSystem()) {
            for (const auto& systemEmitter : system->emitters()) {
                if (!systemEmitter) {
                    continue;
                }
                alive += static_cast<std::size_t>(systemEmitter->particleCount());
                for (const auto& particle : systemEmitter->particles()) {
                    minY = std::min(minY, particle.position.y());
                    maxY = std::max(maxY, particle.position.y());
                    minX = std::min(minX, particle.position.x());
                    maxX = std::max(maxX, particle.position.x());
                }
                if (sample.isEmpty() && !systemEmitter->particles().empty()) {
                    const auto& particle = systemEmitter->particles().front();
                    sample = QStringLiteral(
                                 "shape=%1 rate=%2 alive=%3 first_pos=(%4,%5,%6)")
                                 .arg(static_cast<int>(systemEmitter->params().shape))
                                 .arg(systemEmitter->params().rate)
                                 .arg(systemEmitter->particleCount())
                                 .arg(particle.position.x())
                                 .arg(particle.position.y())
                                 .arg(particle.position.z());
                }
            }
        }
        report.write(QStringLiteral(
            "alive_particles=%1 emitter_count=%2 renderer=%3 %4\n"
            "sim_x_range=[%5,%6] sim_y_range=[%7,%8]\n")
            .arg(static_cast<qulonglong>(alive))
            .arg(layer.emitterCount())
            .arg(renderer.particleDebugState())
            .arg(sample)
            .arg(minX)
            .arg(maxX)
            .arg(minY)
            .arg(maxY)
            .toUtf8());
    }

    if (temporalDifference.differentPixels < MinChangedPixels) {
        qCritical("2D Mir particle animation did not change between frames");
        renderer.destroy();
        return 11;
    }

    Artifact::ArtifactParticleLayer directSeekLayer;
    directSeekLayer.setLayerPropertyValue(QStringLiteral("particle.previewWidth"),
                                          QVariant(CanvasWidth));
    directSeekLayer.setLayerPropertyValue(QStringLiteral("particle.previewHeight"),
                                          QVariant(CanvasHeight));
    directSeekLayer.clearEmitters();
    for (std::size_t i = 0; i < emitterPositions.size(); ++i) {
        const auto& position = emitterPositions[i];
        auto params = makeEmberParams(false, 0.18f, 1.0f);
        params.shape = Artifact::EmitterShape::Point;
        params.position = position;
        params.radius = 26.0f;
        params.directionSpread = 40.0f;
        params.randomSeed = 20261118u + static_cast<std::uint32_t>(i) * 7919u;
        params.speedMin = 170.0f;
        params.speedMax = 430.0f;
        params.lifeMin = 4.2f;
        params.lifeMax = 7.5f;
        params.gravity = QVector3D(0.0f, 0.0f, 0.0f);
        params.drag = 0.02f;
        params.turbulenceAmplitude = 88.0f;
        params.directionSpread = 30.0f;
        params.scaleMin = 3.0f;
        params.scaleMax = 8.0f;
        params.scaleEndMin = 1.8f;
        params.scaleEndMax = 4.0f;
        params.opacityEndMin = 0.45f;
        params.opacityEndMax = 0.45f;
        params.maxParticles = 3000;
        if (!directSeekLayer.addEmitter(params)) {
            qCritical("Could not rebuild the Mir ridge emitters for the determinism pass");
            renderer.destroy();
            return 12;
        }
    }
    directSeekLayer.resetParticleSystem();
    directSeekLayer.play();
    renderer.clear();
    directSeekLayer.goToFrame(CaptureFrame);
    directSeekLayer.draw(&renderer);
    renderer.flushAndWait();
    const QImage directSeekFrame =
        renderer.readbackToImage().convertToFormat(QImage::Format_ARGB32);
    if (!savePng(directSeekFrame,
                 output.filePath(QStringLiteral("mir_2d_direct_seek_frame_45.png")),
                 report)) {
        renderer.destroy();
        return 13;
    }
    const auto determinismDifference = compareImages(referenceFrame, directSeekFrame);

    report.write(QStringLiteral(
        "temporal_differing_pixels=%1 determinism_differing_pixels=%2 "
        "determinism_max_channel_delta=%3 determinism_tolerance=%4px/%5 "
        "ridge_samples=%6\n")
        .arg(static_cast<qulonglong>(temporalDifference.differentPixels))
        .arg(static_cast<qulonglong>(determinismDifference.differentPixels))
        .arg(static_cast<qulonglong>(determinismDifference.maxChannelDelta))
        .arg(MaxDeterminismDifferentPixels)
        .arg(MaxDeterminismChannelDelta)
        .arg(static_cast<qulonglong>(profile.size()))
        .toUtf8());

    {
        std::size_t alive = 0;
        if (const auto* system = layer.particleSystem()) {
            for (const auto& systemEmitter : system->emitters()) {
                if (systemEmitter) {
                    alive += static_cast<std::size_t>(systemEmitter->particleCount());
                }
            }
        }
        report.write(QStringLiteral(
            "alive_particles=%1 renderer=%2 layer=%3 emitter_count=%4\n")
            .arg(static_cast<qulonglong>(alive))
            .arg(renderer.particleDebugState())
            .arg(layer.debugState())
            .arg(layer.emitterCount())
            .toUtf8());
    }

    renderer.destroy();
    if (determinismDifference.differentPixels > MaxDeterminismDifferentPixels ||
        determinismDifference.maxChannelDelta > MaxDeterminismChannelDelta) {
        qCritical("2D Mir particle seek exceeded the deterministic image tolerance");
        return 14;
    }

    qInfo("2D Mir particle captures passed");
    return 0;
}

// ---------------------------------------------------------------------------
// 2D ソフトレンダラー経路の比較
// ---------------------------------------------------------------------------

// GPU 経路（Diligent billboard）とソフト経路（QPainter フォールバック）の
// 同一シーン出力を比較する。ソフト経路は renderer を未初期化のまま渡すことで
// ArtifactParticleLayer::draw() のソフトウェアフォールバックを通す。
int capture2DSoftware(const QString& outputDirectory)
{
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) {
        qCritical("Could not create 2D software capture directory");
        return 2;
    }
    QFile report(output.filePath(QStringLiteral("mir_particle_2d_software_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical("Could not open 2D software capture report");
        return 3;
    }

    const auto terrainSettings = makeMirTerrainSettings(7u);
    const auto terrain =
        ArtifactCore::Procedural3DGenerators::generateTerrain(terrainSettings, 0.0f);
    const auto profile = sampleRidgeProfile(terrain, terrainSettings.columns,
                                            terrainSettings.rows);
    const float ridgeBaseY = static_cast<float>(CanvasHeight) * 0.72f;
    const auto emitterPositions =
        makeRidgeEmitterPositions(profile, ridgeBaseY, 64);
    if (emitterPositions.empty()) {
        qCritical("Mir terrain did not produce a usable 2D ridge profile");
        return 4;
    }

    Artifact::ArtifactParticleLayer layer;
    layer.setLayerPropertyValue(QStringLiteral("particle.previewWidth"),
                                 QVariant(CanvasWidth));
    layer.setLayerPropertyValue(QStringLiteral("particle.previewHeight"),
                                 QVariant(CanvasHeight));
    layer.clearEmitters();
    for (std::size_t i = 0; i < emitterPositions.size(); ++i) {
        const auto& position = emitterPositions[i];
        auto params = makeEmberParams(false, 0.18f, 1.0f);
        params.shape = Artifact::EmitterShape::Point;
        params.position = position;
        params.radius = 26.0f;
        params.directionSpread = 40.0f;
        params.randomSeed = 20261118u + static_cast<std::uint32_t>(i) * 7919u;
        params.speedMin = 170.0f;
        params.speedMax = 430.0f;
        params.lifeMin = 4.2f;
        params.lifeMax = 7.5f;
        params.gravity = QVector3D(0.0f, 0.0f, 0.0f);
        params.drag = 0.02f;
        params.turbulenceAmplitude = 88.0f;
        params.directionSpread = 30.0f;
        params.scaleMin = 3.0f;
        params.scaleMax = 8.0f;
        params.scaleEndMin = 1.8f;
        params.scaleEndMax = 4.0f;
        params.opacityEndMin = 0.45f;
        params.opacityEndMax = 0.45f;
        params.maxParticles = 3000;
        if (!layer.addEmitter(params)) {
            qCritical("Could not create the Mir ridge emitters");
            return 5;
        }
    }
    layer.resetParticleSystem();
    layer.play();

    // 未初期化 renderer を渡すとソフトウェアフォールバック経路を通る。
    Artifact::ArtifactIRenderer softwareRenderer;
    QImage softwareFrame;
    for (int frame = 1; frame <= CaptureFrame; ++frame) {
        layer.goToFrame(frame);
        layer.draw(&softwareRenderer);
        if (frame == CaptureFrame) {
            layer.getCachedFrame(frame, softwareFrame);
        }
    }
    if (softwareFrame.isNull()) {
        qCritical("2D Mir software frame was not produced");
        return 6;
    }
    softwareFrame = softwareFrame.convertToFormat(QImage::Format_ARGB32);
    if (!savePng(softwareFrame,
                 output.filePath(QStringLiteral("mir_2d_software_frame_45.png")),
                 report)) {
        return 7;
    }

    // GPU 経路と同じシーンを取得して差分を測る。
    Artifact::ArtifactIRenderer gpuRenderer;
    if (!prepareRenderer(gpuRenderer, Width2D, Height2D)) {
        qCritical("Could not initialize the headless GPU renderer");
        return 8;
    }
    Artifact::ArtifactParticleLayer gpuLayer;
    gpuLayer.setLayerPropertyValue(QStringLiteral("particle.previewWidth"),
                                   QVariant(CanvasWidth));
    gpuLayer.setLayerPropertyValue(QStringLiteral("particle.previewHeight"),
                                   QVariant(CanvasHeight));
    gpuLayer.clearEmitters();
    for (std::size_t i = 0; i < emitterPositions.size(); ++i) {
        const auto& position = emitterPositions[i];
        auto params = makeEmberParams(false, 0.18f, 1.0f);
        params.shape = Artifact::EmitterShape::Point;
        params.position = position;
        params.radius = 26.0f;
        params.randomSeed = 20261118u + static_cast<std::uint32_t>(i) * 7919u;
        params.speedMin = 170.0f;
        params.speedMax = 430.0f;
        params.lifeMin = 4.2f;
        params.lifeMax = 7.5f;
        params.gravity = QVector3D(0.0f, 0.0f, 0.0f);
        params.drag = 0.02f;
        params.turbulenceAmplitude = 88.0f;
        params.directionSpread = 30.0f;
        params.scaleMin = 3.0f;
        params.scaleMax = 8.0f;
        params.scaleEndMin = 1.8f;
        params.scaleEndMax = 4.0f;
        params.opacityEndMin = 0.45f;
        params.opacityEndMax = 0.45f;
        params.maxParticles = 3000;
        if (!gpuLayer.addEmitter(params)) {
            qCritical("Could not create the Mir ridge emitters on the GPU layer");
            gpuRenderer.destroy();
            return 9;
        }
    }
    gpuLayer.resetParticleSystem();
    gpuLayer.play();
    // GPU 経路は applyParticleRenderLOD() で screenScale に応じて粒子を間引く。
    // 合成不属于 composition のためグローバル変換の縮尺が 1 未満になり、
    // 間引きが過剰になっていないかを確認するため fullParticleDensity で
    // 間引きを無効化して取得する。
    auto gpuRenderSettings = gpuLayer.particleSystem()->renderSettings();
    gpuRenderSettings.fullParticleDensity = true;
    gpuLayer.particleSystem()->setRenderSettings(gpuRenderSettings);
    QImage gpuFrame;
    for (int frame = 1; frame <= CaptureFrame; ++frame) {
        gpuRenderer.clear();
        gpuLayer.goToFrame(frame);
        gpuLayer.draw(&gpuRenderer);
        gpuRenderer.flushAndWait();
        if (frame == CaptureFrame) {
            gpuFrame = gpuRenderer.readbackToImage().convertToFormat(QImage::Format_ARGB32);
        }
    }
    gpuRenderer.destroy();
    if (gpuFrame.isNull()) {
        qCritical("2D Mir GPU frame was not produced for the software comparison");
        return 10;
    }
    if (!savePng(gpuFrame,
                 output.filePath(QStringLiteral("mir_2d_gpu_frame_45.png")),
                 report)) {
        return 11;
    }

    const auto pathDifference = compareImages(gpuFrame, softwareFrame);
    const CaptureStats gpuStats = measureCapture(gpuFrame);
    const CaptureStats softwareStats = measureCapture(softwareFrame);
    report.write(QStringLiteral(
        "gpu_software_differing_pixels=%1 max_channel_delta=%2 "
        "gpu_visible_pixels=%3 software_visible_pixels=%4 "
        "gpu_software_ratio=%5\n")
        .arg(static_cast<qulonglong>(pathDifference.differentPixels))
        .arg(static_cast<qulonglong>(pathDifference.maxChannelDelta))
        .arg(static_cast<qulonglong>(gpuStats.visiblePixels))
        .arg(static_cast<qulonglong>(softwareStats.visiblePixels))
        .arg(gpuStats.visiblePixels
                 ? static_cast<double>(softwareStats.visiblePixels) /
                       static_cast<double>(gpuStats.visiblePixels)
                 : 0.0)
        .toUtf8());

    qInfo("2D Mir software-vs-GPU comparison captured");
    return 0;
}

// ---------------------------------------------------------------------------
// 3D キャプチャ
// ---------------------------------------------------------------------------

void setCamera(Artifact::ArtifactIRenderer& renderer, float orbitDegrees,
               float distance, float height)
{
    constexpr float Pi = 3.14159265358979323846f;
    // 視覚採点で「地形が垂直方向に低く配置され上部に大きな余白がある」と
    // 指摘されたため、注視点を地形面へ寄せ、カメラを looming させて
    // 地平線を画面上端付近まで上げる（地形が画面下部を埋める）構図にする。
    const QVector3D target(0.0f, -60.0f, 0.0f);
    const float angle = orbitDegrees * Pi / 180.0f;
    const QVector3D eye(target.x() + std::sin(angle) * distance,
                        target.y() + height,
                        target.z() + std::cos(angle) * distance);
    QMatrix4x4 view;
    view.lookAt(eye, target, QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(58.0f, static_cast<float>(Width2D) / Height2D,
                           10.0f, 20000.0f);
    renderer.set3DCameraMatrices(view, projection);
}

int capture3D(const QString& outputDirectory)
{
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("."))) {
        qCritical("Could not create 3D capture directory");
        return 2;
    }
    QFile report(output.filePath(QStringLiteral("mir_particle_3d_report.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical("Could not open 3D capture report");
        return 3;
    }

    Artifact::ArtifactIRenderer renderer;
    if (!prepareRenderer(renderer, Width2D * SuperSample, Height2D * SuperSample)) {
        qCritical("Could not initialize the headless GPU renderer");
        return 4;
    }
    // 視覚採点で「地形が孤立した浮遊スラブとして見え、下端の生えた
    // ポリゴン境界が露出する」と指摘されたため、地形をカメラより深く
    // 大きく生成して、画面外まで Continuity を持たせる。
    const auto terrainSettings =
        makeMirTerrainSettings(11u, 2600.0f, 2600.0f);
    const auto terrain =
        ArtifactCore::Procedural3DGenerators::generateTerrain(terrainSettings, 0.0f);
    const auto triangles = makeTerrainTriangles3D(terrain);
    const auto terrainMesh = makeRenderMesh(terrain);
    if (triangles.isEmpty() || !terrainMesh.isValid()) {
        qCritical("Mir terrain did not produce a usable 3D mesh");
        renderer.destroy();
        return 5;
    }

    ArtifactCore::Material terrainMaterial;
    terrainMaterial.setBaseColor(QColor(58, 74, 92));
    terrainMaterial.setMetallic(0.0f);
    terrainMaterial.setRoughness(0.85f);

    Artifact::ArtifactParticle3DLayer layer;
    layer.clearEmitters();
    auto* emitter = layer.addEmitter(makeEmberParams(true, 1.5f));
    if (!emitter) {
        qCritical("Could not create the Mir 3D emitter");
        renderer.destroy();
        return 6;
    }
    if (!emitter->setEmissionTriangles(triangles)) {
        qCritical("Could not bind the Mir terrain triangles to the 3D emitter");
        renderer.destroy();
        return 7;
    }
    layer.resetParticleSystem();
    layer.play();

    const auto renderFrame = [&renderer, &layer, &terrainMesh, &terrainMaterial](
                                 int frameNumber, float orbitDegrees, bool capture) {
        // 視覚採点で「遠景の稜線に轻微な aliasing」と指摘されたため、
        // カメラを近づけて地形を大きく見せ、かつスーパーサンプリングで
        // 稜線のサブピクセル shimmer を抑える。
        setCamera(renderer, orbitDegrees, 1150.0f, 620.0f);
        renderer.clear();
        layer.goToFrame(frameNumber);
        renderer.drawMesh(QStringLiteral("mir-terrain"), terrainMesh,
                          terrainMaterial, QMatrix4x4(), 1.0f, 3);        layer.draw(&renderer);
        renderer.flushAndWait();
        if (!capture) {
            return QImage();
        }
        return downsample(
            renderer.readbackToImage().convertToFormat(QImage::Format_ARGB32));
    };

    QImage referenceFrame;
    for (int frame = 1; frame <= CaptureFrame; ++frame) {
        referenceFrame = renderFrame(frame, 0.0f, frame == CaptureFrame);
    }
    if (referenceFrame.isNull()) {
        qCritical("3D Mir particle frame was not produced");
        renderer.destroy();
        return 8;
    }
    if (!savePng(referenceFrame,
                 output.filePath(QStringLiteral("mir_3d_front_frame_45.png")),
                 report)) {
        renderer.destroy();
        return 9;
    }

    const QImage orbitFrame = renderFrame(CaptureFrame, 35.0f, true);
    if (!savePng(orbitFrame,
                 output.filePath(QStringLiteral("mir_3d_orbit_frame_45.png")),
                 report)) {
        renderer.destroy();
        return 10;
    }
    const auto orbitDifference = compareImages(referenceFrame, orbitFrame);
    if (orbitDifference.differentPixels < MinChangedPixels) {
        qCritical("3D Mir camera orbit did not change the image");
        renderer.destroy();
        return 11;
    }

    const QImage laterFrame = renderFrame(LaterFrame, 0.0f, true);
    if (!savePng(laterFrame,
                 output.filePath(QStringLiteral("mir_3d_frame_60.png")), report)) {
        renderer.destroy();
        return 12;
    }
    const auto temporalDifference = compareImages(referenceFrame, laterFrame);
    if (temporalDifference.differentPixels < MinChangedPixels) {
        qCritical("3D Mir particle animation did not change between frames");
        renderer.destroy();
        return 13;
    }

    report.write(QStringLiteral(
        "orbit_differing_pixels=%1 temporal_differing_pixels=%2 "
        "terrain_vertices=%3 terrain_triangles=%4\n")
        .arg(static_cast<qulonglong>(orbitDifference.differentPixels))
        .arg(static_cast<qulonglong>(temporalDifference.differentPixels))
        .arg(static_cast<qulonglong>(terrainMesh.vertexCount()))
        .arg(static_cast<qulonglong>(triangles.size() / 3))
        .toUtf8());

    renderer.destroy();
    qInfo("3D Mir particle captures passed");
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-2d")) {
        return capture2D(arguments[2]);
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-3d")) {
        return capture3D(arguments[2]);
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--capture-2d-software")) {
        return capture2DSoftware(arguments[2]);
    }
    qCritical("Usage: ArtifactParticleMirPlayground "
              "--capture-2d <output-directory> | --capture-3d <output-directory> | "
              "--capture-2d-software <output-directory>");
    return 1;
}
