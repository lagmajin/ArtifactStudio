#include <gtest/gtest.h>

#include <QApplication>
#include <QBuffer>
#include <QByteArray>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMatrix4x4>
#include <QRect>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#include <QTemporaryDir>
#include <QVariant>
#include <QVector3D>
#include <QWidget>

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <cmath>
#include <limits>
#include <thread>
#include <utility>

#include "../ArtifactTestMcpServer.h"
#include "../ArtifactTestWindowCapture.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <DebugOutput.h>
#include <Win32Debug.hpp>
#include <RenderDevice.h>

import Graphics;
import Application.AppSettings;

import Artifact.Composition.InitParams;
import Artifact.Composition.Abstract;
import Preview.Quality;
import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.GauusianBlur;
import Artifact.Effect.Rasterizer.DropShadow;
import Artifact.Effect.Rasterizer.Ripple;
import Artifact.Effect.Rasterizer.TurbulentDisplace;
import Artifact.Effect.Glow.ChromaticGlow;
import Artifact.Effect.Glow.EdgeBloom;
import Composition3D;
import Artifact.Layer.Light;
import Artifact.Layer.Camera;
import Artifact.Layer.AdjustableLayer;
import Artifact.Layers.Model3D;
import Artifact.Layer.Particle;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Text;
import Layer.Blend;
import Artifact.Service.Playback;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.CompositionHostFrameSnapshot;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Render.IRenderer;
import Artifact.Render.Context;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.OffscreenComposition;
import Color.Float;
import Core.Light;
import Frame.Position;
import Memory.SharedPtr;
import Utils.Id;
import Utils.String.UniString;
import Time.Rational;
import Size;

namespace {

class TestParticle3DLayer final : public Artifact::ArtifactParticleLayer {
public:
    void enable3DForTest() { setIs3D(true); }
};

std::atomic_bool diligentErrorDetected{false};
std::atomic<const char*> testPhase{"startup"};
std::array<wchar_t, 1024> crashDumpPath{};
std::atomic_bool crashDumpAttempted{false};

void writeCrashDump(EXCEPTION_POINTERS* exception)
{
    if (!exception || crashDumpPath[0] == 0 ||
        crashDumpAttempted.exchange(true)) return;
    const HANDLE file = CreateFileW(crashDumpPath.data(), GENERIC_WRITE, 0,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD error = file == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    bool saved = false;
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exception, FALSE};
        const auto type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithUnloadedModules |
            MiniDumpWithThreadInfo);
        saved = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
            file, type, &info, nullptr, nullptr) != FALSE;
        if (!saved) error = GetLastError();
        CloseHandle(file);
    }
    char path[3072]{};
    WideCharToMultiByte(CP_UTF8, 0, crashDumpPath.data(), -1,
        path, sizeof(path), nullptr, nullptr);
    char line[3200];
    const int length = std::snprintf(line, sizeof(line),
        "[CrashDump] saved=%d error=%lu path=%s\n", saved, error, path);
    DWORD written = 0;
    if (length > 0 && length < sizeof(line))
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), line, length, &written, nullptr);
}

void writeExceptionStack(EXCEPTION_POINTERS* exception, HANDLE output)
{
#ifdef _M_X64
    if (!exception || !exception->ContextRecord) return;
    CONTEXT context = *exception->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC = {context.Rip, 0, AddrModeFlat};
    frame.AddrFrame = {context.Rbp, 0, AddrModeFlat};
    frame.AddrStack = {context.Rsp, 0, AddrModeFlat};
    const HANDLE process = GetCurrentProcess();
    SymRefreshModuleList(process);
    for (int i = 0; i < 24 && frame.AddrPC.Offset != 0; ++i) {
        alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 256]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;
        const bool resolved = SymFromAddr(process, frame.AddrPC.Offset,
                                          &displacement, symbol);
        HMODULE module = nullptr;
        char modulePath[192] = "unknown";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(frame.AddrPC.Offset), &module))
            GetModuleFileNameA(module, modulePath, sizeof(modulePath));
        char line[384];
        const int length = std::snprintf(line, sizeof(line),
            "[FaultStack] 0x%llx %.160s+0x%llx %.80s+0x%llx\n",
            static_cast<unsigned long long>(frame.AddrPC.Offset),
            modulePath,
            static_cast<unsigned long long>(frame.AddrPC.Offset - reinterpret_cast<ULONG_PTR>(module)),
            resolved ? symbol->Name : "unknown",
            static_cast<unsigned long long>(displacement));
        DWORD written = 0;
        if (length > 0 && length < sizeof(line))
            WriteFile(output, line, static_cast<DWORD>(length), &written, nullptr);
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(),
                         &frame, &context, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr)) break;
    }
#endif
}

LONG WINAPI integrationUnhandledExceptionFilter(EXCEPTION_POINTERS* exception)
{
    const auto code = exception && exception->ExceptionRecord
        ? exception->ExceptionRecord->ExceptionCode : 0u;
    const auto address = exception && exception->ExceptionRecord
        ? exception->ExceptionRecord->ExceptionAddress : nullptr;
    constexpr char digits[] = "0123456789abcdef";
    char message[96] = "[UnhandledWindowsException] code=0x00000000 address=0x";
    std::size_t cursor = sizeof("[UnhandledWindowsException] code=0x") - 1;
    for (int shift = 28; shift >= 0; shift -= 4) {
        message[cursor++] = digits[(code >> shift) & 0xfu];
    }
    cursor = sizeof("[UnhandledWindowsException] code=0x00000000 address=0x") - 1;
    const auto addressValue = reinterpret_cast<ULONG_PTR>(address);
    for (int shift = static_cast<int>(sizeof(addressValue) * 8) - 4;
         shift >= 0; shift -= 4) {
        message[cursor++] = digits[(addressValue >> shift) & 0xfu];
    }
    message[cursor++] = '\n';
    DWORD written = 0;
    const HANDLE errorHandle = GetStdHandle(STD_ERROR_HANDLE);
    if (errorHandle != INVALID_HANDLE_VALUE && errorHandle != nullptr) {
        WriteFile(errorHandle, message, static_cast<DWORD>(cursor),
                  &written, nullptr);
        char phaseMessage[128];
        const int phaseLength = std::snprintf(phaseMessage, sizeof(phaseMessage),
            "[TestPhase] %s\n", testPhase.load(std::memory_order_relaxed));
        if (phaseLength > 0 && phaseLength < sizeof(phaseMessage))
            WriteFile(errorHandle, phaseMessage, static_cast<DWORD>(phaseLength), &written, nullptr);
#ifdef _M_X64
        if (exception && exception->ContextRecord) {
            const auto& context = *exception->ContextRecord;
            const auto* record = exception->ExceptionRecord;
            char contextMessage[384];
            const int contextLength = std::snprintf(contextMessage, sizeof(contextMessage),
                "[FaultContext] access=%llu memory=0x%llx rcx=0x%llx rdx=0x%llx "
                "r8=0x%llx r9=0x%llx rsp=0x%llx\n",
                record && record->NumberParameters >= 2 ? record->ExceptionInformation[0] : 0,
                record && record->NumberParameters >= 2 ? record->ExceptionInformation[1] : 0,
                context.Rcx, context.Rdx, context.R8, context.R9, context.Rsp);
            if (contextLength > 0 && contextLength < sizeof(contextMessage))
                WriteFile(errorHandle, contextMessage, contextLength, &written, nullptr);
        }
#endif
        writeExceptionStack(exception, errorHandle);
        HMODULE faultModule = nullptr;
        if (address && GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(address), &faultModule)) {
            char moduleMessage[512] = "[FaultModule] ";
            constexpr DWORD prefixSize = sizeof("[FaultModule] ") - 1;
            const DWORD pathSize = GetModuleFileNameA(faultModule,
                moduleMessage + prefixSize, sizeof(moduleMessage) - prefixSize - 1);
            if (pathSize > 0 && pathSize < sizeof(moduleMessage) - prefixSize - 1) {
                moduleMessage[prefixSize + pathSize] = '\n';
                WriteFile(errorHandle, moduleMessage, prefixSize + pathSize + 1,
                          &written, nullptr);
            }
        }
    }
    OutputDebugStringA(message);
    writeCrashDump(exception);
    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI integrationExceptionObserver(EXCEPTION_POINTERS* exception)
{
    // GoogleTest catches test-body SEH before the unhandled filter. Observe
    // access violations without swallowing them or changing exception dispatch.
    static thread_local bool reporting = false;
    if (!reporting && exception && exception->ExceptionRecord &&
        exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        reporting = true;
        integrationUnhandledExceptionFilter(exception);
        reporting = false;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void integrationDiligentMessageCallback(Diligent::DEBUG_MESSAGE_SEVERITY severity,
                                        const Diligent::Char* message,
                                        const Diligent::Char*,
                                        const Diligent::Char*, int)
{
    if (severity >= Diligent::DEBUG_MESSAGE_SEVERITY_ERROR) {
        diligentErrorDetected.store(true, std::memory_order_relaxed);
        std::fprintf(stderr, "[DiligentTestError] %s\n",
                     message ? message : "(empty Diligent error)");
    }
}

constexpr QSize kViewportSize{320, 240};

QVector3D averageOpaqueColor(const QImage& image,
                             const QRect& sample = QRect(20, 20, 100, 100))
{
    std::array<double, 3> totals{};
    int count = 0;
    const QRect boundedSample = sample.intersected(image.rect());
    for (int y = boundedSample.top(); y <= boundedSample.bottom(); ++y) {
        for (int x = boundedSample.left(); x <= boundedSample.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() < 8) {
                continue;
            }
            totals[0] += pixel.red();
            totals[1] += pixel.green();
            totals[2] += pixel.blue();
            ++count;
        }
    }
    if (count == 0) {
        return {};
    }
    return {static_cast<float>(totals[0] / count),
            static_cast<float>(totals[1] / count),
            static_cast<float>(totals[2] / count)};
}

struct LitPixelBounds {
    int count = 0;
    QRect bounds;
};

LitPixelBounds findLitPixels(const QImage& image)
{
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() < 8 ||
                std::max({pixel.red(), pixel.green(), pixel.blue()}) <= 32) {
                continue;
            }
            ++count;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }
    return {count, count > 0 ? QRect(left, top, right - left + 1,
                                    bottom - top + 1) : QRect{}};
}


} // namespace

