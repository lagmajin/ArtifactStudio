#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <QByteArray>
#include <QGuiApplication>
#include <QImage>
#include <QColor>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>

import Geometry.Fracture;
import Physics2D;

using namespace ArtifactCore;

namespace {

class FractureImageTestEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char appName[] = "ArtifactCoreFracturePlaneImageTest";
        static char* argv[] = {appName, nullptr};
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
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

FractureImageTestEnvironment* const testEnv = [] {
    auto* env = new FractureImageTestEnvironment();
    testing::AddGlobalTestEnvironment(env);
    return env;
}();

constexpr int kWidth = 256;
constexpr int kHeight = 192;

FractureSettings makePlaneSettings()
{
    FractureSettings settings;
    settings.pattern = FracturePattern::Radial;
    settings.shardCount = 12;
    settings.debrisCount = 0;
    settings.impulseStrength = 90.0f;
    settings.angularStrength = 4.0f;
    settings.gravity = 60.0f;
    settings.damping = 0.99f;
    settings.lifetimeMin = 5.0f;
    settings.lifetimeMax = 5.0f;
    settings.seed = 1234;
    settings.protectedCenterRadius = 0.0f;
    return settings;
}

QImage makeSourceImage()
{
    QImage image(kWidth, kHeight, QImage::Format_RGBA8888);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const bool left = x < kWidth / 2;
            const bool top = y < kHeight / 2;
            QColor c = top ? (left ? QColor(70, 130, 200, 255) : QColor(220, 90, 70, 255))
                           : (left ? QColor(80, 180, 110, 255) : QColor(230, 200, 80, 255));
            image.setPixelColor(x, y, c);
        }
    }
    return image;
}

bool pointInPolygon(double x, double y, const QPolygonF& poly)
{
    bool inside = false;
    const int n = poly.size();
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const double xi = poly[i].x(), yi = poly[i].y();
        const double xj = poly[j].x(), yj = poly[j].y();
        if (((yi > y) != (yj > y)) &&
            (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

// Shard polygon is stored in absolute source coords; update() moves only
// shard.position, so the visual offset is (position - sourceCentroid).
QImage renderShards(const QImage& source, const std::vector<FractureShard>& shards)
{
    QImage out(kWidth, kHeight, QImage::Format_RGBA8888);
    out.fill(QColor(18, 20, 26, 255));
    for (const auto& shard : shards) {
        if (!shard.active || shard.polygon.size() < 3) {
            continue;
        }
        const double dx = shard.position.x() - shard.sourceCentroid.x();
        const double dy = shard.position.y() - shard.sourceCentroid.y();
        QPolygonF moved;
        for (const QPointF& p : shard.polygon) {
            moved << QPointF(p.x() + dx, p.y() + dy);
        }
        const QRectF bb = moved.boundingRect();
        const int x0 = std::max(0, static_cast<int>(std::floor(bb.left())));
        const int x1 = std::min(kWidth - 1, static_cast<int>(std::ceil(bb.right())));
        const int y0 = std::max(0, static_cast<int>(std::floor(bb.top())));
        const int y1 = std::min(kHeight - 1, static_cast<int>(std::ceil(bb.bottom())));
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                if (!pointInPolygon(x + 0.5, y + 0.5, moved)) {
                    continue;
                }
                // Sample the source at the un-moved location (UV equivalent).
                const int sx = std::clamp(static_cast<int>(std::round(x - dx)), 0, kWidth - 1);
                const int sy = std::clamp(static_cast<int>(std::round(y - dy)), 0, kHeight - 1);
                QColor c = source.pixelColor(sx, sy);
                c.setAlphaF(std::clamp(shard.opacity, 0.0f, 1.0f));
                out.setPixelColor(x, y, c);
            }
        }
    }
    return out;
}

std::filesystem::path previewDir()
{
    if (const char* env = std::getenv("ARTIFACT_FRACTURE_PREVIEW_DIR")) {
        if (*env) {
            return std::filesystem::path(env);
        }
    }
    // Repo外・build外の邪魔にならない既定: OSの一時ディレクトリ配下。
    return std::filesystem::temp_directory_path() / "artifact-fracture-preview";
}

} // namespace

