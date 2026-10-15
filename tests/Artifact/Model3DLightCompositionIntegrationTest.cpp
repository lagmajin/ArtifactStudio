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
#include <QRect>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>
#include <QVector3D>
#include <QWidget>

#include <atomic>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <cmath>

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

import Artifact.Composition.InitParams;
import Composition3D;
import Artifact.Layer.Light;
import Artifact.Layers.Model3D;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Render.IRenderer;
import Color.Float;
import Core.Light;
import Memory.SharedPtr;
import Utils.Id;
import Utils.String.UniString;
import Time.Rational;

namespace {

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

void exerciseCompositionLight(Artifact::LightType type,
                              bool verifyIncludeOnlyLinking = false,
                              bool verifyTwoPointLights = false,
                              bool verifyVisibilityAndModelOptOut = false,
                              const QString& modelPath = {},
                              bool verifyRayTracingRegistration = false,
                              bool verifyLightingDifference = false)
{
    auto& app = application();
    diligentErrorDetected.store(false, std::memory_order_relaxed);
    Diligent::SetDebugMessageCallback(integrationDiligentMessageCallback);
    Diligent::WindowsDebug::SetBreakOnError(false);
    QCoreApplication::setApplicationName(
        QStringLiteral("Model3DLightCompositionIntegrationTest"));
    QStandardPaths::setTestModeEnabled(true);

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
    ASSERT_TRUE(composition->appendLayerBottom(model).success);
    ASSERT_TRUE(composition->appendLayerTop(light).success);

    controller.setComposition(composition);
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
    const auto renderDirtyFrame = [&](quint64 afterSerial) {
        controller.markRenderDirty();
        controller.renderOneFrame();
        return controller.currentFrameSerial() > afterSerial
            ? controller.currentFrameSerial() : quint64{0};
    };
    const quint64 firstRenderedFrame = waitForFrame(0);
    if (firstRenderedFrame == 0) {
        controller.stop();
        controller.destroy();
        FAIL() << "The production render ticker did not render its dirty composition";
    }
    const quint64 baselineRenderedFrame = renderDirtyFrame(firstRenderedFrame);
    if (baselineRenderedFrame <= firstRenderedFrame) {
        controller.stop();
        controller.destroy();
        FAIL() << "The production render ticker did not complete the baseline frame";
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
    const QImage baselineImage = controller.readbackPresentedFrameImage();
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
        unlitImage = controller.readbackPresentedFrameImage();
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
        modelLightingDisabledImage = controller.readbackPresentedFrameImage();
        model->setAffectedByLights(true);
        lastRenderedFrame = renderDirtyFrame(secondRenderedFrame);
        if (lastRenderedFrame <= secondRenderedFrame) {
            controller.stop();
            controller.destroy();
            FAIL() << "The production render ticker did not render the model lighting change";
        }
    }
    const QImage litImage = sceneWasUpdated
        ? controller.readbackPresentedFrameImage() : baselineImage;
    controller.stop();
    controller.destroy();

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
    }

    ASSERT_FALSE(litImage.isNull()) << "The controller produced no frame image";
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
    EXPECT_GT(litColor.x(), 40.0f)
        << "No lit model pixels were found in the viewport center";
    EXPECT_GT(litColor.x(), litColor.y() * 1.4f)
        << "The composition controller did not pass the red "
        << lightTypeName(type).toStdString()
        << " light layer to the model; RGB="
        << litColor.x() << ", " << litColor.y() << ", " << litColor.z();
}



} // namespace

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerAppliesPointLightLayer)
{
    exerciseCompositionLight(Artifact::LightType::Point);
}

TEST(Model3DLightCompositionIntegrationTest,
     CompositionControllerAppliesSpotLightLayer)
{
    exerciseCompositionLight(Artifact::LightType::Spot);
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