namespace {

QApplication& application()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(integrationUnhandledExceptionFilter);
    static const bool symbols = SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    (void)symbols;
    static const auto observer = AddVectoredExceptionHandler(0, integrationExceptionObserver);
    (void)observer;
    if (qEnvironmentVariableIsSet(
            "ARTIFACT_TEST_TRIGGER_UNHANDLED_EXCEPTION")) {
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    }
    static int argc = 1;
    static char applicationName[] = "Model3DLightCompositionIntegrationTest";
    static char* argv[] = {applicationName, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

QString lightTypeName(Artifact::LightType type)
{
    switch (type) {
    case Artifact::LightType::Point: return QStringLiteral("point");
    case Artifact::LightType::Spot: return QStringLiteral("spot");
    case Artifact::LightType::Parallel: return QStringLiteral("parallel");
    case Artifact::LightType::Ambient: return QStringLiteral("ambient");
    case Artifact::LightType::Area: return QStringLiteral("area");
    }
    return QStringLiteral("unknown");
}

class TestAdjustableLayer final : public Artifact::ArtifactAdjustableLayer
{
public:
    void setSourceSizeForTest(const ArtifactCore::Size_2D& size)
    {
        setSourceSize(size);
    }
};

void exerciseCompositionLight(Artifact::LightType type,
                              bool verifyIncludeOnlyLinking = false,
                              bool verifyTwoPointLights = false,
                              bool verifyVisibilityAndModelOptOut = false,
                              const QString& modelPath = {},
                              bool verifyRayTracingRegistration = false,
                              bool verifyLightingDifference = false,
                              bool verifyAsyncWorkerRecording = false,
                              bool verifyRenderedChannel = false,
                              bool verifyDeferredShadowOverlap = false,
                              bool verifyDedicatedFrameThread = false,
                              bool verifyWorkerCompositionPresent = false,
                              bool verifyRamPreviewFallback = false)
{
    auto& app = application();
    struct RamPreviewStateRestore {
        Artifact::ArtifactPlaybackService* playback = nullptr;
        Artifact::ArtifactCompositionPtr previousComposition;
        bool previousEnabled = false;
        bool previousDiskEnabled = true;
        ~RamPreviewStateRestore()
        {
            if (!playback) return;
            playback->clearRamPreviewCache();
            playback->setRamPreviewEnabled(previousEnabled);
            playback->setDiskPreviewCacheEnabled(previousDiskEnabled);
            playback->setCurrentComposition(previousComposition);
        }
    } ramPreviewStateRestore;
    if (verifyRamPreviewFallback) {
        ramPreviewStateRestore.playback =
            Artifact::ArtifactPlaybackService::instance();
        ASSERT_NE(ramPreviewStateRestore.playback, nullptr);
        ramPreviewStateRestore.previousComposition =
            ramPreviewStateRestore.playback->currentComposition();
        ramPreviewStateRestore.previousEnabled =
            ramPreviewStateRestore.playback->isRamPreviewEnabled();
        ramPreviewStateRestore.previousDiskEnabled =
            ramPreviewStateRestore.playback->isDiskPreviewCacheEnabled();
    }
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);
    QCoreApplication::setApplicationName(
        QStringLiteral("Model3DLightCompositionIntegrationTest"));
    QStandardPaths::setTestModeEnabled(true);
    struct AntiAliasingSettingRestore {
        ArtifactCore::ArtifactAppSettings* settings = nullptr;
        int previousMode = 1;
        ~AntiAliasingSettingRestore()
        {
            if (settings) settings->setCompositionAntiAliasingMode(previousMode);
        }
    } antiAliasingSettingRestore;
    if (verifyAsyncWorkerRecording || verifyDeferredShadowOverlap) {
        antiAliasingSettingRestore.settings =
            ArtifactCore::ArtifactAppSettings::instance();
        if (antiAliasingSettingRestore.settings) {
            antiAliasingSettingRestore.previousMode =
                antiAliasingSettingRestore.settings->compositionAntiAliasingMode();
            antiAliasingSettingRestore.settings->setCompositionAntiAliasingMode(
                1);
        }
    }

    QWidget host;
    host.resize(kViewportSize);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized())
        << "The configured GPU backend could not initialize the production composition viewport";
    ASSERT_NE(controller.renderer(), nullptr)
        << "The initialized composition viewport did not expose its production renderer";

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("LightIntegration")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kViewportSize.width(), kViewportSize.height());
    params.setDurationFrames(100);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("light-integration")), params);

    auto model = ArtifactCore::makeShared<Artifact::Artifact3DLayer>();
    model->loadFromFile(modelPath.isEmpty()
        ? QString::fromUtf8(ARTIFACT_TEST_MODEL_OBJ_PATH) : modelPath);
    ASSERT_GT(model->mesh().vertexCount(), 0u)
        << "The OBJ fixture did not produce renderable mesh geometry";
    // Keep the generated low-poly sphere near the production viewport's
    // default model framing while exposing multiple surface normals to light.
    model->transform3D().setCurrentScale(60.0f, 60.0f);
    ASSERT_TRUE(model->setLayerPropertyValue(
        QStringLiteral("material.base.color"), QColor(255, 255, 255)));
    auto light = ArtifactCore::makeShared<Artifact::ArtifactLightLayer>();
    light->setLightType(type);
    light->setColor(ArtifactCore::FloatColor(1.0f, 0.04f, 0.02f, 1.0f));
    light->setIntensity(ArtifactCore::Units::Percent{
        type == Artifact::LightType::Ambient
            ? 100.0f : (verifyTwoPointLights ? 250.0f : 1000.0f)});
    if (verifyVisibilityAndModelOptOut) {
        light->setVisible(false);
    }
    if (verifyIncludeOnlyLinking) {
        light->setLightLinkMode(Artifact::LightLinkMode::IncludeOnly);
        light->setLinkedLayerIdsText(QStringLiteral("not-the-model-layer"));
    }
    if (type == Artifact::LightType::Point) {
        light->setRange(ArtifactCore::Units::Pixels{5000.0f});
        light->transform3D().setCurrentPositionZ(350.0f);
    } else if (type == Artifact::LightType::Spot) {
        light->setConeLength(ArtifactCore::Units::Pixels{5000.0f});
        light->setConeAngle(ArtifactCore::Units::Degrees{60.0f});
        light->setConeFeather(ArtifactCore::Units::Degrees{10.0f});
        light->transform3D().setCurrentPositionZ(350.0f);
        light->transform3D().setCurrentRotationY(180.0f);
    } else if (type == Artifact::LightType::Parallel) {
        light->transform3D().setCurrentRotationY(180.0f);
    } else if (type == Artifact::LightType::Area) {
        light->setAreaSize(ArtifactCore::Units::Pixels{1000.0f},
                           ArtifactCore::Units::Pixels{1000.0f});
        light->transform3D().setCurrentPositionZ(350.0f);
        light->transform3D().setCurrentRotationY(180.0f);
    }
    std::array<decltype(ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>()),
               16> deferredPacketLayers{};
    const int deferredPacketLayerCount = 16;
    if (verifyAsyncWorkerRecording || verifyDeferredShadowOverlap) {
        // The production recorder intentionally keeps batches below its
        // 16-packet threshold on the immediate path. Add tiny black packets
        // beneath the model so this integration case exercises the real
        // Composition base-to-overlay asynchronous recording seam without
        // materially changing the captured lighting image.
        for (int index = 0; index < deferredPacketLayerCount; ++index) {
            auto packetLayer =
                ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
            ASSERT_TRUE(packetLayer);
            deferredPacketLayers[static_cast<std::size_t>(index)] = packetLayer;
            packetLayer->setLayerName(
                QStringLiteral("Deferred worker packet %1").arg(index));
            packetLayer->setSize(1, 1);
            packetLayer->setColor(
                ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
            packetLayer->transform2D().setPosition(
                static_cast<float>(index), 0.0f);
            ASSERT_TRUE(composition->appendLayerBottom(packetLayer).success);
        }
    }
    ASSERT_TRUE(composition->appendLayerBottom(model).success);
    ASSERT_TRUE(composition->appendLayerTop(light).success);

    if (verifyWorkerCompositionPresent) {
        // Match the production viewport fit-to-composition zoom before
        // capturing matrices for the worker-owned offscreen pass.
        controller.setComposition(composition);
    }

    QImage workerPresentedReferenceImage;
    QString workerParityCaptureRoot;
    if (verifyWorkerCompositionPresent) {
        Artifact::CompositionHostFrameSnapshot hostFacts;
        hostFacts.hasHost = true;
        hostFacts.visible = true;
        hostFacts.logicalSize = kViewportSize;
        hostFacts.devicePixelRatio = host.devicePixelRatio();
        hostFacts.viewportZoom = controller.renderer()->getZoom();
        controller.renderer()->getPan(hostFacts.viewportPanX,
                                      hostFacts.viewportPanY);
        hostFacts.viewportTransformCaptured = true;
        hostFacts.compositionFramePosition =
            composition->framePosition().framePosition();
        hostFacts.compositionFrameCaptured = true;
        hostFacts.sceneMutationGeneration = 1;
        Artifact::CompositionRenderFramePacketCache packetCache;
        const auto snapshotCaptureStarted =
            std::chrono::steady_clock::now();
        const auto packet = packetCache.capture(hostFacts, composition);
        const double snapshotCaptureMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - snapshotCaptureStarted)
                .count();
        ASSERT_TRUE(packet.isValid());
        EXPECT_EQ(packetCache.sceneSerializationCount(), 1u);
        EXPECT_EQ(packet.compositionId, composition->id().toString());
        EXPECT_EQ(packet.host.compositionRevision, composition->revision());
        EXPECT_EQ(packet.host.compositionFramePosition,
                  composition->framePosition().framePosition());
        EXPECT_TRUE(packet.host.viewportTransformCaptured);
        EXPECT_FLOAT_EQ(packet.host.viewportZoom,
                        controller.renderer()->getZoom());
        float expectedViewportPanX = 0.0f;
        float expectedViewportPanY = 0.0f;
        controller.renderer()->getPan(expectedViewportPanX,
                                      expectedViewportPanY);
        EXPECT_FLOAT_EQ(packet.host.viewportPanX, expectedViewportPanX);
        EXPECT_FLOAT_EQ(packet.host.viewportPanY, expectedViewportPanY);

        const auto serializedLayers = packet.serializedComposition.object()
                                          .value(QStringLiteral("layers"))
                                          .toArray();
        const auto layerSnapshotStarted = std::chrono::steady_clock::now();
        size_t independentlySerializedLayerCount = 0;
        bool layerSnapshotsMatchPacket = true;
        for (const auto& layer : composition->allLayerRef()) {
            if (!layer) {
                continue;
            }
            const QJsonObject layerSnapshot = layer->toJson();
            bool foundMatchingLayer = false;
            for (const auto& serializedLayerValue : serializedLayers) {
                const QJsonObject serializedLayer =
                    serializedLayerValue.toObject();
                if (serializedLayer.value(QStringLiteral("id")).toString() ==
                    layer->id().toString()) {
                    foundMatchingLayer = serializedLayer == layerSnapshot;
                    break;
                }
            }
            layerSnapshotsMatchPacket =
                layerSnapshotsMatchPacket && foundMatchingLayer;
            ++independentlySerializedLayerCount;
        }
        const double independentLayerSnapshotMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - layerSnapshotStarted)
                .count();
        EXPECT_EQ(independentlySerializedLayerCount,
                  composition->allLayerRef().size());
        EXPECT_TRUE(layerSnapshotsMatchPacket)
            << "Per-layer JSON snapshots do not match the scene packet";

        const auto expectedLayerCount = composition->allLayerRef().size();
        const auto expectedCompositionSize =
            composition->effectiveCompositionSize();
        const auto expectedModelSourcePath = model->sourcePath();
        const auto expectedModelVertexCount = model->mesh().vertexCount();
        const auto modelLayerId = model->id();
        const auto workerDevice = controller.renderer()->device();
        const auto workerContext = controller.renderer()->immediateContext();
        ASSERT_NE(workerDevice, nullptr);
        ASSERT_NE(workerContext, nullptr);
        ASSERT_GT(expectedCompositionSize.width(), 0);
        ASSERT_GT(expectedCompositionSize.height(), 0);

        ASSERT_TRUE(packet.host.viewportCameraCaptured);
        const QMatrix4x4 viewportView = packet.host.viewportViewMatrix;
        const QMatrix4x4 viewportProjection =
            packet.host.viewportProjectionMatrix;

        const int workerPresentWidth = qMax(
            1, qRound(host.width() * host.devicePixelRatioF()));
        const int workerPresentHeight = qMax(
            1, qRound(host.height() * host.devicePixelRatioF()));
        const HWND workerPresentHwnd = CreateWindowExW(
            0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, workerPresentWidth, workerPresentHeight,
            reinterpret_cast<HWND>(host.winId()), nullptr,
            GetModuleHandleW(nullptr), nullptr);
        ASSERT_NE(workerPresentHwnd, nullptr);
        Artifact::CompositionRenderFramePacketMailbox framePacketMailbox;
        ASSERT_TRUE(framePacketMailbox.publish(packet, 1));

        const auto renderModelOffscreen = [&](
            Artifact::Artifact3DLayer* sceneModel) {
            if (!sceneModel) {
                return QImage();
            }
            Artifact::OffscreenCompositionRenderer offscreen(
                workerDevice, workerContext,
                static_cast<Diligent::Uint32>(expectedCompositionSize.width()),
                static_cast<Diligent::Uint32>(expectedCompositionSize.height()));
            offscreen.renderer()->set3DCameraMatrices(
                viewportView, viewportProjection);
            offscreen.renderer()->setGizmoCameraMatrices(
                viewportView, viewportProjection);
            offscreen.renderer()->setSceneLights({});
            return offscreen.renderLayerToQImage(
                ArtifactCore::FramePosition(packet.host.compositionFramePosition),
                sceneModel);
        };
        QImage sourceOwnedSceneImage;
        {
            sourceOwnedSceneImage = renderModelOffscreen(model.get());
        }
        ASSERT_FALSE(sourceOwnedSceneImage.isNull());
        ASSERT_GT(findLitPixels(sourceOwnedSceneImage).count, 0)
            << "The source model produced no visible pixels in the worker-clone render contract";

        const QString packetCaptureRoot = qEnvironmentVariable(
            "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
        workerParityCaptureRoot = packetCaptureRoot;

        struct WorkerSceneSnapshotResult {
            bool cloneCreated = false;
            bool cloneFrameSynchronized = false;
            bool cloneHasWorkerAffinity = false;
            bool cloneLayersHaveWorkerAffinity = false;
            bool cloneModelHasLoadedMesh = false;
            size_t workerAffineLayerCount = 0;
            QString compositionId;
            QString modelSourcePath;
            QSize compositionSize;
            QImage renderedImage;
            QImage unlitCompositionImage;
            QImage renderedCompositionImage;
            QImage presentedCompositionImage;
            QImage firstRenderedImage;
            DWORD renderThreadId = 0;
            DWORD presentThreadId = 0;
            quint64 presentSuccessCount = 0;
            quint64 frameRequestSequence = 0;
            bool presenterInitialized = false;
            bool presenterHasSwapChain = false;
            bool frameRequestStillCurrent = false;
            Diligent::RENDER_DEVICE_TYPE presenterDeviceType =
                Diligent::RENDER_DEVICE_TYPE_UNDEFINED;
            size_t layerCount = 0;
            size_t modelVertexCount = 0;
            double cloneRehydrateMs = 0.0;
            double cloneRendererInitMs = 0.0;
            double cloneRenderMs = 0.0;
            double cloneWarmRenderMs = 0.0;
            double presenterInitMs = 0.0;
        } snapshotResult;
        std::atomic_bool snapshotWorkerDone{false};
        std::atomic_bool guiTaskProcessed{false};
        std::atomic_bool guiTaskProcessedWhileWorkerActive{false};
        std::atomic_bool guiTaskPosted{false};
        std::thread snapshotWorker(
            [&framePacketMailbox, modelLayerId, workerDevice, workerContext,
             workerPresentHwnd, workerPresentWidth, workerPresentHeight,
             &snapshotResult, &snapshotWorkerDone, &guiTaskProcessed,
             &guiTaskProcessedWhileWorkerActive, &guiTaskPosted,
             &app, viewportView, viewportProjection]() mutable {
                Artifact::CompositionRenderFramePacket workerPacket;
                quint64 workerRequestSequence = 0;
                if (!framePacketMailbox.waitTakeLatest(
                        workerPacket, workerRequestSequence)) {
                    snapshotWorkerDone.store(true,
                                             std::memory_order_release);
                    return;
                }
                snapshotResult.frameRequestSequence = workerRequestSequence;
                guiTaskPosted.store(QMetaObject::invokeMethod(
                    &app,
                    [&snapshotWorkerDone, &guiTaskProcessed,
                     &guiTaskProcessedWhileWorkerActive] {
                        guiTaskProcessedWhileWorkerActive.store(
                            !snapshotWorkerDone.load(
                                std::memory_order_acquire),
                            std::memory_order_release);
                        guiTaskProcessed.store(true, std::memory_order_release);
                    },
                    Qt::QueuedConnection), std::memory_order_release);
                Artifact::ScopedRenderDeviceBinding deviceBinding(
                    workerDevice, workerContext);
                snapshotResult.renderThreadId = GetCurrentThreadId();
                const auto cloneStarted = std::chrono::steady_clock::now();
                const auto workerComposition =
                    Artifact::ArtifactAbstractComposition::fromJson(
                        workerPacket.serializedComposition);
                snapshotResult.cloneRehydrateMs =
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - cloneStarted)
                        .count();
                snapshotResult.cloneCreated =
                    workerPacket.isValid() && static_cast<bool>(workerComposition);
                if (workerComposition) {
                    snapshotResult.cloneFrameSynchronized =
                        workerPacket.applyCapturedFramePosition(
                            *workerComposition);
                    snapshotResult.cloneHasWorkerAffinity =
                        workerComposition->thread() == QThread::currentThread();
                    snapshotResult.compositionId =
                        workerComposition->id().toString();
                    snapshotResult.compositionSize =
                        workerComposition->effectiveCompositionSize();
                    snapshotResult.layerCount =
                        workerComposition->allLayerRef().size();
                    snapshotResult.cloneLayersHaveWorkerAffinity = true;
                    for (const auto& layer : workerComposition->allLayerRef()) {
                        if (!layer ||
                            layer->thread() != QThread::currentThread()) {
                            snapshotResult.cloneLayersHaveWorkerAffinity = false;
                            continue;
                        }
                        ++snapshotResult.workerAffineLayerCount;
                    }
                    const auto workerModel =
                        ArtifactCore::dynamicPointerCast<
                            Artifact::Artifact3DLayer>(
                            workerComposition->layerById(modelLayerId));
                    if (workerModel) {
                        snapshotResult.modelSourcePath =
                            workerModel->sourcePath();
                        snapshotResult.modelVertexCount =
                            workerModel->mesh().vertexCount();
                        snapshotResult.cloneModelHasLoadedMesh =
                            snapshotResult.modelVertexCount > 0;
                    }
                    const auto cloneRendererInitStarted =
                        std::chrono::steady_clock::now();
                    Artifact::OffscreenCompositionRenderer offscreen(
                        workerDevice, workerContext,
                        static_cast<Diligent::Uint32>(
                            workerPacket.host.compositionSize.width()),
                        static_cast<Diligent::Uint32>(
                            workerPacket.host.compositionSize.height()));
                    snapshotResult.cloneRendererInitMs =
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() -
                            cloneRendererInitStarted)
                            .count();
                    offscreen.renderer()->set3DCameraMatrices(
                        viewportView, viewportProjection);
                    offscreen.renderer()->setGizmoCameraMatrices(
                        viewportView, viewportProjection);
                    offscreen.renderer()->setSceneLights({});
                    for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
                        const auto renderStarted =
                            std::chrono::steady_clock::now();
                        QImage rendered = offscreen.renderLayerToQImage(
                            ArtifactCore::FramePosition(
                                workerPacket.host.compositionFramePosition),
                            workerModel.get());
                        const double renderMs =
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - renderStarted)
                                .count();
                        if (frameIndex == 0) {
                            snapshotResult.firstRenderedImage = rendered;
                            snapshotResult.cloneRenderMs = renderMs;
                        } else {
                            snapshotResult.cloneWarmRenderMs += renderMs;
                        }
                        snapshotResult.renderedImage = std::move(rendered);
                    }
                    snapshotResult.cloneWarmRenderMs *= 0.5;
                    snapshotResult.unlitCompositionImage =
                        offscreen.renderToQImage(
                            ArtifactCore::FramePosition(
                                workerPacket.host.compositionFramePosition),
                            workerComposition.get());
                    ArtifactCore::Light workerPointLight(
                        ArtifactCore::LightType::Point);
                    workerPointLight.setColor(
                        ArtifactCore::float3<float>{1.0f, 0.04f, 0.02f});
                    workerPointLight.setIntensity(10.0f);
                    workerPointLight.setPosition(
                        ArtifactCore::Coordinates::worldPoint3FromQVector3D(
                            QVector3D(0.0f, 0.0f, 350.0f)));
                    workerPointLight.setRange(
                        ArtifactCore::Units::Pixels{5000.0f});
                    const std::vector<ArtifactCore::Light> workerSceneLights{
                        workerPointLight};
                    offscreen.renderer()->setSceneLights(workerSceneLights);
                    snapshotResult.renderedCompositionImage =
                        offscreen.renderToQImage(
                            ArtifactCore::FramePosition(
                                workerPacket.host.compositionFramePosition),
                            workerComposition.get());

                    Artifact::ArtifactIRenderer presenter;
                    const auto presenterInitStarted =
                        std::chrono::steady_clock::now();
                    presenter.initializeOnRenderThread(
                        workerPresentHwnd, workerPresentWidth,
                        workerPresentHeight);
                    snapshotResult.presenterInitMs =
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() -
                            presenterInitStarted)
                            .count();
                    snapshotResult.presenterInitialized =
                        presenter.isInitialized();
                    snapshotResult.presenterHasSwapChain =
                        presenter.hasSwapChain();
                    if (const auto device = presenter.device()) {
                        snapshotResult.presenterDeviceType =
                            device->GetDeviceInfo().Type;
                    }
                    if (snapshotResult.presenterInitialized &&
                        snapshotResult.presenterHasSwapChain) {
                        presenter.setCanvasSize(
                            static_cast<float>(workerPresentWidth),
                            static_cast<float>(workerPresentHeight));
                        presenter.setViewportSize(
                            static_cast<float>(workerPresentWidth),
                            static_cast<float>(workerPresentHeight));
                        presenter.setClearColor(
                            ArtifactCore::FloatColor(0.01f, 0.02f, 0.04f,
                                                     1.0f));
                        presenter.clear();
                        if (auto* sceneSrv =
                                offscreen.renderTargetShaderResourceView()) {
                            presenter.drawSprite(
                                0.0f, 0.0f,
                                static_cast<float>(workerPresentWidth),
                                static_cast<float>(workerPresentHeight),
                                sceneSrv, 1.0f);
                            presenter.flush();
                            snapshotResult.presentedCompositionImage =
                                presenter.readbackToImage();
                            snapshotResult.frameRequestStillCurrent =
                                framePacketMailbox.isCurrent(
                                    workerRequestSequence);
                            if (snapshotResult.frameRequestStillCurrent) {
                                presenter.present();
                            }
                            snapshotResult.presentThreadId =
                                GetCurrentThreadId();
                            snapshotResult.presentSuccessCount =
                                presenter.presentSuccessCount();
                        }
                    }
                }
                snapshotWorkerDone.store(true, std::memory_order_release);
            });
        while (!snapshotWorkerDone.load(std::memory_order_acquire)) {
            app.processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        snapshotWorker.join();
        EXPECT_TRUE(guiTaskPosted.load(std::memory_order_acquire));
        EXPECT_TRUE(guiTaskProcessed.load(std::memory_order_acquire));
        EXPECT_TRUE(guiTaskProcessedWhileWorkerActive.load(
            std::memory_order_acquire))
            << "The GUI event loop did not process a queued task while the "
               "dedicated Composition worker was rendering and presenting";
        framePacketMailbox.close();
        DestroyWindow(workerPresentHwnd);
        EXPECT_TRUE(snapshotResult.cloneCreated);
        EXPECT_TRUE(snapshotResult.cloneFrameSynchronized);
        EXPECT_TRUE(snapshotResult.cloneHasWorkerAffinity);
        EXPECT_TRUE(snapshotResult.cloneLayersHaveWorkerAffinity)
            << "Every layer QObject must be owned by the worker before traversal";
        EXPECT_EQ(snapshotResult.workerAffineLayerCount,
                  snapshotResult.layerCount);
        EXPECT_EQ(snapshotResult.compositionId, composition->id().toString());
        EXPECT_EQ(snapshotResult.frameRequestSequence, 1u);
        EXPECT_TRUE(snapshotResult.frameRequestStillCurrent);
        EXPECT_EQ(snapshotResult.compositionSize, expectedCompositionSize);
        EXPECT_EQ(snapshotResult.layerCount, expectedLayerCount);
        EXPECT_TRUE(snapshotResult.cloneModelHasLoadedMesh);
        EXPECT_EQ(snapshotResult.modelSourcePath, expectedModelSourcePath);
        EXPECT_EQ(snapshotResult.modelVertexCount, expectedModelVertexCount);
        ASSERT_FALSE(snapshotResult.renderedImage.isNull());
        ASSERT_FALSE(snapshotResult.renderedCompositionImage.isNull());
        EXPECT_EQ(snapshotResult.renderedCompositionImage.size(),
                  sourceOwnedSceneImage.size());
        EXPECT_GT(findLitPixels(snapshotResult.renderedCompositionImage).count,
                  0)
            << "The worker-owned Composition traversal produced no visible model pixels";
        ASSERT_FALSE(snapshotResult.unlitCompositionImage.isNull());
        ASSERT_EQ(snapshotResult.renderedCompositionImage.size(),
                  snapshotResult.unlitCompositionImage.size());
        quint64 sceneLightChangedPixels = 0;
        for (int y = 0; y < snapshotResult.renderedCompositionImage.height(); ++y) {
            for (int x = 0; x < snapshotResult.renderedCompositionImage.width(); ++x) {
                const QColor litPixel =
                    snapshotResult.renderedCompositionImage.pixelColor(x, y);
                const QColor unlitPixel =
                    snapshotResult.unlitCompositionImage.pixelColor(x, y);
                const int rgbDifference =
                    std::abs(litPixel.red() - unlitPixel.red()) +
                    std::abs(litPixel.green() - unlitPixel.green()) +
                    std::abs(litPixel.blue() - unlitPixel.blue());
                if (rgbDifference > 6) ++sceneLightChangedPixels;
            }
        }
        EXPECT_GT(sceneLightChangedPixels, 0u)
            << "The worker-owned full Composition traversal did not apply its scene light";
        EXPECT_TRUE(snapshotResult.presenterInitialized);
        EXPECT_TRUE(snapshotResult.presenterHasSwapChain);
        EXPECT_NE(snapshotResult.presenterDeviceType,
                  Diligent::RENDER_DEVICE_TYPE_UNDEFINED);
        const QString requestedBackend = qEnvironmentVariable(
            "ARTIFACT_RENDER_BACKEND").trimmed().toLower();
        if (requestedBackend == QStringLiteral("d3d12")) {
            EXPECT_EQ(snapshotResult.presenterDeviceType,
                      Diligent::RENDER_DEVICE_TYPE_D3D12);
        } else if (requestedBackend == QStringLiteral("vulkan")) {
            EXPECT_EQ(snapshotResult.presenterDeviceType,
                      Diligent::RENDER_DEVICE_TYPE_VULKAN);
        }
        EXPECT_NE(snapshotResult.renderThreadId,
                  GetCurrentThreadId());
        EXPECT_EQ(snapshotResult.renderThreadId,
                  snapshotResult.presentThreadId);
        EXPECT_EQ(snapshotResult.presentSuccessCount, 1u);
        ASSERT_FALSE(snapshotResult.presentedCompositionImage.isNull());
        EXPECT_EQ(snapshotResult.presentedCompositionImage.size(),
                  QSize(workerPresentWidth, workerPresentHeight));
        const auto presentedLitPixels =
            findLitPixels(snapshotResult.presentedCompositionImage);
        EXPECT_GT(presentedLitPixels.count, 0)
            << "The worker-presented Composition image contains no visible model pixels";
        EXPECT_LT(std::abs(presentedLitPixels.bounds.center().x() -
                           workerPresentWidth / 2),
                  workerPresentWidth / 4)
            << "The worker-presented model is not centered in its viewport";
        EXPECT_LT(std::abs(presentedLitPixels.bounds.center().y() -
                           workerPresentHeight / 2),
                  workerPresentHeight / 4)
            << "The worker-presented model is not centered in its viewport";
        EXPECT_TRUE(snapshotResult.firstRenderedImage ==
                    snapshotResult.renderedImage)
            << "Repeated worker-owned model frames changed pixels";
        EXPECT_EQ(snapshotResult.renderedImage.size(),
                  sourceOwnedSceneImage.size());
        EXPECT_TRUE(snapshotResult.renderedImage == sourceOwnedSceneImage)
            << "The worker-owned Composition clone rendered different pixels";
        EXPECT_GT(findLitPixels(snapshotResult.renderedImage).count, 0);
        workerPresentedReferenceImage =
            snapshotResult.presentedCompositionImage;
        std::fprintf(stderr,
                     "[CompositionFramePacketProfile] layers=%zu captureMs=%.3f perLayerCaptureMs=%.3f workerRehydrateMs=%.3f offscreenRendererInitMs=%.3f firstRenderMs=%.3f warmRenderMs=%.3f presenterInitMs=%.3f\n",
                     expectedLayerCount, snapshotCaptureMs,
                     independentLayerSnapshotMs,
                     snapshotResult.cloneRehydrateMs,
                     snapshotResult.cloneRendererInitMs,
                     snapshotResult.cloneRenderMs,
                     snapshotResult.cloneWarmRenderMs,
                     snapshotResult.presenterInitMs);
        if (!packetCaptureRoot.isEmpty()) {
            ASSERT_TRUE(QDir().mkpath(packetCaptureRoot));
            EXPECT_TRUE(snapshotResult.renderedImage.save(
                QDir(packetCaptureRoot).filePath(
                    QStringLiteral("composition_worker_clone_offscreen.png")),
                "PNG"));
            EXPECT_TRUE(snapshotResult.renderedCompositionImage.save(
                QDir(packetCaptureRoot).filePath(
                    QStringLiteral("composition_worker_clone_full_offscreen.png")),
                "PNG"));
            EXPECT_TRUE(snapshotResult.presentedCompositionImage.save(
                QDir(packetCaptureRoot).filePath(
                    QStringLiteral("composition_worker_clone_presented_frame.png")),
                "PNG"));
        }
    }
    if (!verifyWorkerCompositionPresent) {
        controller.setComposition(composition);
    }
    const quint64 workerSubmitCountBeforeFrame =
        controller.renderer()->deferredWorkerSubmitCount();
    const quint64 shadowMapOverlapCountBeforeFrame =
        controller.renderer()->deferredWorkerShadowMapOverlapCount();
    const quint64 deferredContextWorkCountBeforeFrame =
        controller.renderer()->deferredContextWorkCount();
    controller.start();
    const auto waitForFrame = [&](quint64 afterSerial) {
        QEventLoop frameLoop;
        QTimer pollTimer;
        QTimer timeoutTimer;
        pollTimer.setInterval(5);
        timeoutTimer.setSingleShot(true);
        QObject::connect(&pollTimer, &QTimer::timeout, &frameLoop, [&] {
            if (controller.currentFrameSerial() > afterSerial) {
                frameLoop.quit();
            }
        });
        QObject::connect(&timeoutTimer, &QTimer::timeout,
                         &frameLoop, &QEventLoop::quit);
        pollTimer.start();
        timeoutTimer.start(5000);
        if (controller.currentFrameSerial() <= afterSerial) {
            frameLoop.exec();
        }
        pollTimer.stop();
        return controller.currentFrameSerial();
    };
    int presentedFrameCaptureFallbackCount = 0;
    int directRendererReadbackCount = 0;
    quint64 presentCountBeforeRequestedFrame = 0;
    const auto renderDirtyFrame = [&](quint64 afterSerial) {
        presentCountBeforeRequestedFrame =
            controller.renderer()->presentSuccessCount();
        controller.markRenderDirty();
        controller.renderOneFrame();
        return controller.currentFrameSerial() > afterSerial
            ? controller.currentFrameSerial() : quint64{0};
    };
    const auto readPresentedFrame = [&] {
        if (verifyDedicatedFrameThread || verifyWorkerCompositionPresent ||
            verifyRamPreviewFallback) {
            // Compare the same GPU render target on both backends. Presented
            // surface readback availability differs by backend and can select
            // a different capture path even when the rendered frame matches.
            ++directRendererReadbackCount;
            return controller.renderer()->readbackToImage();
        }
        QElapsedTimer timeout;
        timeout.start();
        while (controller.renderer()->presentSuccessCount() <=
                   presentCountBeforeRequestedFrame &&
               timeout.elapsed() < 5000) {
            app.processEvents(QEventLoop::AllEvents, 10);
        }
        if (controller.renderer()->presentSuccessCount() <=
            presentCountBeforeRequestedFrame) {
            return QImage();
        }
        QImage image = controller.readbackPresentedFrameImage();
        if (image.isNull()) {
            ++presentedFrameCaptureFallbackCount;
            image = controller.captureCurrentFrameImage();
        }
        return image;
    };
    const quint64 firstRenderedFrame = waitForFrame(0);
    if (firstRenderedFrame == 0) {
        controller.stop();
        controller.destroy();
        FAIL() << "The production render ticker did not render its dirty composition";
    }
    if (verifyDeferredShadowOverlap) {
        const auto device = controller.renderer()->device();
        EXPECT_TRUE(device) << "The production renderer has no Diligent device";
        if (device) {
            const QString requestedBackend =
                qEnvironmentVariable("ARTIFACT_RENDER_BACKEND")
                    .trimmed().toLower();
            const auto actualBackend = device->GetDeviceInfo().Type;
            if (requestedBackend == QStringLiteral("vulkan") ||
                requestedBackend == QStringLiteral("vk")) {
                EXPECT_EQ(actualBackend, Diligent::RENDER_DEVICE_TYPE_VULKAN)
                    << "The overlap case must run on Vulkan, without backend fallback";
            } else if (requestedBackend == QStringLiteral("d3d12") ||
                       requestedBackend == QStringLiteral("dx12")) {
                EXPECT_EQ(actualBackend, Diligent::RENDER_DEVICE_TYPE_D3D12)
                    << "The overlap case must run on D3D12, without backend fallback";
            }
        }
    }
    if (verifyAsyncWorkerRecording) {
        EXPECT_GT(controller.renderer()->deferredWorkerSubmitCount(),
                  workerSubmitCountBeforeFrame)
            << "The production Composition frame did not submit its base batch "
               "to the deferred recording worker";
    }
    app.processEvents(QEventLoop::AllEvents, 10);
    const quint64 baselineRenderedFrame = renderDirtyFrame(firstRenderedFrame);
    if (baselineRenderedFrame <= firstRenderedFrame) {
        controller.stop();
        controller.destroy();
        FAIL() << "The production render ticker did not complete the baseline frame";
    }
    if (verifyAsyncWorkerRecording || verifyDeferredShadowOverlap) {
        QElapsedTimer deferredContextWorkWait;
        deferredContextWorkWait.start();
        while (controller.renderer()->deferredContextWorkCount() <=
                   deferredContextWorkCountBeforeFrame &&
               deferredContextWorkWait.elapsed() < 5000) {
            app.processEvents(QEventLoop::AllEvents, 10);
        }
        EXPECT_GT(controller.renderer()->deferredContextWorkCount(),
                  deferredContextWorkCountBeforeFrame)
            << "The visible-3D Composition frame did not record FXAA on the "
               "deferred context worker; state="
            << controller.renderer()->deferredContextWorkDebugState().toStdString();
    }
    if (verifyDeferredShadowOverlap) {
        EXPECT_GT(controller.renderer()->deferredWorkerShadowMapOverlapCount(),
                  shadowMapOverlapCountBeforeFrame)
            << "The production spot-light shadow map did not overlap active "
               "deferred context recording";
        const quintptr guiThreadId = reinterpret_cast<quintptr>(
            QThread::currentThreadId());
        const quintptr shadowMapThreadId =
            controller.renderer()->lastShadowMapExecutionThreadId();
        EXPECT_NE(shadowMapThreadId, quintptr{0});
        EXPECT_NE(shadowMapThreadId, guiThreadId)
            << "The production shadow-map draw pass still ran on the GUI thread";
    }
    if (verifyRayTracingRegistration) {
        const QString rayTracingState = controller.renderer()->rayTracingDebugState();
        if (!rayTracingState.contains(QStringLiteral("supported=1"))) {
            controller.stop();
            controller.destroy();
            GTEST_SKIP() << "The selected renderer backend does not support the "
                            "production ray-tracing lane: "
                         << rayTracingState.toStdString();
        }
        EXPECT_TRUE(rayTracingState.contains(QStringLiteral("registeredBLAS=1")))
            << "The displayed FBX did not register its geometry with the RT manager: "
            << rayTracingState.toStdString();
        EXPECT_TRUE(rayTracingState.contains(QStringLiteral("activeInstances=1")))
            << "The FBX geometry did not become an active RT instance: "
            << rayTracingState.toStdString();
        EXPECT_TRUE(rayTracingState.contains(QStringLiteral("traceDispatches=1")))
            << "The renderer did not issue its current RT warmup dispatch: "
            << rayTracingState.toStdString();
        EXPECT_TRUE(rayTracingState.contains(QStringLiteral("lastTrace=1x1")))
            << "The observed RT dispatch was not the known 1x1 warmup trace: "
            << rayTracingState.toStdString();
    }
    const QImage baselineImage = readPresentedFrame();
    QImage ramPreviewFallbackImage;
    if (verifyRamPreviewFallback) {
        auto* playback = ramPreviewStateRestore.playback;
        const quint64 stableViewportFrame =
            renderDirtyFrame(baselineRenderedFrame);
        ASSERT_GT(stableViewportFrame, baselineRenderedFrame);
        playback->stop();
        playback->setDiskPreviewCacheEnabled(false);
        playback->setRamPreviewEnabled(true);
        playback->setCurrentComposition(composition);
        QImage cachedFrame(2, 2, QImage::Format_RGBA8888);
        cachedFrame.fill(QColor(255, 0, 0, 255));
        ASSERT_TRUE(playback->storeRamPreviewFrameImage(
            0, cachedFrame, QStringLiteral("controller-fallback-render"), false));
        const auto cachedState = playback->ramPreviewFrameState(0);
        ASSERT_TRUE(cachedState.ready);
        ASSERT_TRUE(cachedState.imageAvailable);
        const quint64 fallbackFrame = renderDirtyFrame(stableViewportFrame);
        ASSERT_GT(fallbackFrame, stableViewportFrame);
        // Stop the periodic tick before pumping any GUI events. Otherwise a
        // later viewport-transform frame can replace the RAM Preview frame
        // between render completion and the backbuffer readback.
        controller.stop();
        const QString captureDirectory = qEnvironmentVariable(
            "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
        ramPreviewFallbackImage = QImage(QDir(captureDirectory).filePath(
            QStringLiteral("composition_controller_ram_preview_before_present.png")));
        ASSERT_FALSE(ramPreviewFallbackImage.isNull());
        ASSERT_EQ(ramPreviewFallbackImage.size(), baselineImage.size());
        const QColor cachedCenter = ramPreviewFallbackImage.pixelColor(
            ramPreviewFallbackImage.width() / 2,
            ramPreviewFallbackImage.height() / 2);
        EXPECT_GT(cachedCenter.red(), 160)
            << "The RAM Preview frame did not show its red upload; center RGB="
            << cachedCenter.red() << ", " << cachedCenter.green() << ", "
            << cachedCenter.blue();
        EXPECT_GT(cachedCenter.red(), cachedCenter.green() * 2);
        playback->clearRamPreviewCache();
        playback->setRamPreviewEnabled(false);
    }
    if (verifyWorkerCompositionPresent) {
        ASSERT_FALSE(baselineImage.isNull())
            << "The production controller parity reference is empty";
        EXPECT_GT(findLitPixels(baselineImage).count, 0)
            << "The production controller parity reference has no lit pixels";
        ASSERT_FALSE(workerPresentedReferenceImage.isNull());
        ASSERT_EQ(baselineImage.size(),
                  workerPresentedReferenceImage.size());
        quint64 absoluteChannelError = 0;
        quint64 pixelsOverTolerance = 0;
        int maximumChannelError = 0;
        quint64 fixtureScenePixels = 0;
        quint64 fixtureScenePixelsOverTolerance = 0;
        quint64 fixtureSceneAbsoluteChannelError = 0;
        int fixtureSceneMaximumChannelError = 0;
        constexpr int channelTolerance = 2;
        for (int y = 0; y < baselineImage.height(); ++y) {
            for (int x = 0; x < baselineImage.width(); ++x) {
                const QColor guiPixel = baselineImage.pixelColor(x, y);
                const QColor workerPixel =
                    workerPresentedReferenceImage.pixelColor(x, y);
                const std::array<int, 4> differences{
                    std::abs(guiPixel.red() - workerPixel.red()),
                    std::abs(guiPixel.green() - workerPixel.green()),
                    std::abs(guiPixel.blue() - workerPixel.blue()),
                    std::abs(guiPixel.alpha() - workerPixel.alpha())};
                bool exceedsTolerance = false;
                for (const int difference : differences) {
                    absoluteChannelError +=
                        static_cast<quint64>(difference);
                    maximumChannelError =
                        std::max(maximumChannelError, difference);
                    exceedsTolerance =
                        exceedsTolerance || difference > channelTolerance;
                }
                if (exceedsTolerance) ++pixelsOverTolerance;
                const auto isFixtureLightPixel = [](const QColor& pixel) {
                    return pixel.red() > 80 &&
                           pixel.red() * 4 > pixel.green() * 5 &&
                           pixel.red() * 20 > pixel.blue() * 23;
                };
                if (isFixtureLightPixel(guiPixel) ||
                    isFixtureLightPixel(workerPixel)) {
                    ++fixtureScenePixels;
                    if (exceedsTolerance) {
                        ++fixtureScenePixelsOverTolerance;
                    }
                    for (const int difference : differences) {
                        fixtureSceneAbsoluteChannelError +=
                            static_cast<quint64>(difference);
                        fixtureSceneMaximumChannelError = std::max(
                            fixtureSceneMaximumChannelError, difference);
                    }
                }
            }
        }
        const quint64 pixelCount = static_cast<quint64>(baselineImage.width()) *
                                   static_cast<quint64>(baselineImage.height());
        std::fprintf(stderr,
            "[CompositionWorkerParity] backend=%s pixelsOverTolerance=%llu/%llu meanAbsoluteChannelError=%.5f maxChannelError=%d\n",
            qEnvironmentVariable("ARTIFACT_RENDER_BACKEND").toUtf8().constData(),
            static_cast<unsigned long long>(pixelsOverTolerance),
            static_cast<unsigned long long>(pixelCount),
            pixelCount == 0 ? 0.0 : static_cast<double>(absoluteChannelError) /
                static_cast<double>(pixelCount * 4),
            maximumChannelError);
        ASSERT_GT(fixtureScenePixels, 0u)
            << "The point-light fixture produced no pixels for the scene parity comparison";
        const double fixtureSceneMeanAbsoluteChannelError =
            static_cast<double>(fixtureSceneAbsoluteChannelError) /
            static_cast<double>(fixtureScenePixels * 4);
        std::fprintf(stderr,
            "[CompositionWorkerSceneParity] backend=%s pixelsOverTolerance=%llu/%llu meanAbsoluteChannelError=%.5f maxChannelError=%d\n",
            qEnvironmentVariable("ARTIFACT_RENDER_BACKEND").toUtf8().constData(),
            static_cast<unsigned long long>(fixtureScenePixelsOverTolerance),
            static_cast<unsigned long long>(fixtureScenePixels),
            fixtureSceneMeanAbsoluteChannelError,
            fixtureSceneMaximumChannelError);
        EXPECT_LE(fixtureScenePixelsOverTolerance * 100,
                  fixtureScenePixels * 8)
            << "Worker-rendered point-light scene pixels exceed the 8% parity tolerance";
        EXPECT_LE(fixtureSceneMeanAbsoluteChannelError, 1.25)
            << "Worker-rendered point-light scene differs too much from production";
        if (!workerParityCaptureRoot.isEmpty()) {
            EXPECT_TRUE(baselineImage.save(
                QDir(workerParityCaptureRoot).filePath(
                    QStringLiteral("composition_gui_controller_reference.png")),
                "PNG"));
        }
    }
    QImage deferredHiddenImage;
    QImage deferredRestoredImage;
    if (verifyAsyncWorkerRecording && !verifyDeferredShadowOverlap) {
        const quint64 workerSubmitCountBeforeVisibilityChanges =
            controller.renderer()->deferredWorkerSubmitCount();
        for (const auto& packetLayer : deferredPacketLayers) {
            packetLayer->setColor(
                ArtifactCore::FloatColor(1.0f, 0.0f, 0.0f, 1.0f));
        }
        model->setVisible(false);
        const quint64 hiddenFrame = renderDirtyFrame(baselineRenderedFrame);
        if (hiddenFrame <= baselineRenderedFrame) {
            controller.stop();
            controller.destroy();
            FAIL() << "The production renderer did not complete the hidden-model frame";
            return;
        }
        deferredHiddenImage = readPresentedFrame();

        for (const auto& packetLayer : deferredPacketLayers) {
            packetLayer->setColor(
                ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
        }
        model->setVisible(true);
        const quint64 restoredFrame = renderDirtyFrame(hiddenFrame);
        if (restoredFrame <= hiddenFrame) {
            controller.stop();
            controller.destroy();
            FAIL() << "The production renderer did not complete the restored-model frame";
            return;
        }
        deferredRestoredImage = readPresentedFrame();
        EXPECT_GT(controller.renderer()->deferredWorkerSubmitCount(),
                  workerSubmitCountBeforeVisibilityChanges)
            << "State-changing Composition frames did not return to async recording";
        EXPECT_FALSE(deferredHiddenImage.isNull())
            << "The hidden-model Composition frame produced no readback image";
        EXPECT_FALSE(deferredRestoredImage.isNull())
            << "The restored-model Composition frame produced no readback image";
        EXPECT_FALSE(baselineImage == deferredHiddenImage)
            << "Hiding the model did not change the rendered frame image";
        EXPECT_TRUE(baselineImage == deferredRestoredImage)
            << "Restoring model visibility did not reproduce the baseline frame image";
    }
    QImage unlitImage;
    QImage modelLightingDisabledImage;
    quint64 secondRenderedFrame = baselineRenderedFrame;
    if (verifyLightingDifference) {
        light->setVisible(false);
        secondRenderedFrame = renderDirtyFrame(baselineRenderedFrame);
        if (secondRenderedFrame <= baselineRenderedFrame) {
            controller.stop();
            controller.destroy();
            FAIL() << "The production render ticker did not complete the unlit FBX frame";
        }
        unlitImage = readPresentedFrame();
        light->setVisible(true);
    }
    if (verifyVisibilityAndModelOptOut) {
        light->setVisible(true);
        model->setAffectedByLights(false);
    }
    if (verifyIncludeOnlyLinking) {
        light->setLinkedLayerIdsText(model->id().toString());
    }
    if (verifyTwoPointLights) {
        auto secondLight = ArtifactCore::makeShared<Artifact::ArtifactLightLayer>();
        secondLight->setLightType(Artifact::LightType::Point);
        secondLight->setColor(
            ArtifactCore::FloatColor(1.0f, 0.04f, 0.02f, 1.0f));
        secondLight->setIntensity(ArtifactCore::Units::Percent{250.0f});
        secondLight->setRange(ArtifactCore::Units::Pixels{5000.0f});
        secondLight->transform3D().setCurrentPositionZ(350.0f);
        ASSERT_TRUE(composition->appendLayerTop(secondLight).success);
    }
    const bool sceneWasUpdated = verifyIncludeOnlyLinking || verifyTwoPointLights ||
        verifyVisibilityAndModelOptOut || verifyLightingDifference;
    if (sceneWasUpdated) {
        secondRenderedFrame = renderDirtyFrame(baselineRenderedFrame);
        if (secondRenderedFrame <= baselineRenderedFrame) {
            controller.stop();
            controller.destroy();
            FAIL() << "The production render ticker did not render the updated frame";
        }
    }
    quint64 lastRenderedFrame = secondRenderedFrame;
    if (verifyVisibilityAndModelOptOut) {
        modelLightingDisabledImage = readPresentedFrame();
        model->setAffectedByLights(true);
        lastRenderedFrame = renderDirtyFrame(secondRenderedFrame);
        if (lastRenderedFrame <= secondRenderedFrame) {
            controller.stop();
            controller.destroy();
            FAIL() << "The production render ticker did not render the model lighting change";
        }
    }
    const QImage litImage = sceneWasUpdated
        ? readPresentedFrame() : baselineImage;
    if (verifyDedicatedFrameThread) {
        const quintptr guiThreadId = reinterpret_cast<quintptr>(
            QThread::currentThreadId());
        EXPECT_NE(controller.lastFrameExecutionThreadId(), guiThreadId)
            << "The production Composition frame body still ran on the GUI thread";
    }
    QImage redChannelImage;
    if (verifyLightingDifference || verifyRenderedChannel) {
        controller.setViewportChannelDisplayMode(
            Artifact::ViewportChannelDisplayMode::Red);
        const quint64 redChannelFrame = renderDirtyFrame(lastRenderedFrame);
        if (redChannelFrame <= lastRenderedFrame) {
            controller.stop();
            controller.destroy();
            FAIL() << "The production render ticker did not render red-channel display";
            return;
        }
        redChannelImage = controller.readbackViewportChannelDisplayImage();

    }
    controller.stop();
    controller.destroy();
    if (presentedFrameCaptureFallbackCount > 0) {
        std::fprintf(stderr,
                     "[CompositionFrameCapture] presented-surface-fallbacks=%d\n",
                     presentedFrameCaptureFallbackCount);
    }
    if (verifyDedicatedFrameThread) {
        EXPECT_EQ(presentedFrameCaptureFallbackCount, 0)
            << "Dedicated-thread verification requires a presented GPU frame, "
               "not a desktop screenshot fallback";
        std::fprintf(stderr,
                     "[CompositionFrameCapture] direct-renderer-readbacks=%d\n",
                     directRendererReadbackCount);
    }

    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
        << "Diligent reported an error while rendering this light case";

    if (verifyIncludeOnlyLinking) {
        ASSERT_FALSE(baselineImage.isNull())
            << "The composition controller produced no unlinked baseline frame";
        const QVector3D unlinkedColor = averageOpaqueColor(baselineImage);
        EXPECT_LE(unlinkedColor.x(), unlinkedColor.y() * 1.2f + 8.0f)
            << "An IncludeOnly light affected a model whose ID was not linked; RGB="
            << unlinkedColor.x() << ", " << unlinkedColor.y() << ", "
            << unlinkedColor.z();
    }
    if (verifyVisibilityAndModelOptOut) {
        ASSERT_FALSE(baselineImage.isNull())
            << "The composition controller produced no hidden-light baseline frame";
        ASSERT_FALSE(modelLightingDisabledImage.isNull())
            << "The composition controller produced no affected-by-lights-off frame";
        const QVector3D hiddenLightColor = averageOpaqueColor(baselineImage);
        const QVector3D optedOutColor = averageOpaqueColor(modelLightingDisabledImage);
        EXPECT_LE(hiddenLightColor.x(), hiddenLightColor.y() * 1.2f + 8.0f)
            << "A hidden light affected the model; RGB=" << hiddenLightColor.x()
            << ", " << hiddenLightColor.y() << ", " << hiddenLightColor.z();
        EXPECT_LE(optedOutColor.x(), optedOutColor.y() * 1.2f + 8.0f)
            << "A model with affectedByLights=false received scene lighting; RGB="
            << optedOutColor.x() << ", " << optedOutColor.y() << ", "
            << optedOutColor.z();
    }
    if (verifyLightingDifference) {
        ASSERT_FALSE(unlitImage.isNull())
            << "The composition controller produced no unlit FBX reference frame";
        const QVector3D unlitColor = averageOpaqueColor(unlitImage);
        const QVector3D litColor = averageOpaqueColor(litImage);
        EXPECT_GT((litColor - unlitColor).length(), 4.0f)
            << "The point light did not measurably change the imported FBX pixels; "
            << "unlit RGB=" << unlitColor.x() << ", " << unlitColor.y() << ", "
            << unlitColor.z() << "; lit RGB=" << litColor.x() << ", "
            << litColor.y() << ", " << litColor.z();
    }
    const QString captureRoot = qEnvironmentVariable(
        "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
    if (!captureRoot.isEmpty()) {
        QDir().mkpath(captureRoot);
        litImage.save(QDir(captureRoot).filePath(
            QStringLiteral("composition_controller_%1_light.png")
                .arg(lightTypeName(type))));
        if (!deferredHiddenImage.isNull()) {
            deferredHiddenImage.save(QDir(captureRoot).filePath(
                QStringLiteral("composition_controller_deferred_hidden.png")));
        }
        if (!deferredRestoredImage.isNull()) {
            deferredRestoredImage.save(QDir(captureRoot).filePath(
                QStringLiteral("composition_controller_deferred_restored.png")));
        }
        if (!redChannelImage.isNull()) {
            redChannelImage.save(
                QDir(captureRoot).filePath(
                    QStringLiteral("composition_controller_red_channel.png")));
        }
        if (!ramPreviewFallbackImage.isNull()) {
            ramPreviewFallbackImage.save(
                QDir(captureRoot).filePath(
                    QStringLiteral("composition_controller_ram_preview_fallback.png")));
        }
    }

    ASSERT_FALSE(litImage.isNull()) << "The controller produced no frame image";
    if (verifyRamPreviewFallback) {
        ASSERT_FALSE(ramPreviewFallbackImage.isNull());
        EXPECT_GT(findLitPixels(ramPreviewFallbackImage).count,
                  ramPreviewFallbackImage.width() *
                      ramPreviewFallbackImage.height() / 2);
        return;
    }
    if (verifyLightingDifference || verifyRenderedChannel) {
        ASSERT_FALSE(redChannelImage.isNull())
            << "The controller produced no red-channel display frame";
        const QVector3D redChannelColor = averageOpaqueColor(redChannelImage);
        EXPECT_NEAR(redChannelColor.x(), redChannelColor.y(), 2.0f);
        EXPECT_NEAR(redChannelColor.y(), redChannelColor.z(), 2.0f);
        EXPECT_GT(findLitPixels(redChannelImage).count, 0)
            << "The rendered red-channel view contained no visible model pixels";
    }
    EXPECT_GT(litImage.width(), 0);
    const LitPixelBounds litPixels = findLitPixels(litImage);
    const int totalPixels = litImage.width() * litImage.height();
    EXPECT_GT(litPixels.count, totalPixels / 30)
        << "The frame contains too little non-background model coverage";
    EXPECT_LT(litPixels.count, totalPixels * 3 / 4)
        << "The light test colored nearly the entire frame; model/background separation is missing";
    EXPECT_GT(litPixels.bounds.width(), 30);
    EXPECT_GT(litPixels.bounds.height(), 30);
    const QVector3D litColor = averageOpaqueColor(litImage);
    if (verifyTwoPointLights) {
        ASSERT_FALSE(baselineImage.isNull())
            << "The composition controller produced no single-light baseline frame";
        const QVector3D baselineColor = averageOpaqueColor(baselineImage);
        EXPECT_GT(litColor.x(), baselineColor.x() * 1.08f)
            << "Adding a second point light did not increase model illumination; before RGB="
            << baselineColor.x() << ", " << baselineColor.y() << ", "
            << baselineColor.z() << "; after RGB=" << litColor.x() << ", "
            << litColor.y() << ", " << litColor.z();
    }
    if (!verifyRenderedChannel) {
        EXPECT_GT(litColor.x(), 40.0f)
            << "No lit model pixels were found in the viewport center";
        EXPECT_GT(litColor.x(), litColor.y() * 1.4f)
            << "The composition controller did not pass the red "
            << lightTypeName(type).toStdString()
            << " light layer to the model; RGB="
            << litColor.x() << ", " << litColor.y() << ", " << litColor.z();
    }
}



} // namespace

TEST(Model3DLightCompositionIntegrationTest,
     CompositionHostSnapshotCapturesWorkerCameraWithoutRendererAccess)
{
    Artifact::CompositionHostFrameSnapshot snapshot;
    EXPECT_FALSE(Artifact::captureCompositionViewportCamera(snapshot));
    EXPECT_FALSE(snapshot.viewportCameraCaptured);

    snapshot.logicalSize = QSize(640, 360);
    snapshot.devicePixelRatio = 2.0;
    snapshot.viewportZoom = 1.5f;
    snapshot.viewportPanX = 23.0f;
    snapshot.viewportPanY = -14.0f;
    snapshot.viewportTransformCaptured = true;
    ASSERT_TRUE(Artifact::captureCompositionViewportCamera(snapshot));
    ASSERT_TRUE(snapshot.viewportCameraCaptured);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            EXPECT_TRUE(std::isfinite(snapshot.viewportViewMatrix(row, column)));
            EXPECT_TRUE(std::isfinite(
                snapshot.viewportProjectionMatrix(row, column)));
        }
    }
    EXPECT_LT(snapshot.viewportProjectionMatrix(1, 1), 0.0f)
        << "The worker camera must preserve the viewport's Y-axis convention";
    const QMatrix4x4 capturedView = snapshot.viewportViewMatrix;
    snapshot.viewportZoom = 3.0f;
    EXPECT_TRUE(Artifact::captureCompositionViewportCamera(snapshot));
    EXPECT_EQ(snapshot.viewportViewMatrix, capturedView)
        << "A captured frame camera must remain immutable when later host state changes";

    snapshot.logicalSize.setWidth(0);
    EXPECT_FALSE(Artifact::captureCompositionViewportCamera(snapshot));
    EXPECT_FALSE(snapshot.viewportCameraCaptured)
        << "Invalid host dimensions must not publish a stale camera";

    Artifact::CompositionHostFrameSnapshot nonFiniteSnapshot;
    nonFiniteSnapshot.logicalSize = QSize(640, 360);
    nonFiniteSnapshot.devicePixelRatio = 1.0;
    nonFiniteSnapshot.viewportZoom =
        std::numeric_limits<float>::quiet_NaN();
    nonFiniteSnapshot.viewportTransformCaptured = true;
    EXPECT_FALSE(
        Artifact::captureCompositionViewportCamera(nonFiniteSnapshot));
    EXPECT_FALSE(nonFiniteSnapshot.viewportCameraCaptured)
        << "Non-finite viewport input must not create worker matrices";
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionFramePacketCacheReusesSceneAcrossPlaybackFrames)
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("PacketCache")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    params.setResolution(320, 240);
    params.setDurationFrames(100);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("packet-cache")), params);
    composition->goToFrame(3);

    Artifact::CompositionHostFrameSnapshot host;
    host.logicalSize = QSize(320, 240);
    host.devicePixelRatio = 1.0;
    host.viewportTransformCaptured = true;
    host.viewportInteracting = true;
    host.interactiveRenderRegionActive = true;
    host.interactiveRenderRegionRect = QRectF(20.0, 30.0, 120.0, 80.0);
    host.interactiveRenderRegionResolutionScale = 0.5f;
    host.compositionFrameCaptured = true;
    host.sceneMutationGeneration = 7;

    Artifact::CompositionRenderFramePacketCache cache;
    const auto firstPacket = cache.capture(host, composition);
    ASSERT_TRUE(firstPacket.isValid());
    EXPECT_TRUE(firstPacket.host.viewportInteracting)
        << "A worker packet must preserve the interaction state captured for its frame";
    EXPECT_TRUE(firstPacket.host.interactiveRenderRegionActive);
    EXPECT_EQ(firstPacket.host.interactiveRenderRegionRect,
              QRectF(20.0, 30.0, 120.0, 80.0));
    EXPECT_FLOAT_EQ(firstPacket.host.interactiveRenderRegionResolutionScale,
                    0.5f);
    ASSERT_EQ(cache.sceneSerializationCount(), 1u);
    EXPECT_EQ(firstPacket.host.compositionFramePosition, 3);
    const auto firstSceneJson = firstPacket.serializedComposition;

    composition->goToFrame(12);
    host.compositionFramePosition = 12;
    host.viewportInteracting = false;
    host.interactiveRenderRegionActive = false;
    host.interactiveRenderRegionRect = QRectF();
    host.interactiveRenderRegionResolutionScale = 1.0f;
    const auto nextFramePacket = cache.capture(host, composition);
    ASSERT_TRUE(nextFramePacket.isValid());
    EXPECT_EQ(cache.sceneSerializationCount(), 1u)
        << "Playback-only changes must reuse the immutable scene payload";
    EXPECT_EQ(nextFramePacket.host.compositionFramePosition, 12);
    EXPECT_FALSE(nextFramePacket.host.viewportInteracting)
        << "Latest frame metadata must replace the prior interaction state";
    EXPECT_FALSE(nextFramePacket.host.interactiveRenderRegionActive);
    EXPECT_EQ(nextFramePacket.host.interactiveRenderRegionRect, QRectF());
    EXPECT_FLOAT_EQ(nextFramePacket.host.interactiveRenderRegionResolutionScale,
                    1.0f);
    EXPECT_EQ(nextFramePacket.serializedComposition, firstSceneJson);

    auto workerComposition = Artifact::ArtifactAbstractComposition::fromJson(
        nextFramePacket.serializedComposition);
    ASSERT_TRUE(workerComposition);
    EXPECT_EQ(workerComposition->framePosition().framePosition(), 3)
        << "The cached JSON should retain its original frame until the packet applies the new frame";
    ASSERT_TRUE(nextFramePacket.applyCapturedFramePosition(
        *workerComposition));
    EXPECT_EQ(workerComposition->framePosition().framePosition(), 12);

    host.sceneMutationGeneration = 8;
    const auto changedScenePacket = cache.capture(host, composition);
    ASSERT_TRUE(changedScenePacket.isValid());
    EXPECT_EQ(cache.sceneSerializationCount(), 2u)
        << "A scene mutation identity change must rebuild the scene payload";
    EXPECT_EQ(changedScenePacket.serializedComposition.object()
                  .value(QStringLiteral("currentFrame")).toInteger(),
              12);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionFramePacketAppliesCapturedPlaybackFrame)
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("CapturedPlaybackFrame")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f));
    params.setResolution(320, 240);
    params.setDurationFrames(100);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("captured-playback-frame")),
        params);
    ASSERT_TRUE(composition);
    composition->goToFrame(3);

    Artifact::CompositionHostFrameSnapshot host;
    host.compositionFramePosition = 3;
    host.compositionFrameCaptured = true;
    host.playbackCompositionId = composition->id().toString();
    host.playbackFramePosition = 12;
    host.playbackFrameCaptured = true;

    Artifact::CompositionRenderFramePacketCache cache;
    const auto packet = cache.capture(host, composition);
    ASSERT_TRUE(packet.isValid());
    EXPECT_EQ(packet.host.compositionFramePosition, 3);
    EXPECT_EQ(packet.host.playbackFramePosition, 12);

    auto workerComposition = Artifact::ArtifactAbstractComposition::fromJson(
        packet.serializedComposition);
    ASSERT_TRUE(workerComposition);
    EXPECT_EQ(workerComposition->framePosition().framePosition(), 3);
    ASSERT_TRUE(packet.applyCapturedFramePosition(*workerComposition));
    EXPECT_EQ(workerComposition->framePosition().framePosition(), 12)
        << "The worker must render the captured playback position, not a later live service value";
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionRenderFramePacketMailboxCoalescesToLatestRequest)
{
    const auto makePacket = [](qint64 frame) {
        Artifact::CompositionRenderFramePacket packet;
        packet.compositionId = QStringLiteral("mailbox-composition");
        packet.host.compositionFramePosition = frame;
        QJsonObject scene;
        scene.insert(QStringLiteral("frame"), frame);
        packet.serializedComposition = QJsonDocument(scene);
        return packet;
    };

    Artifact::CompositionRenderFramePacketMailbox mailbox;
    ASSERT_TRUE(mailbox.publish(makePacket(1), 1));
    Artifact::CompositionRenderFramePacket inFlightPacket;
    quint64 inFlightSequence = 0;
    ASSERT_TRUE(mailbox.tryTakeLatest(inFlightPacket, inFlightSequence));
    ASSERT_EQ(inFlightSequence, 1u);
    EXPECT_TRUE(mailbox.isCurrent(inFlightSequence));
    ASSERT_TRUE(mailbox.publish(makePacket(2), 2));
    EXPECT_FALSE(mailbox.isCurrent(inFlightSequence))
        << "A newer frame request must invalidate an in-flight result";
    ASSERT_TRUE(mailbox.publish(makePacket(3), 3));
    EXPECT_EQ(mailbox.latestRequestSequence(), 3u);
    EXPECT_FALSE(mailbox.isCurrent(1));
    EXPECT_FALSE(mailbox.isCurrent(2));
    EXPECT_TRUE(mailbox.isCurrent(3));

    Artifact::CompositionRenderFramePacket latestPacket;
    quint64 latestSequence = 0;
    ASSERT_TRUE(mailbox.tryTakeLatest(latestPacket, latestSequence));
    EXPECT_EQ(latestSequence, 3u);
    EXPECT_EQ(latestPacket.host.compositionFramePosition, 3);
    EXPECT_EQ(latestPacket.compositionId,
              QStringLiteral("mailbox-composition"));
    EXPECT_FALSE(mailbox.tryTakeLatest(latestPacket, latestSequence));

    EXPECT_FALSE(mailbox.publish(makePacket(2), 2))
        << "Older render requests must not replace the current scene/frame";
    EXPECT_FALSE(mailbox.publish(makePacket(3), 3))
        << "Request sequence numbers must be strictly increasing";
    ASSERT_TRUE(mailbox.publish(makePacket(4), 4));
    mailbox.close();
    EXPECT_FALSE(mailbox.tryTakeLatest(latestPacket, latestSequence))
        << "Shutdown must discard an obsolete pending frame";
    EXPECT_FALSE(mailbox.isCurrent(4));
    EXPECT_FALSE(mailbox.publish(makePacket(5), 5));
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionRenderFramePacketMailboxWakesWorkerAndCloses)
{
    Artifact::CompositionRenderFramePacketMailbox mailbox;
    Artifact::CompositionRenderFramePacket receivedPacket;
    quint64 receivedSequence = 0;
    bool received = false;
    std::thread consumer([&] {
        received = mailbox.waitTakeLatest(receivedPacket, receivedSequence);
    });

    Artifact::CompositionRenderFramePacket packet;
    packet.compositionId = QStringLiteral("worker-mailbox-composition");
    packet.serializedComposition = QJsonDocument(QJsonObject{});
    const bool published = mailbox.publish(packet, 41);
    if (!published) {
        mailbox.close();
    }
    consumer.join();
    ASSERT_TRUE(published);
    EXPECT_TRUE(received);
    EXPECT_EQ(receivedSequence, 41u);
    EXPECT_EQ(receivedPacket.compositionId,
              QStringLiteral("worker-mailbox-composition"));
    EXPECT_TRUE(mailbox.isCurrent(41));

    std::atomic_bool closeWaitStarted{false};
    bool closeWaitTookPacket = true;
    std::thread closingConsumer([&] {
        closeWaitStarted.store(true, std::memory_order_release);
        closeWaitTookPacket = mailbox.waitTakeLatest(
            receivedPacket, receivedSequence);
    });
    while (!closeWaitStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    mailbox.close();
    closingConsumer.join();
    EXPECT_FALSE(closeWaitTookPacket)
        << "Closing an idle mailbox must wake its waiting render worker";
    EXPECT_FALSE(mailbox.isCurrent(41));
    EXPECT_FALSE(mailbox.waitTakeLatest(receivedPacket, receivedSequence));
    EXPECT_FALSE(mailbox.publish(packet, 42));
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerAppliesPointLightLayer)
{
    exerciseCompositionLight(Artifact::LightType::Point, false, false, false,
                             {}, false, false, true);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerRendersPinnedRamPreviewUpload)
{
    exerciseCompositionLight(Artifact::LightType::Point, false, false, false,
                             {}, false, false, false, false, false, false,
                             false, true);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerAppliesSpotLightLayer)
{
    exerciseCompositionLight(Artifact::LightType::Spot);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerRecordsDeferredFxaaWhileRenderingSpotShadowMap)
{
    exerciseCompositionLight(Artifact::LightType::Spot, false, false, false,
                             {}, false, false, true, false, true);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionFrameBodyRunsOutsideTheGuiThread)
{
    exerciseCompositionLight(Artifact::LightType::Point, false, false, false,
                             {}, false, false, false, false, false, true);
}

TEST(Model3DLightCompositionIntegrationTest,
     WorkerCompositionCloneRendersAndPresentsOnDedicatedThread)
{
    exerciseCompositionLight(Artifact::LightType::Point, false, false, false,
                             {}, false, false, false, false, false, false,
                             true);
}

TEST(Model3DLightCompositionIntegrationTest,
     RendererInitializesDrawsAndPresentsOnDedicatedThread)
{
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);

    struct RenderThreadResult {
        QImage image;
        DWORD threadId = 0;
        DWORD presentThreadId = 0;
        quint64 presentSuccessCount = 0;
        bool initialized = false;
        bool swapChain = false;
        Diligent::RENDER_DEVICE_TYPE deviceType = Diligent::RENDER_DEVICE_TYPE_UNDEFINED;
    } result;

    QWidget host;
    host.resize(320, 240);
    host.show();
    app.processEvents();
    const HWND parentHwnd = reinterpret_cast<HWND>(host.winId());
    const qreal dpr = host.devicePixelRatioF();
    const int width = qMax(1, qRound(host.width() * dpr));
    const int height = qMax(1, qRound(host.height() * dpr));
    const DWORD guiThreadId = GetCurrentThreadId();
    std::atomic_bool workerFinished{false};

    Artifact::ArtifactIRenderer guiResourceOwner;
    guiResourceOwner.initialize(&host);
    ASSERT_TRUE(guiResourceOwner.isInitialized());
    void* guiCreatedTexture = guiResourceOwner.createOffscreenTexture(64, 64);
    ASSERT_NE(guiCreatedTexture, nullptr);
    guiResourceOwner.clearRenderTarget(
        guiCreatedTexture, ArtifactCore::FloatColor(0.12f, 0.72f, 0.24f, 1.0f));
    guiResourceOwner.flushAndWait();
    Diligent::ITextureView* guiCreatedTextureSrv =
        guiResourceOwner.offscreenTextureShaderResourceView(guiCreatedTexture);
    ASSERT_NE(guiCreatedTextureSrv, nullptr);
    const HWND renderHwnd = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, width, height, parentHwnd, nullptr, GetModuleHandleW(nullptr),
        nullptr);
    ASSERT_NE(renderHwnd, nullptr);

    std::thread renderThread([&] {
        result.threadId = GetCurrentThreadId();
        Artifact::ArtifactIRenderer renderer;
        renderer.initializeOnRenderThread(renderHwnd, width, height);
        result.initialized = renderer.isInitialized();
        result.swapChain = renderer.hasSwapChain();
        if (result.initialized && result.swapChain) {
            const auto device = renderer.device();
            if (device) result.deviceType = device->GetDeviceInfo().Type;
            renderer.setClearColor(
                ArtifactCore::FloatColor(0.04f, 0.08f, 0.18f, 1.0f));
            renderer.clear();
            renderer.drawSprite(0.0f, 0.0f, static_cast<float>(width),
                                static_cast<float>(height),
                                guiCreatedTextureSrv, 1.0f);
            renderer.flush();
            result.image = renderer.readbackToImage();
            result.presentThreadId = GetCurrentThreadId();
            renderer.present();
            result.presentSuccessCount = renderer.presentSuccessCount();
        }
        workerFinished.store(true, std::memory_order_release);
    });
    while (!workerFinished.load(std::memory_order_acquire)) {
        app.processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    renderThread.join();
    DestroyWindow(renderHwnd);
    guiResourceOwner.destroyOffscreenTexture(guiCreatedTexture);

    EXPECT_NE(result.threadId, guiThreadId);
    EXPECT_EQ(result.threadId, result.presentThreadId);
    ASSERT_TRUE(result.initialized);
    ASSERT_TRUE(result.swapChain);
    EXPECT_NE(result.deviceType, Diligent::RENDER_DEVICE_TYPE_UNDEFINED);
    const QString requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND").trimmed().toLower();
    if (requestedBackend == QStringLiteral("d3d12")) {
        EXPECT_EQ(result.deviceType, Diligent::RENDER_DEVICE_TYPE_D3D12);
    } else if (requestedBackend == QStringLiteral("vulkan")) {
        EXPECT_EQ(result.deviceType, Diligent::RENDER_DEVICE_TYPE_VULKAN);
    }
    EXPECT_EQ(result.presentSuccessCount, 1u);
    ASSERT_FALSE(result.image.isNull());
    EXPECT_EQ(result.image.size(), QSize(width, height));
    const QColor transferredPixel = result.image.pixelColor(32, 32);
    const QColor clearPixel = result.image.pixelColor(width - 16, height - 16);
    EXPECT_GT(transferredPixel.green(), transferredPixel.red() * 1.5);
    EXPECT_GT(transferredPixel.green(), transferredPixel.blue() * 1.3);
    EXPECT_GT(clearPixel.blue(), clearPixel.green());
    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed));

    const QString captureRoot = qEnvironmentVariable(
        "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
    if (!captureRoot.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(captureRoot));
        EXPECT_TRUE(result.image.save(
            QDir(captureRoot).filePath("render_thread_owner_frame.png")));
    }
}