TEST(FracturePlaneTest, SameSeedProducesSameShards)
{
    const QRectF bounds(0, 0, kWidth, kHeight);
    FractureEffect a, b;
    a.setSourceBounds(bounds);
    b.setSourceBounds(bounds);
    a.setImpactPoint(bounds.center());
    b.setImpactPoint(bounds.center());
    const FractureSettings settings = makePlaneSettings();
    a.setSettings(settings);
    b.setSettings(settings);
    ASSERT_TRUE(a.generate());
    ASSERT_TRUE(b.generate());
    ASSERT_TRUE(a.hasResult());
    const auto& sa = a.shards();
    const auto& sb = b.shards();
    ASSERT_EQ(sa.size(), sb.size());
    ASSERT_GE(sa.size(), 3u);
    for (std::size_t i = 0; i < sa.size(); ++i) {
        EXPECT_EQ(sa[i].polygon.size(), sb[i].polygon.size());
        EXPECT_FLOAT_EQ(sa[i].sourceCentroid.x(), sb[i].sourceCentroid.x());
        EXPECT_FLOAT_EQ(sa[i].sourceCentroid.y(), sb[i].sourceCentroid.y());
        EXPECT_FLOAT_EQ(sa[i].velocity.x(), sb[i].velocity.x());
        EXPECT_FLOAT_EQ(sa[i].velocity.y(), sb[i].velocity.y());
    }
}

TEST(FracturePlaneTest, SteppingKeepsMotionFinite)
{
    FractureEffect fx;
    fx.setSourceBounds(QRectF(0, 0, kWidth, kHeight));
    fx.setImpactPoint(QPointF(kWidth / 2.0, kHeight / 2.0));
    fx.setSettings(makePlaneSettings());
    ASSERT_TRUE(fx.generate());
    for (int step = 0; step < 60; ++step) {
        fx.update(1.0f / 30.0f);
    }
    for (const auto& shard : fx.shards()) {
        EXPECT_TRUE(std::isfinite(shard.position.x()));
        EXPECT_TRUE(std::isfinite(shard.position.y()));
        EXPECT_TRUE(std::isfinite(shard.velocity.x()));
        EXPECT_TRUE(std::isfinite(shard.velocity.y()));
        EXPECT_GE(shard.opacity, 0.0f);
        EXPECT_LE(shard.opacity, 1.0f);
    }
}

TEST(FractureImageLayerPreviewTest, WritesBeforeAndAfterFrames)
{
    const QImage source = makeSourceImage();
    FractureEffect fx;
    fx.setSourceBounds(QRectF(0, 0, kWidth, kHeight));
    fx.setImpactPoint(QPointF(kWidth / 2.0, kHeight / 2.0));
    fx.setSettings(makePlaneSettings());
    ASSERT_TRUE(fx.generate());
    ASSERT_TRUE(fx.hasResult());

    const std::filesystem::path dir = previewDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    ASSERT_FALSE(ec) << ec.message();

    // frame 0: intact source itself.
    const std::filesystem::path p0 = dir / "fracture_frame00_intact.png";
    ASSERT_TRUE(source.save(QString::fromStdString(p0.string()), "PNG"));

    // frame 1: right after shatter (no integration yet).
    const QImage shattered = renderShards(source, fx.shards());
    const std::filesystem::path p1 = dir / "fracture_frame01_shattered.png";
    ASSERT_TRUE(shattered.save(QString::fromStdString(p1.string()), "PNG"));

    // frame 2: after 20 integration steps (fragments have flown apart).
    for (int step = 0; step < 20; ++step) {
        fx.update(1.0f / 30.0f);
    }
    const QImage settled = renderShards(source, fx.shards());
    const std::filesystem::path p2 = dir / "fracture_frame02_settled.png";
    ASSERT_TRUE(settled.save(QString::fromStdString(p2.string()), "PNG"));

    // Regression guard: shatter must actually move pixels.
    int diffPixels = 0;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            if (settled.pixelColor(x, y) != source.pixelColor(x, y)) {
                ++diffPixels;
            }
        }
    }
    EXPECT_GT(diffPixels, 100) << "settled frame is identical to source";

    std::printf("ARTIFACT_FRACTURE_PREVIEW_DIR=%s\n", dir.string().c_str());
    std::printf("ARTIFACT_FRACTURE_FRAME00=%s\n", p0.string().c_str());
    std::printf("ARTIFACT_FRACTURE_FRAME01=%s\n", p1.string().c_str());
    std::printf("ARTIFACT_FRACTURE_FRAME02=%s\n", p2.string().c_str());
}

