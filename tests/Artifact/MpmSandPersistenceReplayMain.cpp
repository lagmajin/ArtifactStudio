#include <QByteArray>
#include <QGuiApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QImage>
#include <QJsonObject>
#include <QVariant>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.MpmSand2D;
import Artifact.Layer.PhysicsBridge;
import Artifact.Layer.Result;
import Artifact.Project.Manager;
import Frame.Position;
import Physics.Mpm2D;
import Memory.SharedPtr;
import Utils.String.UniString;

namespace {

int fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

struct ProjectCleanup {
    Artifact::ArtifactProjectManager& manager;
    ~ProjectCleanup() { manager.closeCurrentProject(); }
};

bool closeFloat(float lhs, float rhs);

bool validateConstitutiveProjection()
{
    constexpr float pi = 3.14159265358979323846f;
    constexpr float lambda = 1.5f;
    constexpr float shear = 1.0f;
    const float alpha = std::sin(35.0f * pi / 180.0f) /
                        std::sqrt(2.0f);

    const auto hydrostatic = ArtifactCore::projectMpmGranularHencky2D(
        -0.1f, -0.1f, lambda, shear, alpha);
    if (!hydrostatic.valid || hydrostatic.projected ||
        !closeFloat(hydrostatic.logStretch0, -0.1f) ||
        !closeFloat(hydrostatic.logStretch1, -0.1f)) {
        std::cerr << "constitutive failure: hydrostatic valid=" << hydrostatic.valid
                  << " projected=" << hydrostatic.projected
                  << " stretches=" << hydrostatic.logStretch0 << ','
                  << hydrostatic.logStretch1 << '\n';
        return false;
    }

    const auto extension = ArtifactCore::projectMpmGranularHencky2D(
        0.05f, 0.02f, lambda, shear, alpha);
    if (!extension.valid || !extension.projected || !extension.extensionTip ||
        !closeFloat(extension.logStretch0, 0.0f) ||
        !closeFloat(extension.logStretch1, 0.0f)) {
        return false;
    }

    const auto plastic = ArtifactCore::projectMpmGranularHencky2D(
        -0.5f, 0.3f, lambda, shear, alpha);
    const auto invalidAlpha = ArtifactCore::projectMpmGranularHencky2D(
        -0.1f, -0.1f, lambda, shear, 1.0f);
    const auto invalidMaterial = ArtifactCore::projectMpmGranularHencky2D(
        -0.1f, -0.1f, -1.0f, 1.0f, alpha);
    const auto nonFiniteInput = ArtifactCore::projectMpmGranularHencky2D(
        std::numeric_limits<float>::quiet_NaN(), -0.1f,
        lambda, shear, alpha);
    return plastic.valid && plastic.projected && !plastic.extensionTip &&
        closeFloat(plastic.plasticMultiplier, 0.36289553f) &&
        closeFloat(plastic.logStretch0, -0.24339411f) &&
        closeFloat(plastic.logStretch1, 0.04339411f) &&
        closeFloat(plastic.logStretch0 + plastic.logStretch1, -0.2f) &&
        !invalidAlpha.valid && !invalidMaterial.valid && !nonFiniteInput.valid;
}

bool sameImage(const QImage& lhs, const QImage& rhs)
{
    if (lhs.isNull() || rhs.isNull() || lhs.size() != rhs.size() ||
        lhs.format() != rhs.format()) {
        return false;
    }
    for (int y = 0; y < lhs.height(); ++y) {
        const auto* left = reinterpret_cast<const QRgb*>(lhs.constScanLine(y));
        const auto* right = reinterpret_cast<const QRgb*>(rhs.constScanLine(y));
        for (int x = 0; x < lhs.width(); ++x) {
            if (left[x] != right[x]) return false;
        }
    }
    return true;
}

bool anyPixelDiffers(const QImage& lhs, const QImage& rhs)
{
    if (lhs.isNull() || rhs.isNull() || lhs.size() != rhs.size() ||
        lhs.format() != rhs.format()) {
        return false;
    }
    for (int y = 0; y < lhs.height(); ++y) {
        const auto* left = reinterpret_cast<const QRgb*>(lhs.constScanLine(y));
        const auto* right = reinterpret_cast<const QRgb*>(rhs.constScanLine(y));
        for (int x = 0; x < lhs.width(); ++x) {
            if (left[x] != right[x]) return true;
        }
    }
    return false;
}

bool closeFloat(float lhs, float rhs)
{
    return std::isfinite(lhs) && std::isfinite(rhs) &&
        std::abs(lhs - rhs) <= 1.0e-6f;
}

bool sameSimulationState(const ArtifactCore::MpmSnapshot2D& lhs,
                         const ArtifactCore::MpmSnapshot2D& rhs)
{
    if (lhs.particles.size() != rhs.particles.size() ||
        lhs.fracturedIndices != rhs.fracturedIndices ||
        lhs.particleGridColumns != rhs.particleGridColumns ||
        lhs.particleGridRows != rhs.particleGridRows ||
        lhs.solverGridColumns != rhs.solverGridColumns ||
        lhs.solverGridRows != rhs.solverGridRows ||
        lhs.gridExpansionLimitReached != rhs.gridExpansionLimitReached ||
        !closeFloat(lhs.gridOrigin.x, rhs.gridOrigin.x) ||
        !closeFloat(lhs.gridOrigin.y, rhs.gridOrigin.y) ||
        !closeFloat(lhs.accumulatedTime, rhs.accumulatedTime) ||
        !closeFloat(lhs.physicsWorldAccumulator, rhs.physicsWorldAccumulator) ||
        !closeFloat(lhs.solverGridCellSize, rhs.solverGridCellSize)) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.particles.size(); ++i) {
        const auto& a = lhs.particles[i];
        const auto& b = rhs.particles[i];
        if (!closeFloat(a.pos.x, b.pos.x) || !closeFloat(a.pos.y, b.pos.y) ||
            !closeFloat(a.vel.x, b.vel.x) || !closeFloat(a.vel.y, b.vel.y) ||
            !closeFloat(a.mass, b.mass) || !closeFloat(a.volume0, b.volume0) ||
            !closeFloat(a.F.m00, b.F.m00) || !closeFloat(a.F.m01, b.F.m01) ||
            !closeFloat(a.F.m10, b.F.m10) || !closeFloat(a.F.m11, b.F.m11) ||
            !closeFloat(a.Fp.m00, b.Fp.m00) || !closeFloat(a.Fp.m01, b.Fp.m01) ||
            !closeFloat(a.Fp.m10, b.Fp.m10) || !closeFloat(a.Fp.m11, b.Fp.m11) ||
            !closeFloat(a.C.m00, b.C.m00) || !closeFloat(a.C.m01, b.C.m01) ||
            !closeFloat(a.C.m10, b.C.m10) || !closeFloat(a.C.m11, b.C.m11) ||
            !closeFloat(a.plasticStrain, b.plasticStrain) ||
            a.active != b.active || !closeFloat(a.r, b.r) ||
            !closeFloat(a.g, b.g) || !closeFloat(a.b, b.b)) {
            return false;
        }
    }
    return true;
}