TEST(Model3DLightCompositionIntegrationTest,
     RendererDrawsAndPresentsOnDedicatedThreadWithBackendDeviceOwnership)
{
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);

    struct RenderThreadResult {
        QImage image;
        DWORD threadId = 0;
        DWORD presentThreadId = 0;
        quint64 presentSuccessCount = 0;
        bool initialized = false;
        bool swapChain = false;
        bool independentDevice = false;
        bool renderDeviceBindingActive = false;
        bool boundDeviceAcquired = false;
        bool boundDeviceMatchesRenderer = false;
        bool boundContextMatchesRenderer = false;
        bool sharedDeviceAcquiredAfterBinding = false;
        bool sharedDeviceRestoredAfterBinding = false;
        bool sharedContextRestoredAfterBinding = false;
        Diligent::RENDER_DEVICE_TYPE deviceType =
            Diligent::RENDER_DEVICE_TYPE_UNDEFINED;
        Diligent::IRenderDevice* device = nullptr;
        Diligent::IDeviceContext* context = nullptr;
    } result;

    constexpr int width = 320;
    constexpr int height = 240;
    QWidget host;
    host.resize(width, height);
    host.show();
    app.processEvents();
    const HWND parentHwnd = reinterpret_cast<HWND>(host.winId());
    const DWORD guiThreadId = GetCurrentThreadId();
    Artifact::ArtifactIRenderer guiRenderer;
    guiRenderer.initialize(&host);
    ASSERT_TRUE(guiRenderer.isInitialized());
    const auto guiDevice = guiRenderer.device();
    const auto guiContext = guiRenderer.immediateContext();
    ASSERT_NE(guiDevice, nullptr);
    ASSERT_NE(guiContext, nullptr);
    const QString requestedBackend = qEnvironmentVariable(
        "ARTIFACT_RENDER_BACKEND").trimmed().toLower();
    const bool useIndependentDevice =
        requestedBackend != QStringLiteral("vulkan");

    const HWND renderHwnd = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, width, height, parentHwnd, nullptr, GetModuleHandleW(nullptr),
        nullptr);
    ASSERT_NE(renderHwnd, nullptr);

    if (requestedBackend == QStringLiteral("vulkan")) {
        std::atomic_bool independentVulkanRejected{false};
        std::thread rejectionThread([&] {
            Artifact::ArtifactIRenderer rejectedRenderer;
            rejectedRenderer.initializeIndependentOnRenderThread(
                renderHwnd, width, height);
            independentVulkanRejected.store(
                !rejectedRenderer.isInitialized(), std::memory_order_release);
        });
        rejectionThread.join();
        EXPECT_TRUE(independentVulkanRejected.load(std::memory_order_acquire));
    }

    std::atomic_bool workerFinished{false};
    std::thread renderThread([&] {
        result.threadId = GetCurrentThreadId();
        Artifact::ArtifactIRenderer renderer;
        result.independentDevice = useIndependentDevice;
        if (useIndependentDevice) {
            renderer.initializeIndependentOnRenderThread(renderHwnd, width,
                                                         height);
        } else {
            renderer.initializeOnRenderThread(renderHwnd, width, height);
        }
        result.initialized = renderer.isInitialized();
        result.swapChain = renderer.hasSwapChain();
        if (result.initialized && result.swapChain) {
            const auto device = renderer.device();
            const auto context = renderer.immediateContext();
            if (device) {
                result.deviceType = device->GetDeviceInfo().Type;
                result.device = device.RawPtr();
            }
            if (context) result.context = context.RawPtr();
            {
                Artifact::ScopedRenderDeviceBinding deviceBinding(
                    device, context);
                result.renderDeviceBindingActive = deviceBinding.isActive();
                Diligent::RefCntAutoPtr<Diligent::IRenderDevice> routedDevice;
                Diligent::RefCntAutoPtr<Diligent::IDeviceContext> routedContext;
                result.boundDeviceAcquired =
                    Artifact::acquireSharedRenderDeviceForCurrentBackend(
                        routedDevice, routedContext);
                if (result.boundDeviceAcquired) {
                    result.boundDeviceMatchesRenderer =
                        routedDevice.RawPtr() == device.RawPtr();
                    result.boundContextMatchesRenderer =
                        routedContext.RawPtr() == context.RawPtr();
                    Artifact::releaseSharedRenderDevice();
                }
            }
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice> restoredDevice;
            Diligent::RefCntAutoPtr<Diligent::IDeviceContext> restoredContext;
            result.sharedDeviceAcquiredAfterBinding =
                Artifact::acquireSharedRenderDeviceForCurrentBackend(
                    restoredDevice, restoredContext);
            if (result.sharedDeviceAcquiredAfterBinding) {
                result.sharedDeviceRestoredAfterBinding =
                    restoredDevice.RawPtr() == guiDevice.RawPtr();
                result.sharedContextRestoredAfterBinding =
                    restoredContext.RawPtr() == guiContext.RawPtr();
                Artifact::releaseSharedRenderDevice();
            }
            renderer.setClearColor(
                ArtifactCore::FloatColor(0.04f, 0.08f, 0.18f, 1.0f));
            void* workerTexture = renderer.createOffscreenTexture(64, 64);
            Diligent::ITextureView* workerTextureSrv = nullptr;
            if (workerTexture) {
                renderer.clearRenderTarget(
                    workerTexture,
                    ArtifactCore::FloatColor(0.12f, 0.72f, 0.24f, 1.0f));
                workerTextureSrv =
                    renderer.offscreenTextureShaderResourceView(workerTexture);
            }
            renderer.clear();
            if (workerTextureSrv) {
                renderer.drawSprite(
                    0.0f, 0.0f, static_cast<float>(width),
                    static_cast<float>(height), workerTextureSrv, 1.0f);
            }
            renderer.flush();
            result.image = renderer.readbackToImage();
            result.presentThreadId = GetCurrentThreadId();
            renderer.present();
            result.presentSuccessCount = renderer.presentSuccessCount();
            renderer.destroyOffscreenTexture(workerTexture);
        }
        workerFinished.store(true, std::memory_order_release);
    });
    while (!workerFinished.load(std::memory_order_acquire)) {
        app.processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    renderThread.join();
    DestroyWindow(renderHwnd);

    EXPECT_NE(result.threadId, guiThreadId);
    EXPECT_EQ(result.threadId, result.presentThreadId);
    ASSERT_TRUE(result.initialized);
    ASSERT_TRUE(result.swapChain);
    EXPECT_TRUE(result.renderDeviceBindingActive);
    EXPECT_TRUE(result.boundDeviceAcquired);
    EXPECT_TRUE(result.boundDeviceMatchesRenderer);
    EXPECT_TRUE(result.boundContextMatchesRenderer);
    EXPECT_TRUE(result.sharedDeviceAcquiredAfterBinding);
    EXPECT_TRUE(result.sharedDeviceRestoredAfterBinding);
    EXPECT_TRUE(result.sharedContextRestoredAfterBinding);
    EXPECT_NE(result.deviceType, Diligent::RENDER_DEVICE_TYPE_UNDEFINED);
    if (result.independentDevice) {
        EXPECT_NE(result.device, guiDevice.RawPtr());
        EXPECT_NE(result.context, guiContext.RawPtr());
    } else {
        EXPECT_EQ(result.device, guiDevice.RawPtr());
        EXPECT_EQ(result.context, guiContext.RawPtr());
    }
    if (requestedBackend == QStringLiteral("d3d12")) {
        EXPECT_EQ(result.deviceType, Diligent::RENDER_DEVICE_TYPE_D3D12);
    } else if (requestedBackend == QStringLiteral("vulkan")) {
        EXPECT_EQ(result.deviceType, Diligent::RENDER_DEVICE_TYPE_VULKAN);
    }
    EXPECT_EQ(result.presentSuccessCount, 1u);
    ASSERT_FALSE(result.image.isNull());
    EXPECT_EQ(result.image.size(), QSize(width, height));
    const QColor renderedPixel = result.image.pixelColor(16, 16);
    const QColor backgroundPixel = result.image.pixelColor(width - 16,
                                                           height - 16);
    EXPECT_GT(renderedPixel.green(), renderedPixel.red() * 1.5);
    EXPECT_GT(renderedPixel.green(), renderedPixel.blue() * 1.3);
    EXPECT_GT(backgroundPixel.blue(), backgroundPixel.green());
    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed));

    const QString captureRoot = qEnvironmentVariable(
        "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
    if (!captureRoot.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(captureRoot));
        EXPECT_TRUE(result.image.save(
            QDir(captureRoot).filePath("render_thread_independent_frame.png")));
    }
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerAppliesParallelLightLayer)
{
    exerciseCompositionLight(Artifact::LightType::Parallel);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerAppliesAmbientLightLayer)
{
    exerciseCompositionLight(Artifact::LightType::Ambient);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerAppliesAreaLightLayer)
{
    exerciseCompositionLight(Artifact::LightType::Area);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerDisplaysLitFbxAndRegistersRayTracingGeometry)
{
#ifdef ARTIFACT_TEST_FBX_PATH
    const QString fixturePath = QString::fromUtf8(ARTIFACT_TEST_FBX_PATH);
    ASSERT_TRUE(QFileInfo::exists(fixturePath))
        << "FBX integration fixture is missing: " << fixturePath.toStdString();
    exerciseCompositionLight(Artifact::LightType::Point, false, false, false,
                             fixturePath, true, true);
#else
    GTEST_SKIP() << "The FBX integration fixture was unavailable at configure time";
#endif
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerRendersRedChannelInspectionFrame)
{
#ifdef ARTIFACT_TEST_FBX_PATH
    const QString fixturePath = QString::fromUtf8(ARTIFACT_TEST_FBX_PATH);
    ASSERT_TRUE(QFileInfo::exists(fixturePath))
        << "FBX integration fixture is missing: " << fixturePath.toStdString();
    exerciseCompositionLight(Artifact::LightType::Point, false, false, false,
                             fixturePath, false, false, false, true);
#else
    GTEST_SKIP() << "The FBX integration fixture was unavailable at configure time";
#endif
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerRespectsPointLightLinking)
{
    exerciseCompositionLight(Artifact::LightType::Point, true);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerCombinesMultiplePointLights)
{
    exerciseCompositionLight(Artifact::LightType::Point, false, true);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerRespectsLightVisibilityAndModelOptOut)
{
    exerciseCompositionLight(Artifact::LightType::Point, false, false, true);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerUsesZoomLODForTinyLayerVisibility)
{
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);

    QWidget host;
    host.resize(kViewportSize);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized());
    ASSERT_NE(controller.renderer(), nullptr);

    struct ControllerCleanup final {
        Artifact::CompositionRenderController& controller;
        ~ControllerCleanup()
        {
            controller.stop();
            controller.destroy();
        }
    } cleanup{controller};

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("ZoomLODVisibility")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kViewportSize.width(), kViewportSize.height());
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("zoom-lod-visibility")),
        params);

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("LOD threshold probe"));
    layer->setSize(12, 12);
    layer->setColor(ArtifactCore::FloatColor(1.0f, 0.1f, 0.1f, 1.0f));
    ASSERT_TRUE(composition->appendLayerTop(layer).success);

    auto subpixelLayer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(subpixelLayer);
    subpixelLayer->setLayerName(QStringLiteral("Subpixel LOD probe"));
    subpixelLayer->setSize(2, 2);
    subpixelLayer->setColor(ArtifactCore::FloatColor(0.1f, 1.0f, 0.1f, 1.0f));
    ASSERT_TRUE(composition->appendLayerTop(subpixelLayer).success);

    auto mediumBoundaryLayer =
        ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(mediumBoundaryLayer);
    mediumBoundaryLayer->setLayerName(QStringLiteral("Medium boundary LOD probe"));
    mediumBoundaryLayer->setSize(4, 4);
    mediumBoundaryLayer->setColor(
        ArtifactCore::FloatColor(0.1f, 0.1f, 1.0f, 1.0f));
    ASSERT_TRUE(composition->appendLayerTop(mediumBoundaryLayer).success);

    auto lowBoundaryLayer =
        ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(lowBoundaryLayer);
    lowBoundaryLayer->setLayerName(QStringLiteral("Low boundary LOD probe"));
    lowBoundaryLayer->setSize(32, 32);
    lowBoundaryLayer->setColor(
        ArtifactCore::FloatColor(0.9f, 0.7f, 0.1f, 1.0f));
    ASSERT_TRUE(composition->appendLayerTop(lowBoundaryLayer).success);

    auto thinLayer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(thinLayer);
    thinLayer->setLayerName(QStringLiteral("Single-axis LOD probe"));
    thinLayer->setSize(64, 1);
    thinLayer->setColor(ArtifactCore::FloatColor(0.8f, 0.5f, 0.1f, 1.0f));
    ASSERT_TRUE(composition->appendLayerTop(thinLayer).success);

    auto tallThinLayer =
        ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(tallThinLayer);
    tallThinLayer->setLayerName(QStringLiteral("Vertical single-axis LOD probe"));
    tallThinLayer->setSize(1, 64);
    tallThinLayer->setColor(ArtifactCore::FloatColor(0.2f, 0.7f, 0.9f, 1.0f));
    ASSERT_TRUE(composition->appendLayerTop(tallThinLayer).success);

    auto textLayer = ArtifactCore::makeShared<Artifact::ArtifactTextLayer>();
    ASSERT_TRUE(textLayer);
    textLayer->setLayerName(QStringLiteral("Text LOD visibility floor"));
    textLayer->setText(ArtifactCore::UniString(QStringLiteral("LOD")));
    ASSERT_TRUE(composition->appendLayerTop(textLayer).success);

    auto particleLayer = ArtifactCore::makeShared<Artifact::ArtifactParticleLayer>();
    ASSERT_TRUE(particleLayer);
    particleLayer->setLayerName(QStringLiteral("Particle LOD visibility floor"));
    ASSERT_TRUE(particleLayer->setLayerPropertyValue(
        QStringLiteral("particle.previewWidth"), QVariant(2)));
    ASSERT_TRUE(particleLayer->setLayerPropertyValue(
        QStringLiteral("particle.previewHeight"), QVariant(2)));
    ASSERT_TRUE(composition->appendLayerTop(particleLayer).success);

    auto particle3DLayer = ArtifactCore::makeShared<TestParticle3DLayer>();
    ASSERT_TRUE(particle3DLayer);
    particle3DLayer->setLayerName(QStringLiteral("3D Particle LOD visibility probe"));
    particle3DLayer->enable3DForTest();
    particle3DLayer->transform3D().setCurrentScale(0.001f, 0.001f);
    ASSERT_TRUE(particle3DLayer->setLayerPropertyValue(
        QStringLiteral("particle.previewWidth"), QVariant(2)));
    ASSERT_TRUE(particle3DLayer->setLayerPropertyValue(
        QStringLiteral("particle.previewHeight"), QVariant(2)));
    ASSERT_TRUE(particle3DLayer->is3D());
    ASSERT_TRUE(composition->appendLayerTop(particle3DLayer).success);

    auto adjustmentLayer =
        ArtifactCore::makeShared<TestAdjustableLayer>();
    ASSERT_TRUE(adjustmentLayer);
    adjustmentLayer->setLayerName(QStringLiteral("Adjustment LOD visibility floor"));
    ASSERT_TRUE(composition->appendLayerTop(adjustmentLayer).success);
    // Even if its stored source bounds are tiny, an adjustment layer affects
    // the composition below it and must not be culled by its own rectangle.
    adjustmentLayer->setSourceSizeForTest(ArtifactCore::Size_2D(1, 1));
    ASSERT_TRUE(adjustmentLayer->isAdjustmentLayer());
    ASSERT_DOUBLE_EQ(adjustmentLayer->localBounds().width(), 1.0);
    ASSERT_DOUBLE_EQ(adjustmentLayer->localBounds().height(), 1.0);

    controller.setComposition(composition);
    controller.start();

    const auto renderAtZoomAndReadVisibility = [&](const float zoom) {
        controller.renderer()->setZoom(zoom);
        const quint64 previousSerial = controller.currentFrameSerial();
        controller.markRenderDirty();
        controller.renderOneFrame();

        QElapsedTimer timeout;
        timeout.start();
        while (controller.currentFrameSerial() <= previousSerial &&
               timeout.elapsed() < 5000) {
            app.processEvents(QEventLoop::AllEvents, 10);
        }
        EXPECT_GT(controller.currentFrameSerial(), previousSerial)
            << "The composition did not render after changing viewport zoom="
            << zoom;

        const auto expectedDetail = zoom < 0.50f
            ? Artifact::LODManager::DetailLevel::Low
            : zoom < 0.90f ? Artifact::LODManager::DetailLevel::Medium
                           : Artifact::LODManager::DetailLevel::High;
        EXPECT_EQ(controller.renderer()->detailLevel(), expectedDetail)
            << "the rendered composition should pass the controller's zoom "
               "band to the renderer; zoom="
            << zoom;

        const auto snapshot = controller.frameDebugSnapshot(false);
        for (const auto& resource : snapshot.resources) {
            if (resource.label == QStringLiteral("Composition Visibility")) {
                return resource.note;
            }
        }
        ADD_FAILURE() << "Composition visibility diagnostics were missing";
        return QString();
    };

    const QString lowZoom = renderAtZoomAndReadVisibility(0.49f);
    const QString lowExactBoundary = renderAtZoomAndReadVisibility(0.25f);
    const QString mediumZoom = renderAtZoomAndReadVisibility(0.5f);
    const QString justAboveLowBoundary = renderAtZoomAndReadVisibility(
        std::nextafter(0.25f, 1.0f));
    const QString mediumNearHighBoundary =
        renderAtZoomAndReadVisibility(0.899f);
    const QString justBelowLowBoundary = renderAtZoomAndReadVisibility(
        std::nextafter(0.25f, 0.0f));
    const QString highZoom = renderAtZoomAndReadVisibility(0.9f);
    const QString highZoomAfterRoundTrip =
        renderAtZoomAndReadVisibility(0.9f);
    const QString mediumZoomAfterRoundTrip =
        renderAtZoomAndReadVisibility(0.5f);
    const QString lowZoomAfterRoundTrip =
        renderAtZoomAndReadVisibility(0.49f);

    EXPECT_TRUE(lowZoom.contains(QStringLiteral("skippedLod=6")))
        << lowZoom.toStdString();
    EXPECT_TRUE(lowZoom.contains(QStringLiteral("layersTotal=10")))
        << lowZoom.toStdString();
    EXPECT_TRUE(lowZoom.contains(QStringLiteral("drawn=4")))
        << "Text, 2D particles, and the adjustment layer should remain eligible "
           "when Low LOD skips the tiny raster layers: "
        << lowZoom.toStdString();
    EXPECT_TRUE(lowExactBoundary.contains(QStringLiteral("skippedLod=6")))
        << lowExactBoundary.toStdString();
    EXPECT_TRUE(lowExactBoundary.contains(QStringLiteral("layersTotal=10")))
        << lowExactBoundary.toStdString();
    EXPECT_TRUE(lowExactBoundary.contains(QStringLiteral("drawn=4")))
        << "A layer projected to exactly 8 pixels and the adjustment layer "
           "should remain eligible at Low LOD: "
        << lowExactBoundary.toStdString();
    EXPECT_TRUE(justAboveLowBoundary.contains(QStringLiteral("skippedLod=6")))
        << "a zoom immediately above the Low visibility boundary should still "
           "cull layers below the 8-pixel floor: "
        << justAboveLowBoundary.toStdString();
    EXPECT_TRUE(justBelowLowBoundary.contains(QStringLiteral("skippedLod=7")))
        << "a zoom immediately below the 8-pixel visibility boundary should "
           "cull the boundary-sized layer as well: "
        << justBelowLowBoundary.toStdString();
    EXPECT_TRUE(mediumZoom.contains(QStringLiteral("skippedLod=4")))
        << mediumZoom.toStdString();
    EXPECT_TRUE(mediumZoom.contains(QStringLiteral("drawn=6")))
        << "Text, 2D particles, and the adjustment layer should remain eligible "
           "at Medium LOD: "
        << mediumZoom.toStdString();
    EXPECT_TRUE(mediumNearHighBoundary.contains(QStringLiteral("skippedLod=4")))
        << mediumNearHighBoundary.toStdString();
    EXPECT_TRUE(highZoom.contains(QStringLiteral("skippedLod=0")))
        << highZoom.toStdString();
    EXPECT_TRUE(highZoom.contains(QStringLiteral("drawn=10")))
        << highZoom.toStdString();
    EXPECT_TRUE(highZoomAfterRoundTrip.contains(QStringLiteral("skippedLod=0")))
        << highZoomAfterRoundTrip.toStdString();
    EXPECT_TRUE(highZoomAfterRoundTrip.contains(QStringLiteral("drawn=10")))
        << highZoomAfterRoundTrip.toStdString();
    EXPECT_TRUE(mediumZoomAfterRoundTrip.contains(QStringLiteral("skippedLod=4")))
        << mediumZoomAfterRoundTrip.toStdString();
    EXPECT_TRUE(mediumZoomAfterRoundTrip.contains(QStringLiteral("drawn=6")))
        << mediumZoomAfterRoundTrip.toStdString();
    EXPECT_TRUE(lowZoomAfterRoundTrip.contains(QStringLiteral("skippedLod=6")))
        << lowZoomAfterRoundTrip.toStdString();
    EXPECT_TRUE(lowZoomAfterRoundTrip.contains(QStringLiteral("drawn=4")))
        << lowZoomAfterRoundTrip.toStdString();

    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
        << "Diligent reported an error while exercising viewport zoom LOD";
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerUsesRasterizerOverscanForTinyLayerLOD)
{
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);

    QWidget host;
    host.resize(kViewportSize);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized());
    ASSERT_NE(controller.renderer(), nullptr);
    struct ControllerCleanup final {
        Artifact::CompositionRenderController& controller;
        ~ControllerCleanup()
        {
            controller.stop();
            controller.destroy();
        }
    } cleanup{controller};

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("EffectExpandedLODVisibility")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kViewportSize.width(), kViewportSize.height());
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("effect-expanded-lod-visibility")),
        params);
    ASSERT_TRUE(composition);

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("Tiny Gaussian overscan LOD probe"));
    layer->setSize(1, 1);
    layer->setColor(ArtifactCore::FloatColor(1.0f, 0.8f, 0.2f, 1.0f));
    auto blur = ArtifactCore::makeShared<Artifact::GaussianBlur>();
    ASSERT_TRUE(blur);
    blur->setSigma(4.0f);
    blur->setAllowOverscan(true);
    layer->addEffect(blur);
    ASSERT_GT(layer->enabledRasterizerOverscanPixels(), 11.9f);
    ASSERT_LT(layer->localBounds().width(), 8.0);
    ASSERT_TRUE(composition->appendLayerTop(layer).success);

    auto unexpandedLayer =
        ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(unexpandedLayer);
    unexpandedLayer->setLayerName(QStringLiteral("Unexpanded tiny LOD control"));
    unexpandedLayer->setSize(1, 1);
    unexpandedLayer->setColor(
        ArtifactCore::FloatColor(0.2f, 0.9f, 1.0f, 1.0f));
    ASSERT_EQ(unexpandedLayer->enabledRasterizerOverscanPixels(), 0.0f);
    ASSERT_TRUE(composition->appendLayerTop(unexpandedLayer).success);

    controller.setComposition(composition);
    controller.start();

    const auto renderAndReadVisibility = [&](const float zoom) {
        controller.renderer()->setZoom(zoom);
        const quint64 previousSerial = controller.currentFrameSerial();
        controller.markRenderDirty();
        controller.renderOneFrame();

        QElapsedTimer timeout;
        timeout.start();
        while (controller.currentFrameSerial() <= previousSerial &&
               timeout.elapsed() < 5000) {
            app.processEvents(QEventLoop::AllEvents, 10);
        }
        EXPECT_GT(controller.currentFrameSerial(), previousSerial)
            << "The composition did not render after changing zoom=" << zoom;

        const auto snapshot = controller.frameDebugSnapshot(false);
        for (const auto& resource : snapshot.resources) {
            if (resource.label == QStringLiteral("Composition Visibility")) {
                return resource.note;
            }
        }
        ADD_FAILURE() << "Composition visibility diagnostics were missing";
        return QString();
    };

    const QString lowZoom = renderAndReadVisibility(0.49f);
    const QString lowerLowZoom = renderAndReadVisibility(0.25f);
    const QString highZoom = renderAndReadVisibility(0.9f);
    EXPECT_TRUE(lowZoom.contains(QStringLiteral("layersTotal=2")))
        << lowZoom.toStdString();
    EXPECT_TRUE(lowZoom.contains(QStringLiteral("skippedLod=1")))
        << "the unexpanded 1-pixel control should be skipped while Gaussian "
           "Blur's 3-sigma overscan keeps its sibling eligible at zoom 0.49: "
        << lowZoom.toStdString();
    EXPECT_TRUE(lowZoom.contains(QStringLiteral("drawn=1")))
        << lowZoom.toStdString();
    EXPECT_TRUE(lowerLowZoom.contains(QStringLiteral("skippedLod=2")))
        << "both 1-pixel sources should fall below the 8-pixel Low-LOD floor "
           "at zoom 0.25: "
        << lowerLowZoom.toStdString();
    EXPECT_TRUE(highZoom.contains(QStringLiteral("skippedLod=0")))
        << highZoom.toStdString();
    EXPECT_TRUE(highZoom.contains(QStringLiteral("drawn=2")))
        << "both sources should return at High LOD after the low-zoom pass: "
        << highZoom.toStdString();

    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
        << "Diligent reported an error while exercising effect-expanded LOD";
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerUsesProjectedPixelSizeFor3DLayerLOD)
{
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);

    QWidget host;
    host.resize(kViewportSize);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized());
    ASSERT_NE(controller.renderer(), nullptr);
    struct ControllerCleanup final {
        Artifact::CompositionRenderController& controller;
        ~ControllerCleanup()
        {
            controller.stop();
            controller.destroy();
        }
    } cleanup{controller};

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("Projected3DLOD")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kViewportSize.width(), kViewportSize.height());
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("projected-3d-lod")), params);

    const auto makeModel = [](const QString& name, const float scaleX,
                              const float scaleY, const float scaleZ) {
        auto model = ArtifactCore::makeShared<Artifact::Artifact3DLayer>();
        model->setLayerName(name);
        model->loadFromFile(QString::fromUtf8(ARTIFACT_TEST_MODEL_OBJ_PATH));
        model->transform3D().setCurrentPosition(
            kViewportSize.width() * 0.5f, kViewportSize.height() * 0.5f);
        model->transform3D().setScale(ArtifactCore::RationalTime(0, 24),
                                      scaleX, scaleY, scaleZ);
        return model;
    };
    auto largeProjectedModel = makeModel(
        QStringLiteral("Large projected"), 6.0f, 6.0f, 6.0f);
    auto mediumOnlyModel = makeModel(
        QStringLiteral("Medium projected"), 2.0f, 2.0f, 2.0f);
    // Keep the width above the visibility floor while making the height far
    // smaller, exercising the two-axis projected-size check.
    auto narrowProjectedModel = makeModel(
        QStringLiteral("Narrow projected"), 20.0f, 0.01f, 1.0f);
    ASSERT_GT(largeProjectedModel->mesh().vertexCount(), 0u);
    ASSERT_GT(mediumOnlyModel->mesh().vertexCount(), 0u);
    ASSERT_GT(narrowProjectedModel->mesh().vertexCount(), 0u);
    ASSERT_TRUE(composition->appendLayerTop(largeProjectedModel).success);
    ASSERT_TRUE(composition->appendLayerTop(mediumOnlyModel).success);
    ASSERT_TRUE(composition->appendLayerTop(narrowProjectedModel).success);

    controller.setComposition(composition);
    controller.start();
    const auto renderAtZoomAndReadVisibility = [&](const float zoom) {
        controller.renderer()->setZoom(zoom);
        const quint64 previousSerial = controller.currentFrameSerial();
        controller.markRenderDirty();
        controller.renderOneFrame();

        QElapsedTimer timeout;
        timeout.start();
        while (controller.currentFrameSerial() <= previousSerial &&
               timeout.elapsed() < 5000) {
            app.processEvents(QEventLoop::AllEvents, 10);
        }
        EXPECT_GT(controller.currentFrameSerial(), previousSerial)
            << "The composition did not render after changing zoom=" << zoom;

        const auto snapshot = controller.frameDebugSnapshot(false);
        for (const auto& resource : snapshot.resources) {
            if (resource.label == QStringLiteral("Composition Visibility")) {
                return resource.note;
            }
        }
        ADD_FAILURE() << "Composition visibility diagnostics were missing";
        return QString();
    };

    const QString lowZoom = renderAtZoomAndReadVisibility(0.49f);
    const QString mediumZoom = renderAtZoomAndReadVisibility(0.5f);
    const QString justBelowMediumBoundary = renderAtZoomAndReadVisibility(
        std::nextafter(0.5f, 0.0f));
    const QString justBelowHighBoundary = renderAtZoomAndReadVisibility(
        std::nextafter(0.9f, 0.0f));
    const QString highZoom = renderAtZoomAndReadVisibility(0.9f);
    const QString lowZoomAfterRoundTrip =
        renderAtZoomAndReadVisibility(0.49f);

    EXPECT_TRUE(lowZoom.contains(QStringLiteral("skippedLod=2")))
        << "the medium-size and narrow projected models should be skipped at Low: "
        << lowZoom.toStdString();
    EXPECT_TRUE(lowZoom.contains(QStringLiteral("drawn=1")))
        << "only the large projected model should remain visible at Low: "
        << lowZoom.toStdString();
    EXPECT_TRUE(mediumZoom.contains(QStringLiteral("skippedLod=1")))
        << mediumZoom.toStdString();
    EXPECT_TRUE(mediumZoom.contains(QStringLiteral("drawn=2")))
        << "the large and medium projected models should draw at Medium: "
        << mediumZoom.toStdString();
    EXPECT_TRUE(justBelowMediumBoundary.contains(QStringLiteral("skippedLod=2")))
        << "the float immediately below 0.5 should remain in Low: "
        << justBelowMediumBoundary.toStdString();
    EXPECT_TRUE(justBelowHighBoundary.contains(QStringLiteral("skippedLod=1")))
        << "the float immediately below 0.9 should remain in Medium: "
        << justBelowHighBoundary.toStdString();
    EXPECT_TRUE(highZoom.contains(QStringLiteral("skippedLod=0")))
        << highZoom.toStdString();
    EXPECT_TRUE(highZoom.contains(QStringLiteral("drawn=3")))
        << "High should restore the narrow model as well: "
        << highZoom.toStdString();
    EXPECT_TRUE(lowZoomAfterRoundTrip.contains(QStringLiteral("skippedLod=2")))
        << lowZoomAfterRoundTrip.toStdString();
    EXPECT_TRUE(lowZoomAfterRoundTrip.contains(QStringLiteral("drawn=1")))
        << "projected-size 3D visibility should return to its Low decision: "
        << lowZoomAfterRoundTrip.toStdString();
    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
        << "Diligent reported an error while exercising projected 3D LOD";
}