// Houdini式: Voronoi破砕 → Box2D剛体(RBD) → 床に崩落・堆積する。
// CoreのFractureEffect::update()は弾道のみで崩落しないため、
// 破片形状だけを借りてPhysics2Dのポリゴン剛体として落とす。
namespace {

struct RbdShardView {
    QPolygonF local;      // 重心原点のローカル形状 (ピクセル単位)
    QPointF centroid;     // 生成時の重心 (UV基準点)
    SharedPtr<RigidBody2D> body;
};

QImage renderRbdShards(const QImage& source, const std::vector<RbdShardView>& views,
                       float simScale)
{
    QImage out(kWidth, kHeight, QImage::Format_RGBA8888);
    out.fill(QColor(18, 20, 26, 255));
    for (const auto& view : views) {
        if (!view.body || view.local.size() < 3) {
            continue;
        }
        // 剛体位置はsim単位なのでピクセルへ戻す。回転は無次元でそのまま。
        const QVector2D pos = view.body->position() * simScale;
        const float angle = view.body->angle();
        const double c = std::cos(angle), s = std::sin(angle);
        auto toWorld = [&](double lx, double ly) {
            return QPointF(pos.x() + lx * c - ly * s, pos.y() + lx * s + ly * c);
        };
        QPolygonF world;
        for (const QPointF& p : view.local) {
            world << toWorld(p.x(), p.y());
        }
        const QRectF bb = world.boundingRect();
        const int x0 = std::max(0, static_cast<int>(std::floor(bb.left())));
        const int x1 = std::min(kWidth - 1, static_cast<int>(std::ceil(bb.right())));
        const int y0 = std::max(0, static_cast<int>(std::floor(bb.top())));
        const int y1 = std::min(kHeight - 1, static_cast<int>(std::ceil(bb.bottom())));
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                if (!pointInPolygon(x + 0.5, y + 0.5, world)) {
                    continue;
                }
                // 逆変換でローカル→生成時絶対座標に戻してサンプルする。
                const double dx = (x + 0.5) - pos.x(), dy = (y + 0.5) - pos.y();
                const double lx = dx * c + dy * s, ly = -dx * s + dy * c;
                const int sx = std::clamp(static_cast<int>(std::round(view.centroid.x() + lx)), 0, kWidth - 1);
                const int sy = std::clamp(static_cast<int>(std::round(view.centroid.y() + ly)), 0, kHeight - 1);
                out.setPixelColor(x, y, source.pixelColor(sx, sy));
            }
        }
    }
    return out;
}

} // namespace

void runRbdCollapsePattern(const FractureSettings& settings, const char* prefix)
{
    const QImage source = makeSourceImage();
    FractureEffect fx;
    // 床から離した上半分のスラブを破砕する。画像全体を割ると下段が床に
    // めり込んでスポーンし、支持鎖で全体が眠って崩落しない。
    const QRectF slab(28.0, 8.0, 200.0, 100.0);
    fx.setSourceBounds(slab);
    fx.setImpactPoint(slab.center());
    fx.setSettings(settings);
    ASSERT_TRUE(fx.generate());
    ASSERT_TRUE(fx.hasResult());
    ASSERT_GE(fx.shards().size(), 3u);

    Physics2D world;
    // Box2D v3.1は既定で最大速度400units/sにクランプする。ピクセル座標の
    // まま980px/s^2で落とすとクランプに当たり崩落が緩慢になるため、
    // テスト内だけで1/50スケール(=メートル換算)して使う。描画時に戻す。
    constexpr float kSimScale = 50.0f;
    world.setGravity(0.0f, 980.0f / kSimScale);
    constexpr float kFloorTop = kHeight - 8.0f;
    world.setStaticFloor(kFloorTop / kSimScale, (kWidth + 400.0f) / kSimScale,
                         20.0f / kSimScale, 0.5f);

    std::vector<RbdShardView> views;
    double startMeanY = 0.0;
    std::size_t index = 0;
    for (const auto& shard : fx.shards()) {
        if (shard.polygon.size() < 3) {
            continue;
        }
        const QPointF centroid(shard.sourceCentroid.x(), shard.sourceCentroid.y());
        // Houdiniのshatter同様、破片間に目地(隙間)を空ける。共有辺のままでは
        // Box2Dのpolygon skinで噛み合い、自立アーチ化して崩落せず眠る。
        constexpr double kShrink = 0.94;
        std::vector<QVector2D> simVerts;
        QPolygonF local;
        for (const QPointF& p : shard.polygon) {
            const double lx = (p.x() - centroid.x()) * kShrink;
            const double ly = (p.y() - centroid.y()) * kShrink;
            simVerts.emplace_back(static_cast<float>(lx) / kSimScale,
                                  static_cast<float>(ly) / kSimScale);
            local << QPointF(lx, ly);
        }
        auto body = world.addPolygonBody(centroid.x() / kSimScale, centroid.y() / kSimScale,
                                         simVerts, true, 1.0f, 0.4f, 0.2f);
        ASSERT_TRUE(body) << "polygon body creation failed at shard " << index;
        // 初速は破砕由来の外向き速度を弱めて与え、少しだけ回転も付ける。
        body->setLinearVelocity(
            QVector2D(shard.velocity.x() * 0.35f / kSimScale,
                      (shard.velocity.y() * 0.35f + 60.0f) / kSimScale));
        const float spin = ((index % 2 == 0) ? 1.0f : -1.0f) * (1.0f + index * 0.13f);
        body->setAngularVelocity(spin);
        views.push_back({local, centroid, body});
        startMeanY += centroid.y();
        ++index;
    }
    ASSERT_FALSE(views.empty());
    startMeanY /= views.size();

    const std::filesystem::path dir = previewDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    ASSERT_FALSE(ec) << ec.message();

    constexpr float kDt = 1.0f / 60.0f;
    auto saveFrame = [&](const char* name) {
        const QImage frame = renderRbdShards(source, views, kSimScale);
        const std::filesystem::path p = dir / (std::string(prefix) + name);
        EXPECT_TRUE(frame.save(QString::fromStdString(p.string()), "PNG"));
        std::printf("ARTIFACT_FRACTURE_%s%s=%s\n", prefix, name, p.string().c_str());
    };

    saveFrame("_frame00_burst.png");
    for (int step = 1; step <= 150; ++step) {
        world.step(kDt, 4);
        if (step == 12) {
            saveFrame("_frame01_falling.png");
        } else if (step == 25) {
            saveFrame("_frame02_impact.png");
        }
    }
    saveFrame("_frame03_piled.png");

    // 検証: 全剛体が有限値を保ち、全体として落下して床付近に留まる。
    double endMeanY = 0.0;
    int nearFloor = 0;
    for (const auto& view : views) {
        const QVector2D pos = view.body->position() * kSimScale;
        const QVector2D vel = view.body->linearVelocity() * kSimScale;
        EXPECT_TRUE(std::isfinite(pos.x()));
        EXPECT_TRUE(std::isfinite(pos.y()));
        EXPECT_TRUE(std::isfinite(view.body->angle()));
        EXPECT_TRUE(std::isfinite(vel.x()));
        EXPECT_TRUE(std::isfinite(vel.y()));
        endMeanY += pos.y();
        if (pos.y() > kFloorTop - 48.0f && pos.y() < kFloorTop + 24.0f) {
            ++nearFloor;
        }
    }
    endMeanY /= views.size();
    EXPECT_GT(endMeanY, startMeanY) << "shards did not fall overall";
    EXPECT_GE(nearFloor, static_cast<int>(views.size() / 2))
        << "fewer than half the shards rest near the floor";
}