bool captureSimulationState(
    Artifact::ArtifactAbstractComposition* composition,
    Artifact::ArtifactMpmSand2DLayer* layer,
    ArtifactCore::MpmSnapshot2D& output)
{
    if (!composition || !layer) return false;
    const auto solver = Artifact::LayerPhysicsBridge::granularMaterialSolver(
        composition->runtimePhysicsWorldId(), layer->id());
    if (!solver) return false;
    output = solver->snapshot();
    return !output.particles.empty();
}

QImage captureSand(Artifact::ArtifactMpmSand2DLayer* layer)
{
    if (!layer) return {};
    const QImage surface =
        layer->getSoftwareRenderSurface(640, 480, nullptr, true);
    return surface.isNull() ? QImage{}
                            : surface.convertToFormat(QImage::Format_ARGB32);
}

} // namespace

int main(int argc, char** argv)
{
    if (!validateConstitutiveProjection())
        return fail("Hencky-Drucker-Prager projection contract failed");
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return fail("could not create temporary directory");

    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", tempDir.path().toUtf8());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactMpmSandPersistenceReplay"));
    QStandardPaths::setTestModeEnabled(true);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("MpmSandPersistenceReplay"), true);
    if (!projects.isProjectCreated()) return fail("project creation failed");

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("MpmReplay")));
    compositionParams.setResolution(640, 480);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(48);
    const auto compositionResult = projects.createComposition(compositionParams);
    if (!compositionResult.success) return fail("composition creation failed");

    Artifact::ArtifactLayerInitParams layerParams(
        QStringLiteral("MPM Sand"), Artifact::LayerType::MpmSand2D);
    const auto layerResult = projects.addLayerToComposition(
        compositionResult.id, layerParams);
    if (!layerResult.success || !layerResult.layer)
        return fail("MPM sand layer creation/attachment failed");
    auto* sandLayer = dynamic_cast<Artifact::ArtifactMpmSand2DLayer*>(
        layerResult.layer.get());
    if (!sandLayer) return fail("factory returned the wrong layer type");
    const auto originalLayerId = sandLayer->id();
    if (!sandLayer->setLayerPropertyValue(
            QStringLiteral("mpmSand.columns"), QVariant(24)) ||
        !sandLayer->setLayerPropertyValue(
            QStringLiteral("mpmSand.rows"), QVariant(12)) ||
        !sandLayer->setLayerPropertyValue(
            QStringLiteral("mpmSand.gravityY"), QVariant(720.0))) {
        return fail("could not apply the round-trip simulation settings");
    }

    constexpr std::int64_t targetFrame = 18;
    const auto compositionLookup = projects.findComposition(compositionResult.id);
    if (!compositionLookup.success)
        return fail("created composition could not be resolved");
    auto composition = compositionLookup.ptr.lock();
    if (!composition) return fail("created composition pointer was null");

    ArtifactCore::MpmSnapshot2D stateBeforeRejectedEdit;
    if (!captureSimulationState(
            composition.get(), sandLayer, stateBeforeRejectedEdit)) {
        return fail("could not snapshot the MPM state before validation probe");
    }
    if (sandLayer->setLayerPropertyValue(
            QStringLiteral("mpmSand.rows"), QVariant(1))) {
        return fail("invalid MPM seed-grid dimensions were accepted");
    }
    ArtifactCore::MpmSnapshot2D stateAfterRejectedEdit;
    if (!captureSimulationState(
            composition.get(), sandLayer, stateAfterRejectedEdit)) {
        return fail("could not snapshot the MPM state after validation probe");
    }
    if (!sameSimulationState(stateBeforeRejectedEdit, stateAfterRejectedEdit) ||
        sandLayer->toJson().value(QStringLiteral("mpmSandRows")).toInt() != 12) {
        return fail("rejected MPM setting edit changed solver or authored state");
    }

    const QImage frameZero = captureSand(sandLayer);
    if (frameZero.isNull()) return fail("initial MPM surface capture failed");
    for (std::int64_t frame = 1; frame <= targetFrame; ++frame) {
        composition->setFramePosition(ArtifactCore::FramePosition(frame));
    }
    const QImage sequentialPlayback = captureSand(sandLayer);
    if (sequentialPlayback.isNull()) return fail("advanced MPM surface capture failed");
    if (!anyPixelDiffers(frameZero, sequentialPlayback))
        return fail("simulation did not visibly advance by the target frame");
    ArtifactCore::MpmSnapshot2D sequentialState;
    if (!captureSimulationState(composition.get(), sandLayer, sequentialState))
        return fail("could not snapshot the sequential MPM state");

    composition->goToFrame(0);
    composition->goToFrame(targetFrame);
    const QImage seekReplay = captureSand(sandLayer);
    ArtifactCore::MpmSnapshot2D seekState;
    if (!captureSimulationState(composition.get(), sandLayer, seekState))
        return fail("could not snapshot the seek-replayed MPM state");
    if (!sameSimulationState(sequentialState, seekState))
        return fail("seek replay changed the MPM solver state");
    if (!sameImage(sequentialPlayback, seekReplay))
        return fail("seek replay changed the rendered MPM surface");

    const QString projectPath = tempDir.filePath(
        QStringLiteral("mpm-sand-roundtrip.artifact"));
    const auto saveResult = projects.saveToFile(projectPath);
    if (!saveResult.success || !QFileInfo(projectPath).isFile())
        return fail("project save at the target frame failed");

    projects.closeCurrentProject();
    if (!projects.loadFromFile(projectPath)) return fail("project reload failed");
    const auto reloadedCompositionLookup =
        projects.findComposition(compositionResult.id);
    if (!reloadedCompositionLookup.success)
        return fail("composition was missing after reload");
    auto reloadedComposition = reloadedCompositionLookup.ptr.lock();
    if (!reloadedComposition)
        return fail("reloaded composition pointer was null");
    if (reloadedComposition->layerCount() != 1)
        return fail("project reload changed the composition layer count");
    auto reloadedLayerBase = reloadedComposition->frontMostLayer();
    auto* reloadedSand = dynamic_cast<Artifact::ArtifactMpmSand2DLayer*>(
        reloadedLayerBase.get());
    if (!reloadedSand) return fail("MPM sand layer was missing or had the wrong type");
    if (reloadedSand->id() != originalLayerId)
        return fail("MPM sand layer identity changed on project reload");
    if (reloadedComposition->framePosition().framePosition() != targetFrame)
        return fail("saved current frame was not restored on project reload");
    const QJsonObject reloadedSettings = reloadedSand->toJson();
    if (reloadedSettings.value(QStringLiteral("mpmSandColumns")).toInt() != 24 ||
        reloadedSettings.value(QStringLiteral("mpmSandRows")).toInt() != 12 ||
        std::abs(reloadedSettings.value(QStringLiteral("mpmSandGravityY")).toDouble() -
                 720.0) > 1.0e-6) {
        return fail("MPM sand authoring settings did not survive project reload");
    }
    const QImage reloadedAtSavedFrame = captureSand(reloadedSand);
    if (!sameImage(sequentialPlayback, reloadedAtSavedFrame))
        return fail("project reload did not reproduce the saved MPM frame");
    ArtifactCore::MpmSnapshot2D reloadedState;
    if (!captureSimulationState(
            reloadedComposition.get(), reloadedSand, reloadedState)) {
        return fail("could not snapshot the reloaded MPM state");
    }
    if (!sameSimulationState(sequentialState, reloadedState))
        return fail("project reload changed the MPM solver state");

    reloadedComposition->goToFrame(0);
    reloadedComposition->goToFrame(targetFrame);
    const QImage reloadedSeekReplay = captureSand(reloadedSand);
    ArtifactCore::MpmSnapshot2D reloadedSeekState;
    if (!captureSimulationState(
            reloadedComposition.get(), reloadedSand, reloadedSeekState)) {
        return fail("could not snapshot the post-reload replay state");
    }
    if (!sameSimulationState(sequentialState, reloadedSeekState))
        return fail("post-reload seek replay changed the MPM solver state");
    if (!sameImage(sequentialPlayback, reloadedSeekReplay))
        return fail("post-reload seek replay changed the rendered MPM surface");

    std::cout << "PASS: MPM constitutive projection, playback, seek, project reload, and replay invariants hold (state tolerance 1e-6; rendered pixels exact)\n";
    return 0;
}