TEST(Model3DLightCompositionIntegrationTest,
     ActiveCameraProjectionChangesProjected3DLayerLODAndRestoresDecision)
{
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);

    QWidget host;
    host.resize(kViewportSize);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized());
    ASSERT_NE(controller.renderer(), nullptr);
    struct ControllerCleanup final {
        Artifact::CompositionRenderController& controller;
        ~ControllerCleanup()
        {
            controller.stop();
            controller.destroy();
        }
    } cleanup{controller};

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("ActiveCameraLOD")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kViewportSize.width(), kViewportSize.height());
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("active-camera-lod")),
        params);
    ASSERT_TRUE(composition);

    auto camera = ArtifactCore::makeShared<Artifact::ArtifactCameraLayer>();
    ASSERT_TRUE(camera);
    camera->transform3D().setCurrentPosition(
        kViewportSize.width() * 0.5f, kViewportSize.height() * 0.5f);
    ASSERT_TRUE(composition->appendLayerTop(camera).success);

    auto model = ArtifactCore::makeShared<Artifact::Artifact3DLayer>();
    ASSERT_TRUE(model);
    model->setLayerName(QStringLiteral("FOV-sensitive model"));
    model->loadFromFile(QString::fromUtf8(ARTIFACT_TEST_MODEL_OBJ_PATH));
    model->transform3D().setCurrentPosition(
        kViewportSize.width() * 0.5f, kViewportSize.height() * 0.5f);
    ASSERT_GT(model->mesh().vertexCount(), 0u);
    ASSERT_TRUE(composition->appendLayerTop(model).success);

    controller.setComposition(composition);
    controller.start();
    const auto renderAtCameraPose = [&](const float fovDegrees,
                                        const float cameraZ,
                                        const float modelScaleX = 1.0f,
                                        const float modelScaleY = 1.0f,
                                        const float modelRotationX = 0.0f) {
        camera->setFov(ArtifactCore::Units::Degrees{fovDegrees});
        camera->transform3D().setCurrentPositionZ(cameraZ);
        model->transform3D().setCurrentScale(modelScaleX, modelScaleY);
        model->transform3D().setCurrentRotationX(modelRotationX);
        controller.renderer()->setZoom(0.49f);
        const quint64 previousSerial = controller.currentFrameSerial();
        controller.markRenderDirty();
        controller.renderOneFrame();

        QElapsedTimer timeout;
        timeout.start();
        while (controller.currentFrameSerial() <= previousSerial &&
               timeout.elapsed() < 5000) {
            app.processEvents(QEventLoop::AllEvents, 10);
        }
        EXPECT_GT(controller.currentFrameSerial(), previousSerial)
            << "The composition did not render with camera FOV=" << fovDegrees
            << " and model scale=" << modelScaleX << "x" << modelScaleY;

        const auto snapshot = controller.frameDebugSnapshot(false);
        for (const auto& resource : snapshot.resources) {
            if (resource.label == QStringLiteral("Composition Visibility")) {
                const QString marker = QStringLiteral("skippedLod=");
                const int markerPosition = resource.note.indexOf(marker);
                if (markerPosition >= 0) {
                    const int valueStart = markerPosition + marker.size();
                    const int valueEnd = resource.note.indexOf(
                        QLatin1Char(' '), valueStart);
                    bool ok = false;
                    const int count = resource.note.mid(
                        valueStart, valueEnd - valueStart).toInt(&ok);
                    if (ok) return count;
                }
            }
        }
        ADD_FAILURE() << "Composition visibility diagnostics were missing";
        return -1;
    };

    const int wideFovSkipped = renderAtCameraPose(120.0f, 1000.0f);
    const int narrowFovSkipped = renderAtCameraPose(1.0f, 1000.0f);
    const int minimumProjectionFovSkipped = renderAtCameraPose(8.0f, 1000.0f);
    const int aboveMaximumProjectionFovSkipped =
        renderAtCameraPose(150.0f, 1000.0f);
    const int wideFovAfterRoundTripSkipped =
        renderAtCameraPose(120.0f, 1000.0f);
    const int farCameraSkipped = renderAtCameraPose(60.0f, 2000.0f);
    const int nearCameraSkipped = renderAtCameraPose(60.0f, 30.0f);
    const int farCameraAfterRoundTripSkipped =
        renderAtCameraPose(60.0f, 2000.0f);
    const int tinyModelSkipped =
        renderAtCameraPose(60.0f, 1000.0f, 0.01f, 0.01f);
    const int largeModelSkipped =
        renderAtCameraPose(60.0f, 1000.0f, 100.0f, 100.0f);
    const int wideAndThinModelSkipped =
        renderAtCameraPose(60.0f, 1000.0f, 100.0f, 0.01f);
    const int narrowAndTallModelSkipped =
        renderAtCameraPose(60.0f, 1000.0f, 0.01f, 100.0f);
    const int tinyModelAfterRoundTripSkipped =
        renderAtCameraPose(60.0f, 1000.0f, 0.01f, 0.01f);
    const int tiltedModelSkipped =
        renderAtCameraPose(60.0f, 30.0f, 1.0f, 1.0f, 75.0f);
    const int frontFacingModelAfterTiltSkipped =
        renderAtCameraPose(60.0f, 30.0f);

    ASSERT_GE(wideFovSkipped, 0);
    ASSERT_GE(narrowFovSkipped, 0);
    ASSERT_GE(minimumProjectionFovSkipped, 0);
    ASSERT_GE(aboveMaximumProjectionFovSkipped, 0);
    ASSERT_GE(wideFovAfterRoundTripSkipped, 0);
    ASSERT_GE(farCameraSkipped, 0);
    ASSERT_GE(nearCameraSkipped, 0);
    ASSERT_GE(farCameraAfterRoundTripSkipped, 0);
    ASSERT_GE(tinyModelSkipped, 0);
    ASSERT_GE(largeModelSkipped, 0);
    ASSERT_GE(wideAndThinModelSkipped, 0);
    ASSERT_GE(narrowAndTallModelSkipped, 0);
    ASSERT_GE(tinyModelAfterRoundTripSkipped, 0);
    ASSERT_GE(tiltedModelSkipped, 0);
    ASSERT_GE(frontFacingModelAfterTiltSkipped, 0);
    EXPECT_GT(wideFovSkipped, narrowFovSkipped)
        << "a wider active-camera FOV should shrink projected geometry enough "
           "to select more Low-LOD visibility skips";
    EXPECT_EQ(wideFovAfterRoundTripSkipped, wideFovSkipped)
        << "returning to the same camera FOV should restore its LOD decision";
    EXPECT_EQ(narrowFovSkipped, minimumProjectionFovSkipped)
        << "camera FOV below the projection minimum should share its clamped LOD decision";
    EXPECT_EQ(wideFovSkipped, aboveMaximumProjectionFovSkipped)
        << "camera FOV above the projection maximum should share its clamped LOD decision";
    EXPECT_GT(farCameraSkipped, nearCameraSkipped)
        << "moving the active camera farther away should shrink projected "
           "geometry enough to select more Low-LOD visibility skips";
    EXPECT_EQ(farCameraAfterRoundTripSkipped, farCameraSkipped)
        << "returning the active camera to the same distance should restore "
           "its LOD decision";
    EXPECT_GT(tinyModelSkipped, largeModelSkipped)
        << "scaling the model down should shrink its projected geometry enough "
           "to select more Low-LOD visibility skips";
    EXPECT_GT(wideAndThinModelSkipped, largeModelSkipped)
        << "a thin projected height should trigger Low-LOD visibility culling "
           "even when projected width is large";
    EXPECT_GT(narrowAndTallModelSkipped, largeModelSkipped)
        << "a thin projected width should trigger Low-LOD visibility culling "
           "even when projected height is large";
    EXPECT_EQ(tinyModelAfterRoundTripSkipped, tinyModelSkipped)
        << "restoring the same model scale should reproduce its LOD decision";
    EXPECT_GT(tiltedModelSkipped, largeModelSkipped)
        << "tilting the model should reduce its projected vertical size and "
           "increase Low-LOD visibility skips";
    EXPECT_EQ(frontFacingModelAfterTiltSkipped, largeModelSkipped)
        << "restoring the model orientation should reproduce its prior LOD decision";
    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
        << "Diligent reported an error while exercising active-camera LOD";
}