FractureSettings makeRbdPatternSettings(FracturePattern pattern, quint32 seed)
{
    FractureSettings settings;
    settings.pattern = pattern;
    settings.shardCount = 14;
    settings.debrisCount = 0;
    settings.seed = seed;
    settings.protectedCenterRadius = 0.0f;
    return settings;
}

TEST(FractureRbdCollapseTest, VoronoiShardsPileOnFloor)
{
    runRbdCollapsePattern(makeRbdPatternSettings(FracturePattern::Voronoi, 777), "rbd");
}

TEST(FractureRbdCollapseTest, GridShardsPileOnFloor)
{
    runRbdCollapsePattern(makeRbdPatternSettings(FracturePattern::Grid, 42), "rbd_grid");
}

TEST(FractureRbdCollapseTest, RadialShardsPileOnFloor)
{
    runRbdCollapsePattern(makeRbdPatternSettings(FracturePattern::Radial, 7), "rbd_radial");
}

TEST(FractureRbdCollapseTest, HybridShardsPileOnFloor)
{
    runRbdCollapsePattern(makeRbdPatternSettings(FracturePattern::Hybrid, 99), "rbd_hybrid");
}

TEST(FracturePresetTest, AllPresetsAreFiniteAndGeneratable)
{
    const QRectF slab(28.0, 8.0, 200.0, 100.0);
    const FracturePreset presets[] = {
        FracturePreset::Glass, FracturePreset::Concrete, FracturePreset::Stone,
        FracturePreset::Metal, FracturePreset::Wood, FracturePreset::Dust,
    };
    for (FracturePreset preset : presets) {
        FractureSettings settings = makeFracturePreset(preset);
        EXPECT_GT(settings.shardCount, 0);
        EXPECT_GT(settings.lifetimeMax, settings.lifetimeMin);
        EXPECT_TRUE(std::isfinite(settings.impulseStrength));
        EXPECT_TRUE(std::isfinite(settings.shatterThreshold));
        FractureEffect fx;
        fx.setSourceBounds(slab);
        fx.setImpactPoint(slab.center());
        fx.setSettings(settings);
        EXPECT_TRUE(fx.generate()) << "preset=" << static_cast<int>(preset);
        EXPECT_FALSE(fx.shards().empty()) << "preset=" << static_cast<int>(preset);
    }
}