TEST(Model3DLightCompositionIntegrationTest,
     InteractiveRegionResolutionScaleStaysWithinSupportedLODRange)
{
    application();
    Artifact::CompositionRenderController controller;

    for (const auto [requestedScale, expectedScale] : {
             std::pair{0.1f, 0.25f},
             std::pair{0.375f, 0.375f},
             std::pair{2.0f, 1.0f},
             std::pair{std::numeric_limits<float>::infinity(), 1.0f},
             std::pair{-std::numeric_limits<float>::infinity(), 0.25f},
             std::pair{std::numeric_limits<float>::quiet_NaN(), 1.0f}}) {
        controller.setInteractiveRenderRegionResolutionScale(requestedScale);
        EXPECT_FLOAT_EQ(controller.interactiveRenderRegionResolutionScale(),
                        expectedScale);
    }
}

TEST(Model3DLightCompositionIntegrationTest,
     InteractiveRegionSnapshotDrivesProductionRenderContext)
{
    auto& app = application();
    QWidget host;
    host.resize(320, 240);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized());
    struct ControllerCleanup final {
        Artifact::CompositionRenderController& controller;
        ~ControllerCleanup()
        {
            controller.stop();
            controller.destroy();
        }
    } cleanup{controller};

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("InteractiveRegionSnapshot")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(320, 240);
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("interactive-region-snapshot")),
        params);
    auto layer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(layer);
    layer->setSize(80, 60);
    layer->setColor(ArtifactCore::FloatColor(0.9f, 0.2f, 0.1f, 1.0f));
    ASSERT_TRUE(composition->appendLayerTop(layer).success);

    controller.setComposition(composition);
    controller.start();
    const auto renderAndReadback = [&]() {
        const quint64 previousSerial = controller.currentFrameSerial();
        const quint64 previousPresent =
            controller.renderer()->presentSuccessCount();
        controller.markRenderDirty();
        controller.renderOneFrame();
        QElapsedTimer presentTimeout;
        presentTimeout.start();
        while (controller.renderer()->presentSuccessCount() <= previousPresent &&
               presentTimeout.elapsed() < 5000) {
            app.processEvents(QEventLoop::AllEvents, 10);
        }
        EXPECT_GT(controller.currentFrameSerial(), previousSerial);
        EXPECT_GT(controller.renderer()->presentSuccessCount(), previousPresent);
        return controller.renderer()->readbackToImage();
    };

    (void)renderAndReadback();
    const QImage baselineFrame = renderAndReadback();
    ASSERT_FALSE(baselineFrame.isNull());
    EXPECT_GT(baselineFrame.pixelColor(40, 40).red(), 50)
        << "the baseline production frame must contain the solid layer";

    controller.setInteractiveRenderRegion(Artifact::CompositionBounds2{
        {20.0, 30.0}, {120.0, 130.0}});
    controller.setInteractiveRenderRegionResolutionScale(0.5f);
    const QImage activeFrame = renderAndReadback();

    const auto& activeContext = controller.renderContext();
    EXPECT_EQ(activeContext.mode, Artifact::RenderMode::Preview);
    EXPECT_DOUBLE_EQ(activeContext.roi.rect.x(), 20.0);
    EXPECT_DOUBLE_EQ(activeContext.roi.rect.y(), 30.0);
    EXPECT_DOUBLE_EQ(activeContext.roi.rect.width(), 100.0);
    EXPECT_DOUBLE_EQ(activeContext.roi.rect.height(), 100.0);
    EXPECT_FLOAT_EQ(activeContext.resolutionScale, 0.5f);
    ASSERT_FALSE(activeFrame.isNull())
        << "the ROI-configured production frame should be read back from GPU";
    EXPECT_GT(activeFrame.width(), 0);
    EXPECT_GT(activeFrame.height(), 0);

    controller.clearInteractiveRenderRegion();
    const QImage clearedFrame = renderAndReadback();
    const auto& clearedContext = controller.renderContext();
    EXPECT_EQ(clearedContext.mode, Artifact::RenderMode::Editor);
    EXPECT_TRUE(clearedContext.roi.isEmpty());
    EXPECT_FLOAT_EQ(clearedContext.resolutionScale, 1.0f);
    ASSERT_FALSE(clearedFrame.isNull())
        << "the frame after clearing ROI should also be read back from GPU";
    EXPECT_EQ(clearedFrame.size(), activeFrame.size());
    const QString captureRoot = qEnvironmentVariable(
        "ARTIFACT_MODEL3D_CAPTURE_DIR");
    if (!captureRoot.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(captureRoot));
        EXPECT_TRUE(activeFrame.save(QDir(captureRoot).filePath(
            QStringLiteral("composition_irr_active.png"))));
        EXPECT_TRUE(clearedFrame.save(QDir(captureRoot).filePath(
            QStringLiteral("composition_irr_cleared.png"))));
    }
}

TEST(Model3DLightCompositionIntegrationTest,
     UnboundProductionControllerCapturesProjectCompositionBeforeRendering)
{
    auto& app = application();
    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(
        QStringLiteral("CompositionFrameCaptureFallbackTest"));
    QStandardPaths::setTestModeEnabled(true);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    struct ProjectCleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~ProjectCleanup() { projects.closeCurrentProject(); }
    } projectCleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("FrameCaptureFallback"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("CaptureFallback")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(320, 240);
    params.setDurationFrames(2);
    const auto created = projects.createComposition(params);
    ASSERT_TRUE(created.success);
    auto composition = projects.currentComposition();
    ASSERT_TRUE(composition);
    auto layer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(layer);
    layer->setSize(80, 60);
    layer->setColor(ArtifactCore::FloatColor(0.9f, 0.2f, 0.1f, 1.0f));
    layer->setBlendMode(ArtifactCore::LAYER_BLEND_TYPE::BLEND_SCREEN);
    ASSERT_TRUE(composition->appendLayerTop(layer).success);

    QWidget host;
    host.resize(320, 240);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized());
    struct ControllerCleanup final {
        Artifact::CompositionRenderController& controller;
        ~ControllerCleanup()
        {
            controller.stop();
            controller.destroy();
        }
    } controllerCleanup{controller};
    EXPECT_FALSE(controller.composition())
        << "The fixture must start without explicitly binding a Composition";

    controller.start();
    const quint64 previousPresent =
        controller.renderer()->presentSuccessCount();
    controller.markRenderDirty();
    controller.renderOneFrame();
    QElapsedTimer timeout;
    timeout.start();
    while (controller.renderer()->presentSuccessCount() <= previousPresent &&
           timeout.elapsed() < 5000) {
        app.processEvents(QEventLoop::AllEvents, 10);
    }

    EXPECT_EQ(controller.composition().get(), composition.get())
        << "The GUI request capture must resolve and bind the project Composition";
    EXPECT_GT(controller.renderer()->presentSuccessCount(), previousPresent);
    QImage frame;
    timeout.restart();
    while (timeout.elapsed() < 5000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        frame = controller.readbackPresentedFrameImage();
        if (!frame.isNull()) {
            break;
        }
    }
    ASSERT_FALSE(frame.isNull())
        << "The production GPU composition path must expose the presented frame";
    EXPECT_GT(frame.pixelColor(40, 40).red(),
              frame.pixelColor(40, 40).green() * 1.5)
        << "The output frame must contain the project-owned solid layer";

    const QString captureRoot = qEnvironmentVariable(
        "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
    if (!captureRoot.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(captureRoot));
        EXPECT_TRUE(frame.save(QDir(captureRoot).filePath(
            QStringLiteral("composition_capture_fallback.png"))));
    }
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerScalesGpuBlurAcrossZoomLODAndRestoresHigh)
{
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);

    QWidget host;
    host.resize(kViewportSize);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized());
    ASSERT_NE(controller.renderer(), nullptr);
    struct ControllerCleanup final {
        Artifact::CompositionRenderController& controller;
        ~ControllerCleanup()
        {
            controller.stop();
            controller.destroy();
        }
    } cleanup{controller};

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("GpuBlurLOD")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kViewportSize.width(), kViewportSize.height());
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("gpu-blur-lod")), params);

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactSolid2DLayer>();
    ASSERT_TRUE(layer);
    layer->setSize(100, 100);
    layer->setColor(ArtifactCore::FloatColor(1.0f, 0.8f, 0.2f, 1.0f));
    auto blur = ArtifactCore::makeShared<Artifact::BlurEffect>();
    ASSERT_TRUE(blur);
    blur->setRadius(24.0f);
    layer->addEffect(blur);
    ASSERT_TRUE(composition->appendLayerTop(layer).success);

    controller.setComposition(composition);
    controller.start();
    const auto renderAtZoom = [&](const float zoom) {
        controller.renderer()->setZoom(zoom);
        const quint64 previousSerial = controller.currentFrameSerial();
        controller.markRenderDirty();
        controller.renderOneFrame();

        QElapsedTimer timeout;
        timeout.start();
        while (controller.currentFrameSerial() <= previousSerial &&
               timeout.elapsed() < 5000) {
            app.processEvents(QEventLoop::AllEvents, 10);
        }
        EXPECT_GT(controller.currentFrameSerial(), previousSerial)
            << "The composition did not render after changing zoom=" << zoom;
        const auto expectedDetail = zoom < 0.50f
            ? Artifact::LODManager::DetailLevel::Low
            : zoom < 0.90f ? Artifact::LODManager::DetailLevel::Medium
                           : Artifact::LODManager::DetailLevel::High;
        EXPECT_EQ(controller.renderer()->detailLevel(), expectedDetail)
            << "the rendered frame should apply the zoom-selected detail level";
        QImage image = controller.readbackPresentedFrameImage();
        if (image.isNull()) {
            image = controller.captureCurrentFrameImage();
        }
        return image;
    };
    const auto renderPathNote = [&]() {
        const auto snapshot = controller.frameDebugSnapshot(false);
        for (const auto& resource : snapshot.resources) {
            if (resource.label == QStringLiteral("Render Path")) {
                return resource.note;
            }
        }
        ADD_FAILURE() << "Render Path diagnostics were missing";
        return QString();
    };

    const QImage immediatelyBelowLowBoundary = renderAtZoom(
        std::nextafter(0.50f, 0.0f));
    const QImage immediatelyBelowHighBoundary = renderAtZoom(
        std::nextafter(0.90f, 0.0f));
    const QImage low = renderAtZoom(0.49f);
    const QImage medium = renderAtZoom(0.7f);
    const QImage high = renderAtZoom(1.0f);
    const QImage mediumAtLowBoundary = renderAtZoom(0.5f);
    const QImage highAtMediumBoundary = renderAtZoom(0.9f);
    const QImage restoredLow = renderAtZoom(0.49f);
    const QImage restoredMedium = renderAtZoom(0.7f);
    const QImage restoredHigh = renderAtZoom(1.0f);
    ASSERT_FALSE(immediatelyBelowLowBoundary.isNull());
    ASSERT_FALSE(immediatelyBelowHighBoundary.isNull());
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(mediumAtLowBoundary.isNull());
    ASSERT_FALSE(highAtMediumBoundary.isNull());
    ASSERT_FALSE(restoredLow.isNull());
    ASSERT_FALSE(restoredMedium.isNull());
    ASSERT_FALSE(restoredHigh.isNull());

    const auto changedPixels = [](const QImage& first, const QImage& second) {
        if (first.size() != second.size()) return -1;
        int changed = 0;
        for (int y = 0; y < first.height(); ++y) {
            for (int x = 0; x < first.width(); ++x) {
                const QColor a = first.pixelColor(x, y);
                const QColor b = second.pixelColor(x, y);
                if (std::abs(a.red() - b.red()) > 3 ||
                    std::abs(a.green() - b.green()) > 3 ||
                    std::abs(a.blue() - b.blue()) > 3 ||
                    std::abs(a.alpha() - b.alpha()) > 3) {
                    ++changed;
                }
            }
        }
        return changed;
    };
    const auto changedPixelsInCentralRegion = [](const QImage& first,
                                                const QImage& second) {
        if (first.size() != second.size()) return -1;
        const QRect region = first.rect().adjusted(
            first.width() / 4, first.height() / 4,
            -first.width() / 4, -first.height() / 4);
        int changed = 0;
        for (int y = region.top(); y <= region.bottom(); ++y) {
            for (int x = region.left(); x <= region.right(); ++x) {
                const QColor a = first.pixelColor(x, y);
                const QColor b = second.pixelColor(x, y);
                if (std::abs(a.red() - b.red()) > 3 ||
                    std::abs(a.green() - b.green()) > 3 ||
                    std::abs(a.blue() - b.blue()) > 3 ||
                    std::abs(a.alpha() - b.alpha()) > 3) {
                    ++changed;
                }
            }
        }
        return changed;
    };

    EXPECT_GT(changedPixels(low, medium), 0)
        << "GPU blur parameters should use the Low and Medium resolution scales";
    EXPECT_GT(changedPixels(medium, high), 0)
        << "GPU blur parameters should use the Medium and High resolution scales";
    EXPECT_EQ(changedPixels(low, restoredLow), 0)
        << "returning to Low should reproduce the prior Low GPU blur output";
    EXPECT_EQ(changedPixels(medium, restoredMedium), 0)
        << "returning to Medium should reproduce the prior Medium GPU blur output";
    EXPECT_EQ(changedPixels(high, restoredHigh), 0)
        << "returning to High after Low should restore the original GPU blur output";

    controller.setInteractiveRenderRegion(Artifact::CompositionBounds2{
        {0.0, 0.0}, {40.0, 40.0}});
    ASSERT_TRUE(controller.isInteractiveRenderRegionActive());
    const QImage regionPreviewAtHighZoom = renderAtZoom(1.0f);
    controller.clearInteractiveRenderRegion();
    EXPECT_FALSE(controller.isInteractiveRenderRegionActive());
    const QImage afterRegionClear = renderAtZoom(1.0f);
    ASSERT_FALSE(regionPreviewAtHighZoom.isNull());
    ASSERT_FALSE(afterRegionClear.isNull());
    EXPECT_GT(changedPixelsInCentralRegion(high, regionPreviewAtHighZoom), 0)
        << "an active render region should force Preview quality at High zoom";
    EXPECT_EQ(changedPixels(high, afterRegionClear), 0)
        << "clearing the region should restore the prior Final quality";

    controller.setPreviewQualityPreset(PreviewQualityPreset::Preview);
    const QImage previewAtHighZoom = renderAtZoom(1.0f);
    const QString previewRenderPath = renderPathNote();
    ASSERT_FALSE(previewAtHighZoom.isNull());
    EXPECT_TRUE(previewRenderPath.contains(QStringLiteral("previewDownsample=2")))
        << previewRenderPath.toStdString();
    EXPECT_TRUE(previewRenderPath.contains(QStringLiteral("effectiveDownsample=1")))
        << previewRenderPath.toStdString();

    controller.setPreviewQualityPreset(PreviewQualityPreset::Draft);
    const QImage draftAtHighZoom = renderAtZoom(1.0f);
    const QString draftRenderPath = renderPathNote();
    controller.setPreviewQualityPreset(PreviewQualityPreset::Final);
    const QImage finalAfterDraft = renderAtZoom(1.0f);
    const QString finalRenderPath = renderPathNote();
    ASSERT_FALSE(draftAtHighZoom.isNull());
    ASSERT_FALSE(finalAfterDraft.isNull());
    EXPECT_TRUE(draftRenderPath.contains(QStringLiteral("previewDownsample=4")))
        << draftRenderPath.toStdString();
    EXPECT_TRUE(finalRenderPath.contains(QStringLiteral("previewDownsample=1")))
        << finalRenderPath.toStdString();
    EXPECT_GT(changedPixels(high, draftAtHighZoom), 0)
        << "Draft preview should lower spatial-effect quality even at High zoom";
    EXPECT_EQ(changedPixels(high, finalAfterDraft), 0)
        << "restoring Final preview should reproduce the High-LOD output";

    for (const auto [preset, expectedPreviewDownsample] : {
             std::pair{PreviewQualityPreset::Preview, 2},
             std::pair{PreviewQualityPreset::Draft, 4}}) {
        controller.setPreviewQualityPreset(preset);
        const QImage lowZoomAtPreset = renderAtZoom(0.49f);
        const QString lowZoomPath = renderPathNote();
        ASSERT_FALSE(lowZoomAtPreset.isNull());
        EXPECT_TRUE(lowZoomPath.contains(QStringLiteral("previewDownsample=%1")
                                             .arg(expectedPreviewDownsample)))
            << lowZoomPath.toStdString();
        EXPECT_TRUE(lowZoomPath.contains(QStringLiteral("effectiveDownsample=1")))
            << lowZoomPath.toStdString();

        const QImage mediumZoomAtPreset = renderAtZoom(0.7f);
        const QString mediumZoomPath = renderPathNote();
        ASSERT_FALSE(mediumZoomAtPreset.isNull());
        EXPECT_TRUE(mediumZoomPath.contains(QStringLiteral("previewDownsample=%1")
                                                 .arg(expectedPreviewDownsample)))
            << mediumZoomPath.toStdString();
        EXPECT_TRUE(mediumZoomPath.contains(QStringLiteral("effectiveDownsample=1")))
            << mediumZoomPath.toStdString();
    }

    layer->removeEffect(blur->effectID());
    auto shadow = ArtifactCore::makeShared<Artifact::DropShadowEffect>();
    ASSERT_TRUE(shadow);
    shadow->setDistance(28.0f);
    shadow->setAngle(0.0f);
    shadow->setSoftness(14.0f);
    shadow->setOpacity(80.0f);
    layer->addEffect(shadow);

    const QImage shadowLow = renderAtZoom(0.49f);
    const QImage shadowMedium = renderAtZoom(0.7f);
    const QImage shadowHigh = renderAtZoom(1.0f);
    const QImage shadowRestoredHigh = renderAtZoom(1.0f);
    ASSERT_FALSE(shadowLow.isNull());
    ASSERT_FALSE(shadowMedium.isNull());
    ASSERT_FALSE(shadowHigh.isNull());
    ASSERT_FALSE(shadowRestoredHigh.isNull());
    EXPECT_GT(changedPixels(shadowLow, shadowMedium), 0)
        << "Drop Shadow offsets and softness should track Low and Medium LOD";
    EXPECT_GT(changedPixels(shadowMedium, shadowHigh), 0)
        << "Drop Shadow parameters should track Medium and High LOD";
    EXPECT_EQ(changedPixels(shadowHigh, shadowRestoredHigh), 0)
        << "returning to High should restore the Drop Shadow output";

    auto chainBlur = ArtifactCore::makeShared<Artifact::BlurEffect>();
    ASSERT_TRUE(chainBlur);
    chainBlur->setRadius(12.0f);
    layer->addEffect(chainBlur);

    const QImage combinedLow = renderAtZoom(0.49f);
    const QImage combinedMedium = renderAtZoom(0.7f);
    const QImage combinedHigh = renderAtZoom(1.0f);
    const QImage combinedRestoredHigh = renderAtZoom(1.0f);
    ASSERT_FALSE(combinedLow.isNull());
    ASSERT_FALSE(combinedMedium.isNull());
    ASSERT_FALSE(combinedHigh.isNull());
    ASSERT_FALSE(combinedRestoredHigh.isNull());
    EXPECT_GT(changedPixels(combinedLow, combinedMedium), 0)
        << "every spatial pass in a multi-effect stack should use the Medium "
           "resolution scale";
    EXPECT_GT(changedPixels(combinedMedium, combinedHigh), 0)
        << "every spatial pass in a multi-effect stack should use the High "
           "resolution scale";
    EXPECT_EQ(changedPixels(combinedHigh, combinedRestoredHigh), 0)
        << "a multi-effect stack should restore its High-LOD output after "
           "quality transitions";

    layer->clearEffects();
    auto ripple = ArtifactCore::makeShared<Artifact::RippleEffect>();
    ASSERT_TRUE(ripple);
    ripple->setAmplitude(36.0f);
    ripple->setFrequency(0.08f);
    ripple->setDecay(0.0f);
    layer->addEffect(ripple);

    const QImage rippleLow = renderAtZoom(0.49f);
    const QImage rippleMedium = renderAtZoom(0.7f);
    const QImage rippleHigh = renderAtZoom(1.0f);
    const QImage rippleRestoredHigh = renderAtZoom(1.0f);
    ASSERT_FALSE(rippleLow.isNull());
    ASSERT_FALSE(rippleMedium.isNull());
    ASSERT_FALSE(rippleHigh.isNull());
    ASSERT_FALSE(rippleRestoredHigh.isNull());
    EXPECT_GT(changedPixels(rippleLow, rippleMedium), 0)
        << "Ripple amplitude should scale between Low and Medium GPU LOD";
    EXPECT_GT(changedPixels(rippleMedium, rippleHigh), 0)
        << "Ripple amplitude should scale between Medium and High GPU LOD";
    EXPECT_EQ(changedPixels(rippleHigh, rippleRestoredHigh), 0)
        << "returning to High should reproduce the Ripple output";

    layer->clearEffects();
    auto turbulent =
        ArtifactCore::makeShared<Artifact::TurbulentDisplaceEffect>();
    ASSERT_TRUE(turbulent);
    turbulent->setAmount(36.0f);
    turbulent->setSize(18.0f);
    turbulent->setOctaves(4);
    turbulent->setSeed(37);
    layer->addEffect(turbulent);

    const QImage turbulentLow = renderAtZoom(0.49f);
    const QImage turbulentMedium = renderAtZoom(0.7f);
    const QImage turbulentHigh = renderAtZoom(1.0f);
    const QImage turbulentRestoredHigh = renderAtZoom(1.0f);
    ASSERT_FALSE(turbulentLow.isNull());
    ASSERT_FALSE(turbulentMedium.isNull());
    ASSERT_FALSE(turbulentHigh.isNull());
    ASSERT_FALSE(turbulentRestoredHigh.isNull());
    EXPECT_GT(changedPixels(turbulentLow, turbulentMedium), 0)
        << "Turbulent Displace amount and size should scale from Low to Medium";
    EXPECT_GT(changedPixels(turbulentMedium, turbulentHigh), 0)
        << "Turbulent Displace amount and size should scale from Medium to High";
    EXPECT_EQ(changedPixels(turbulentHigh, turbulentRestoredHigh), 0)
        << "returning to High should reproduce the Turbulent Displace output";

    layer->clearEffects();
    auto chromaticGlow =
        ArtifactCore::makeShared<Artifact::ChromaticGlowEffect>();
    ASSERT_TRUE(chromaticGlow);
    chromaticGlow->setThreshold(0.55f);
    chromaticGlow->setRadius(24.0f);
    chromaticGlow->setIntensity(2.0f);
    chromaticGlow->setDispersion(0.8f);
    layer->addEffect(chromaticGlow);

    const QImage chromaticLow = renderAtZoom(0.49f);
    const QImage chromaticMedium = renderAtZoom(0.7f);
    const QImage chromaticHigh = renderAtZoom(1.0f);
    const QImage chromaticRestoredHigh = renderAtZoom(1.0f);
    ASSERT_FALSE(chromaticLow.isNull());
    ASSERT_FALSE(chromaticMedium.isNull());
    ASSERT_FALSE(chromaticHigh.isNull());
    ASSERT_FALSE(chromaticRestoredHigh.isNull());
    EXPECT_GT(changedPixels(chromaticLow, chromaticMedium), 0)
        << "Chromatic Glow radius should scale from Low to Medium GPU LOD";
    EXPECT_GT(changedPixels(chromaticMedium, chromaticHigh), 0)
        << "Chromatic Glow radius should scale from Medium to High GPU LOD";
    EXPECT_EQ(changedPixels(chromaticHigh, chromaticRestoredHigh), 0)
        << "returning to High should reproduce the Chromatic Glow output";

    layer->clearEffects();
    auto edgeBloom = ArtifactCore::makeShared<Artifact::EdgeBloomEffect>();
    ASSERT_TRUE(edgeBloom);
    edgeBloom->setThreshold(0.55f);
    edgeBloom->setThresholdSoftness(0.2f);
    edgeBloom->setRadius(24.0f);
    edgeBloom->setQuality(2);
    edgeBloom->setAmount(2.0f);
    edgeBloom->setEdgeBoost(2.5f);
    layer->addEffect(edgeBloom);

    const QImage edgeBloomLow = renderAtZoom(0.49f);
    const QImage edgeBloomMedium = renderAtZoom(0.7f);
    const QImage edgeBloomHigh = renderAtZoom(1.0f);
    const QImage edgeBloomRestoredHigh = renderAtZoom(1.0f);
    ASSERT_FALSE(edgeBloomLow.isNull());
    ASSERT_FALSE(edgeBloomMedium.isNull());
    ASSERT_FALSE(edgeBloomHigh.isNull());
    ASSERT_FALSE(edgeBloomRestoredHigh.isNull());
    EXPECT_GT(changedPixels(edgeBloomLow, edgeBloomMedium), 0)
        << "Edge Bloom radius should scale from Low to Medium GPU LOD";
    EXPECT_GT(changedPixels(edgeBloomMedium, edgeBloomHigh), 0)
        << "Edge Bloom radius should scale from Medium to High GPU LOD";
    EXPECT_EQ(changedPixels(edgeBloomHigh, edgeBloomRestoredHigh), 0)
        << "High LOD should preserve Edge Bloom's selected internal quality";

    edgeBloom->setRadius(5.0f);
    edgeBloom->setQuality(0);
    const QImage edgeBloomDraftQuality = renderAtZoom(1.0f);
    edgeBloom->setQuality(1);
    const QImage edgeBloomStandardQuality = renderAtZoom(1.0f);
    edgeBloom->setQuality(2);
    const QImage edgeBloomHighQuality = renderAtZoom(1.0f);
    edgeBloom->setQuality(0);
    const QImage edgeBloomRestoredDraftQuality = renderAtZoom(1.0f);
    ASSERT_FALSE(edgeBloomDraftQuality.isNull());
    ASSERT_FALSE(edgeBloomStandardQuality.isNull());
    ASSERT_FALSE(edgeBloomHighQuality.isNull());
    ASSERT_FALSE(edgeBloomRestoredDraftQuality.isNull());
    EXPECT_GT(changedPixels(edgeBloomDraftQuality, edgeBloomStandardQuality), 0)
        << "Edge Bloom's Draft and Standard sampling radii should differ";
    EXPECT_GT(changedPixels(edgeBloomStandardQuality, edgeBloomHighQuality), 0)
        << "Edge Bloom's Standard and High sampling radii should differ";
    EXPECT_EQ(changedPixels(edgeBloomDraftQuality,
                            edgeBloomRestoredDraftQuality), 0)
        << "returning to Draft should reproduce Edge Bloom's original output";

    controller.setPreviewQualityPreset(PreviewQualityPreset::Final);
    controller.notifyViewportInteractionActivity();
    const QImage interactingAtHighZoom = renderAtZoom(1.0f);
    controller.finishViewportInteraction();
    const QImage finalAfterInteraction = renderAtZoom(1.0f);
    ASSERT_FALSE(interactingAtHighZoom.isNull());
    ASSERT_FALSE(finalAfterInteraction.isNull());
    EXPECT_GT(changedPixels(edgeBloomHigh, interactingAtHighZoom), 0)
        << "viewport interaction should temporarily lower effect resolution "
           "even at High zoom";
    EXPECT_EQ(changedPixels(edgeBloomHigh, finalAfterInteraction), 0)
        << "ending viewport interaction should restore the Final High-LOD output";

    const auto expectInteractionLodTransition = [&](float zoom,
                                                     const QString& detail,
                                                     const QString& settledPreviewDownsample) {
        controller.notifyViewportInteractionActivity();
        const QImage interacting = renderAtZoom(zoom);
        const QString interactingPath = renderPathNote();
        ASSERT_FALSE(interacting.isNull());
        EXPECT_TRUE(interactingPath.contains(QStringLiteral("effectiveDownsample=4")))
            << interactingPath.toStdString();
        controller.finishViewportInteraction();
        const QImage settled = renderAtZoom(zoom);
        const QString settledPath = renderPathNote();
        ASSERT_FALSE(settled.isNull());
        EXPECT_TRUE(settledPath.contains(QStringLiteral("effectiveDownsample=1")))
            << settledPath.toStdString();
        EXPECT_TRUE(settledPath.contains(settledPreviewDownsample))
            << settledPath.toStdString();
        const auto expectedDetail = detail == QStringLiteral("Low")
            ? Artifact::LODManager::DetailLevel::Low
            : detail == QStringLiteral("Medium")
                ? Artifact::LODManager::DetailLevel::Medium
                : Artifact::LODManager::DetailLevel::High;
        EXPECT_EQ(controller.renderer()->detailLevel(), expectedDetail);
    };
    expectInteractionLodTransition(0.49f, QStringLiteral("Low"),
                                   QStringLiteral("previewDownsample=1"));
    expectInteractionLodTransition(0.7f, QStringLiteral("Medium"),
                                   QStringLiteral("previewDownsample=1"));
    expectInteractionLodTransition(1.0f, QStringLiteral("High"),
                                   QStringLiteral("previewDownsample=1"));

    controller.setPreviewQualityPreset(PreviewQualityPreset::Draft);
    controller.notifyViewportInteractionActivity();
    const QImage interactingDraft = renderAtZoom(0.7f);
    const QString interactingDraftPath = renderPathNote();
    ASSERT_FALSE(interactingDraft.isNull());
    EXPECT_TRUE(interactingDraftPath.contains(QStringLiteral("effectiveDownsample=4")))
        << interactingDraftPath.toStdString();
    controller.setPreviewQualityPreset(PreviewQualityPreset::Preview);
    const QImage interactingPreview = renderAtZoom(0.7f);
    const QString interactingPreviewPath = renderPathNote();
    ASSERT_FALSE(interactingPreview.isNull());
    EXPECT_TRUE(interactingPreviewPath.contains(QStringLiteral("effectiveDownsample=4")))
        << interactingPreviewPath.toStdString();
    controller.finishViewportInteraction();
    const QImage settledPreview = renderAtZoom(0.7f);
    const QString settledPreviewPath = renderPathNote();
    ASSERT_FALSE(settledPreview.isNull());
    EXPECT_TRUE(settledPreviewPath.contains(QStringLiteral("previewDownsample=2")))
        << settledPreviewPath.toStdString();
    EXPECT_TRUE(settledPreviewPath.contains(QStringLiteral("effectiveDownsample=1")))
        << settledPreviewPath.toStdString();
    EXPECT_EQ(controller.renderer()->detailLevel(),
              Artifact::LODManager::DetailLevel::Medium);
    controller.setPreviewQualityPreset(PreviewQualityPreset::Final);
    const QImage restoredFinalAfterPresetTransition = renderAtZoom(0.7f);
    ASSERT_FALSE(restoredFinalAfterPresetTransition.isNull());
    const QString restoredFinalPath = renderPathNote();
    EXPECT_TRUE(restoredFinalPath.contains(QStringLiteral("previewDownsample=1")))
        << restoredFinalPath.toStdString();
    EXPECT_TRUE(restoredFinalPath.contains(QStringLiteral("effectiveDownsample=1")))
        << restoredFinalPath.toStdString();

    controller.setPreviewQualityPreset(
        static_cast<PreviewQualityPreset>(99));
    const QImage unknownPreviewQuality = renderAtZoom(1.0f);
    const QString unknownPreviewQualityPath = renderPathNote();
    ASSERT_FALSE(unknownPreviewQuality.isNull());
    EXPECT_TRUE(unknownPreviewQualityPath.contains(
        QStringLiteral("previewDownsample=1")))
        << unknownPreviewQualityPath.toStdString();
    EXPECT_EQ(changedPixels(finalAfterInteraction, unknownPreviewQuality), 0)
        << "an unknown preview quality should use the Final-resolution output";

    controller.setPreviewQualityPreset(PreviewQualityPreset::Final);
    const QImage finalAfterUnknownQuality = renderAtZoom(1.0f);
    ASSERT_FALSE(finalAfterUnknownQuality.isNull());
    EXPECT_EQ(changedPixels(finalAfterInteraction, finalAfterUnknownQuality), 0)
        << "returning to Final after an unknown quality should restore its output";
    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
        << "Diligent reported an error while exercising GPU effect LOD";
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerDepthOccludesLaterFarModel)
{
    testPhase.store("occlusion-setup");
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);

    QWidget host;
    host.resize(kViewportSize);
    host.show();
    app.processEvents();

    Artifact::CompositionRenderController controller;
    controller.initialize(&host);
    ASSERT_TRUE(controller.isInitialized());
    controller.setShowOriginOverlay(false);
    controller.setShowAnchorCenterOverlay(false);
    controller.setShowGizmoOverlay(false);

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("ModelOcclusion")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kViewportSize.width(), kViewportSize.height());
    params.setDurationFrames(100);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("model-occlusion")), params);

    const auto makeModel = [](const QString& name, const QColor& color,
                              float z) {
        auto model = ArtifactCore::makeShared<Artifact::Artifact3DLayer>();
        model->setLayerName(name);
        model->loadFromFile(QString::fromUtf8(ARTIFACT_TEST_MODEL_OBJ_PATH));
        model->transform3D().setScale(ArtifactCore::RationalTime(0, 24), 24, 24, 24);
        model->transform3D().setCurrentPosition(kViewportSize.width() * 0.5f,
                                                kViewportSize.height() * 0.5f);
        model->transform3D().setCurrentPositionZ(z);
        model->setLayerPropertyValue(QStringLiteral("material.base.color"), color);
        return model;
    };
    auto nearRedModel = makeModel(QStringLiteral("Near Red"),
                                  QColor(255, 20, 10), 60.0f);
    auto farGreenModel = makeModel(QStringLiteral("Far Green"),
                                   QColor(10, 255, 20), 0.0f);
    ASSERT_GT(nearRedModel->mesh().vertexCount(), 0u);
    ASSERT_GT(farGreenModel->mesh().vertexCount(), 0u);

    auto ambient = ArtifactCore::makeShared<Artifact::ArtifactLightLayer>();
    ambient->setLightType(Artifact::LightType::Ambient);
    ambient->setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    ambient->setIntensity(ArtifactCore::Units::Percent{100.0f});
    ASSERT_TRUE(composition->appendLayerBottom(nearRedModel).success);
    ASSERT_TRUE(composition->appendLayerTop(farGreenModel).success);
    ASSERT_TRUE(composition->appendLayerTop(ambient).success);

    testPhase.store("occlusion-first-frame");
    controller.setComposition(composition);
    controller.start();
    QEventLoop frameLoop;
    QTimer pollTimer;
    QTimer timeoutTimer;
    pollTimer.setInterval(5);
    timeoutTimer.setSingleShot(true);
    QObject::connect(&pollTimer, &QTimer::timeout, &frameLoop, [&] {
        if (controller.currentFrameSerial() > 0) frameLoop.quit();
    });
    QObject::connect(&timeoutTimer, &QTimer::timeout,
                     &frameLoop, &QEventLoop::quit);
    pollTimer.start();
    timeoutTimer.start(5000);
    if (controller.currentFrameSerial() == 0) frameLoop.exec();
    pollTimer.stop();
    ASSERT_GT(controller.currentFrameSerial(), 0u)
        << "The production render ticker did not render the occlusion scene";

    const quint64 firstFrame = controller.currentFrameSerial();
    testPhase.store("occlusion-second-frame");
    QEventLoop secondFrameLoop;
    QTimer secondPollTimer;
    QTimer secondTimeoutTimer;
    secondPollTimer.setInterval(5);
    secondTimeoutTimer.setSingleShot(true);
    QObject::connect(&secondPollTimer, &QTimer::timeout,
                     &secondFrameLoop, [&] {
        if (controller.currentFrameSerial() > firstFrame)
            secondFrameLoop.quit();
    });
    QObject::connect(&secondTimeoutTimer, &QTimer::timeout,
                     &secondFrameLoop, &QEventLoop::quit);
    secondPollTimer.start();
    secondTimeoutTimer.start(5000);
    QTimer::singleShot(25, &secondFrameLoop, [&] {
        controller.markRenderDirty();
        controller.renderOneFrame();
    });
    if (controller.currentFrameSerial() <= firstFrame)
        secondFrameLoop.exec();
    secondPollTimer.stop();
    ASSERT_GT(controller.currentFrameSerial(), firstFrame)
        << "The production controller did not complete the final occlusion frame";
    testPhase.store("occlusion-readback");
    const QImage image = controller.readbackPresentedFrameImage();
    const auto snapshot = controller.frameDebugSnapshot(false);
    testPhase.store("occlusion-stop");
    controller.stop();
    const QByteArray validation = controller.renderer()->gpuValidationDebugState().toUtf8();
    std::fprintf(stderr, "[OcclusionGpuValidation] %s\n", validation.constData());
    testPhase.store("occlusion-destroy");
    controller.destroy();
    testPhase.store("occlusion-assertions");

    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
        << "Diligent reported an error while rendering two overlapping models";
    ASSERT_FALSE(image.isNull())
        << "The completed two-model composition did not expose its final GPU surface; "
        << "snapshot failed=" << snapshot.failed
        << ", reason=" << snapshot.failureReason.toStdString()
        << ", total layers=" << snapshot.totalLayerCount
        << ", visible layers=" << snapshot.visibleLayerCount;

    const LitPixelBounds litPixels = findLitPixels(image);
    const int totalPixels = image.width() * image.height();
    EXPECT_GT(litPixels.count, totalPixels / 100)
        << "The two-model composition contains too few lit pixels";
    EXPECT_LT(litPixels.count, totalPixels * 3 / 4)
        << "The two-model composition colored nearly the entire frame";
    EXPECT_GT(litPixels.bounds.width(), 30);
    EXPECT_GT(litPixels.bounds.height(), 30);
    EXPECT_GT(litPixels.bounds.left(), 0);
    EXPECT_GT(litPixels.bounds.top(), 0);
    EXPECT_LT(litPixels.bounds.right(), image.width() - 1);
    EXPECT_LT(litPixels.bounds.bottom(), image.height() - 1);

    const QVector3D overlapColor = averageOpaqueColor(image, litPixels.bounds);
    EXPECT_GT(overlapColor.x(), overlapColor.y() * 1.35f)
        << "The farther green model, drawn later, was not occluded by the nearer red model; "
        << "RGB=" << overlapColor.x() << ", " << overlapColor.y() << ", "
        << overlapColor.z();
    const QString captureRoot = qEnvironmentVariable(
        "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
    if (!captureRoot.isEmpty()) {
        QDir().mkpath(captureRoot);
        EXPECT_TRUE(image.save(QDir(captureRoot).filePath(
            QStringLiteral("composition_controller_two_model_occlusion.png"))));
    }
}

namespace {

int runLightMcpHost()
{
    auto& app = application();
    QStandardPaths::setTestModeEnabled(true);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);
    QWidget host;
    host.setAttribute(Qt::WA_ShowWithoutActivating);
    host.setWindowTitle(QStringLiteral("3D Light Composition Test MCP"));
    host.resize(kViewportSize);
    Artifact::CompositionRenderController controller;
    ArtifactTestMcpServer server(QStringLiteral("artifact-light-composition-test"),
                                 QStringLiteral("1.0.0"));
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("LightMcp")),
        ArtifactCore::FloatColor(0, 0, 0, 0));
    params.setResolution(kViewportSize.width(), kViewportSize.height());
    params.setDurationFrames(100);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("light-mcp")), params);
    auto model = ArtifactCore::makeShared<Artifact::Artifact3DLayer>();
    model->loadFromFile(QString::fromUtf8(ARTIFACT_TEST_MODEL_OBJ_PATH));
    model->transform3D().setCurrentPosition(160, 120);
    model->transform3D().setScale(ArtifactCore::RationalTime(0, 24), 24, 24, 24);
    model->setLayerPropertyValue(QStringLiteral("material.base.color"),
                                 QColor(255, 255, 255));
    auto light = ArtifactCore::makeShared<Artifact::ArtifactLightLayer>();
    light->setLightType(Artifact::LightType::Point);
    light->setIntensity(ArtifactCore::Units::Percent{1000});
    light->setRange(ArtifactCore::Units::Pixels{5000});
    light->setConeLength(ArtifactCore::Units::Pixels{5000});
    light->setConeAngle(ArtifactCore::Units::Degrees{60});
    light->setConeFeather(ArtifactCore::Units::Degrees{10});
    light->setAreaSize(ArtifactCore::Units::Pixels{1000},
                       ArtifactCore::Units::Pixels{1000});
    light->transform3D().setCurrentPosition(160, 120);
    light->transform3D().setCurrentPositionZ(350);
    light->transform3D().setCurrentRotationY(180);
    std::array<float, 3> rgb{1, 0.04f, 0.02f};
    light->setColor(ArtifactCore::FloatColor(rgb[0], rgb[1], rgb[2], 1));
    if (!composition->appendLayerBottom(model).success ||
        !composition->appendLayerTop(light).success ||
        model->mesh().vertexCount() == 0) return 2;

    const auto state = [&]() {
        const auto snapshot = controller.frameDebugSnapshot(false);
        bool layerTargetReady = false;
        bool accumTargetReady = false;
        for (const auto& resource : snapshot.resources) {
            if (resource.label == QStringLiteral("Layer RT"))
                layerTargetReady = resource.texture.valid;
            else if (resource.label == QStringLiteral("Accum RT"))
                accumTargetReady = resource.texture.valid;
        }
        const auto color = light->color();
        QString actualBackend = QStringLiteral("unavailable");
        QString gpuValidation = QStringLiteral("unavailable");
        if (auto* renderer = controller.renderer()) {
            gpuValidation = renderer->gpuValidationDebugState();
            if (const auto device = renderer->device()) {
                const auto type = device->GetDeviceInfo().Type;
                if (type == Diligent::RENDER_DEVICE_TYPE_D3D12)
                    actualBackend = QStringLiteral("d3d12");
                else if (type == Diligent::RENDER_DEVICE_TYPE_VULKAN)
                    actualBackend = QStringLiteral("vulkan");
                else actualBackend = QStringLiteral("other");
            }
        }
        return QJsonObject{
            {QStringLiteral("initialized"), controller.isInitialized()},
            {QStringLiteral("backendRequested"), qEnvironmentVariable("ARTIFACT_RENDER_BACKEND")},
            {QStringLiteral("backendActual"), actualBackend},
            {QStringLiteral("frameSerial"), QString::number(controller.currentFrameSerial())},
            {QStringLiteral("layerTargetReady"), layerTargetReady},
            {QStringLiteral("accumTargetReady"), accumTargetReady},
            {QStringLiteral("failed"), snapshot.failed},
            {QStringLiteral("failureReason"), snapshot.failureReason},
            {QStringLiteral("diligentError"), diligentErrorDetected.load()},
            {QStringLiteral("gpuValidation"), gpuValidation},
            {QStringLiteral("lightType"), lightTypeName(light->lightType())},
            {QStringLiteral("intensityPercent"), light->intensity().value},
            {QStringLiteral("lightVisible"), light->isVisible()},
            {QStringLiteral("modelAffectedByLights"), model->affectedByLights()},
            {QStringLiteral("showGizmos"), controller.isShowGizmoOverlay()},
            {QStringLiteral("colorRGB"), QJsonArray{color.red(), color.green(), color.blue()}},
            {QStringLiteral("viewport"), QJsonArray{host.width(), host.height()}},
            {QStringLiteral("devicePixelRatio"), host.devicePixelRatioF()},
            {QStringLiteral("cameraMode"), QStringLiteral("productionCompositionFallback")},
            {QStringLiteral("modelPosition"), QJsonArray{
                model->transform3D().positionX(), model->transform3D().positionY(),
                model->transform3D().positionZ()}},
            {QStringLiteral("modelScaleXY"), QJsonArray{
                model->transform3D().scaleX(), model->transform3D().scaleY()}},
            {QStringLiteral("vertexCount"), static_cast<double>(model->mesh().vertexCount())}};
    };
    // Defer submission until outside the MCP handler's current event delivery.
    // No new signal/slot connection: bounded event pumping is a test boundary.
    const auto render = [&]() {
        const auto previous = controller.currentFrameSerial();
        bool submitted = false;
        QObject requestContext;
        controller.start();
        QTimer::singleShot(0, &requestContext, [&]() {
            controller.markRenderDirty();
            controller.renderOneFrame();
            submitted = true;
        });
        QElapsedTimer deadline;
        deadline.start();
        while ((!submitted || controller.currentFrameSerial() <= previous) &&
               deadline.elapsed() < 5000) {
            app.processEvents(QEventLoop::ExcludeUserInputEvents, 10);
        }
        controller.stop();
        return submitted && controller.currentFrameSerial() > previous;
    };
    const QJsonObject emptySchema{{QStringLiteral("type"), QStringLiteral("object")}};
    bool registered = server.registerTool(QStringLiteral("light.state"),
        QStringLiteral("Read production composition, light, frame, and diagnostic state"),
        emptySchema, [&](const QJsonObject&) { return state(); });
    registered &= server.registerTool(QStringLiteral("light.set"),
        QStringLiteral("Set light type, RGB linear color [0,1], and intensity percent [0,1000]"),
        QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("type"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("enum"), QJsonArray{"point", "spot", "parallel", "ambient", "area"}}}},
                {QStringLiteral("rgb"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                    {QStringLiteral("minItems"), 3}, {QStringLiteral("maxItems"), 3},
                    {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")},
                        {QStringLiteral("minimum"), 0}, {QStringLiteral("maximum"), 1}}}}},
                {QStringLiteral("intensityPercent"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")},
                    {QStringLiteral("minimum"), 0}, {QStringLiteral("maximum"), 1000}}}}},
            {QStringLiteral("required"), QJsonArray{"type", "rgb", "intensityPercent"}}},
        [&](const QJsonObject& args) {
            const auto typeName = args.value(QStringLiteral("type")).toString();
            const auto colors = args.value(QStringLiteral("rgb")).toArray();
            const auto intensity = args.value(QStringLiteral("intensityPercent"));
            Artifact::LightType type;
            if (typeName == "point") type = Artifact::LightType::Point;
            else if (typeName == "spot") type = Artifact::LightType::Spot;
            else if (typeName == "parallel") type = Artifact::LightType::Parallel;
            else if (typeName == "ambient") type = Artifact::LightType::Ambient;
            else if (typeName == "area") type = Artifact::LightType::Area;
            else return QJsonObject{{QStringLiteral("error"), QStringLiteral("Unknown light type")}};
            if (colors.size() != 3 || !intensity.isDouble() ||
                !std::isfinite(intensity.toDouble()) || intensity.toDouble() < 0 ||
                intensity.toDouble() > 1000)
                return QJsonObject{{QStringLiteral("error"), QStringLiteral("Invalid RGB or intensity")}};
            for (const auto& channel : colors) {
                if (!channel.isDouble() || !std::isfinite(channel.toDouble()) ||
                    channel.toDouble() < 0 || channel.toDouble() > 1)
                    return QJsonObject{{QStringLiteral("error"), QStringLiteral("RGB must be three numbers in [0,1]")}};
            }
            light->setLightType(type);
            for (int i = 0; i < 3; ++i) rgb[i] = static_cast<float>(colors[i].toDouble());
            light->setColor(ArtifactCore::FloatColor(rgb[0], rgb[1], rgb[2], 1));
            light->setIntensity(ArtifactCore::Units::Percent{static_cast<float>(intensity.toDouble())});
            auto result = state();
            result[QStringLiteral("renderPending")] = true;
            return result;
        });
    registered &= server.registerTool(QStringLiteral("light.controls"),
        QStringLiteral("Set light visibility and whether the model receives scene lights"),
        QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("visible"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                {QStringLiteral("showGizmos"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                {QStringLiteral("affectedByLights"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}}}},
            {QStringLiteral("required"), QJsonArray{"visible", "affectedByLights"}}},
        [&](const QJsonObject& args) {
            const auto visible = args.value(QStringLiteral("visible"));
            const auto affected = args.value(QStringLiteral("affectedByLights"));
            const auto gizmos = args.value(QStringLiteral("showGizmos"));
            if (!visible.isBool() || !affected.isBool() ||
                (!gizmos.isUndefined() && !gizmos.isBool()))
                return QJsonObject{{QStringLiteral("error"), QStringLiteral("Both controls must be boolean")}};
            light->setVisible(visible.toBool());
            model->setAffectedByLights(affected.toBool());
            if (!gizmos.isUndefined()) controller.setShowGizmoOverlay(gizmos.toBool());
            auto result = state();
            result[QStringLiteral("renderPending")] = true;
            return result;
        });
    registered &= server.registerTool(QStringLiteral("light.capture"),
        QStringLiteral("Render a fresh frame and return the final GPU composition PNG and geometry bounds"),
        QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("includeWindow"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}}}}},
        [&](const QJsonObject& args) {
            const auto includeWindow = args.value(QStringLiteral("includeWindow"));
            if (!includeWindow.isUndefined() && !includeWindow.isBool())
                return QJsonObject{{QStringLiteral("error"), QStringLiteral("includeWindow must be boolean")}};
            if (!render()) {
                auto result = state();
                result[QStringLiteral("error")] = QStringLiteral("Frame render timed out");
                return result;
            }
            QString windowError;
            const QImage windowImage = includeWindow.toBool()
                ? artifactCaptureWindow(reinterpret_cast<HWND>(host.winId()), &windowError)
                : QImage{};
            const auto image = controller.readbackPresentedFrameImage();
            if (image.isNull()) {
                auto result = state();
                result[QStringLiteral("error")] =
                    QStringLiteral("Final GPU frame unavailable");
                result[QStringLiteral("frameDebugSnapshot")] =
                    controller.frameDebugSnapshot(false).toJson();
                return result;
            }
            QByteArray png;
            QBuffer buffer(&png);
            if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
                return QJsonObject{{QStringLiteral("error"), QStringLiteral("PNG encoding failed")}};
            const auto pixels = findLitPixels(image);
            auto result = state();
            result[QStringLiteral("captureSize")] = QJsonArray{image.width(), image.height()};
            result[QStringLiteral("litPixelCount")] = pixels.count;
            result[QStringLiteral("boundsExcludeThinGuides")] = false;
            result[QStringLiteral("litBounds")] = QJsonArray{pixels.bounds.x(), pixels.bounds.y(),
                pixels.bounds.width(), pixels.bounds.height()};
            result[QStringLiteral("edgeContact")] = pixels.count > 0 &&
                (pixels.bounds.left() <= 0 || pixels.bounds.top() <= 0 ||
                 pixels.bounds.right() >= image.width() - 1 || pixels.bounds.bottom() >= image.height() - 1);
            const auto average = averageOpaqueColor(image, pixels.bounds);
            result[QStringLiteral("averageRGB")] = QJsonArray{average.x(), average.y(), average.z()};
            result[QStringLiteral("mcpContent")] = QJsonArray{QJsonObject{
                {QStringLiteral("type"), QStringLiteral("image")},
                {QStringLiteral("mimeType"), QStringLiteral("image/png")},
                {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}}};
            if (includeWindow.toBool()) {
                result[QStringLiteral("windowCaptureError")] = windowError;
                result[QStringLiteral("windowCaptureSize")] = QJsonArray{windowImage.width(), windowImage.height()};
                if (!windowImage.isNull()) {
                    auto content = result[QStringLiteral("mcpContent")].toArray();
                    content.append(QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("image")},
                        {QStringLiteral("mimeType"), QStringLiteral("image/png")},
                        {QStringLiteral("data"), QString::fromLatin1(artifactEncodePng(windowImage).toBase64())}});
                    result[QStringLiteral("mcpContent")] = content;
                }
            }
            return result;
        });
    if (!registered) return 3;
    host.show();
    app.processEvents();
    controller.initialize(&host);
    if (!controller.isInitialized()) return 4;
    controller.setShowOriginOverlay(false);
    controller.setShowAnchorCenterOverlay(false);
    controller.setShowGizmoOverlay(false);
    controller.setComposition(composition);
    controller.start();
    QElapsedTimer startupDeadline;
    startupDeadline.start();
    bool startupReady = false;
    QJsonObject startupSnapshot;
    while (!startupReady && startupDeadline.elapsed() < 15000) {
        app.processEvents(QEventLoop::ExcludeUserInputEvents, 10);
        if (controller.currentFrameSerial() < 2) continue;
        const auto snapshot = controller.frameDebugSnapshot(false);
        startupSnapshot = snapshot.toJson();
        bool layerTargetReady = false;
        bool accumTargetReady = false;
        for (const auto& resource : snapshot.resources) {
            if (resource.label == QStringLiteral("Layer RT"))
                layerTargetReady = resource.texture.valid;
            else if (resource.label == QStringLiteral("Accum RT"))
                accumTargetReady = resource.texture.valid;
        }
        startupReady = layerTargetReady && accumTargetReady &&
            !controller.readbackPresentedFrameImage().isNull();
    }
    controller.stop();
    if (!startupReady || !server.start()) {
        if (!startupReady) {
            std::fprintf(stderr,
                "[LightMcpStartup] Timed out waiting for production GPU targets; frameSerial=%llu snapshot=%s\n",
                static_cast<unsigned long long>(controller.currentFrameSerial()),
                qPrintable(QJsonDocument(startupSnapshot).toJson(QJsonDocument::Compact)));
        }
        controller.stop();
        controller.destroy();
        return 3;
    }
    const int result = app.exec();
    controller.stop();
    controller.destroy();
    return diligentErrorDetected.load() ? 5 : result;
}

} // namespace

int main(int argc, char** argv)
{
    wchar_t tempPath[MAX_PATH]{};
    const DWORD tempLength = GetTempPathW(MAX_PATH, tempPath);
    if (tempLength > 0 && tempLength < MAX_PATH)
        std::swprintf(crashDumpPath.data(), crashDumpPath.size(),
            L"%lsArtifactLightTest-%lu.dmp", tempPath, GetCurrentProcessId());
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument.startsWith(QStringLiteral("--crash-dump="))) {
            const QString path = argument.mid(13);
            if (!path.isEmpty() && path.size() < crashDumpPath.size()) {
                crashDumpPath.fill(0);
                path.toWCharArray(crashDumpPath.data());
            } else return 6;
        }
    }
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--mcp-stdio"))
            return runLightMcpHost();
    }
    ::testing::InitGoogleTest(&argc, argv);
    // A real native fault must exit at its first failing context. The explicit
    // synthetic SEH case retains GoogleTest's catch to verify its registered gate.
    if (!qEnvironmentVariableIsSet("ARTIFACT_TEST_TRIGGER_UNHANDLED_EXCEPTION"))
        GTEST_FLAG_SET(catch_exceptions, false);
    return RUN_ALL_TESTS();
}
