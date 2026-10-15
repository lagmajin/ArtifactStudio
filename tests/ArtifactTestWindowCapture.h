#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QIODevice>
#include <QString>

#include <chrono>
#include <cstring>
#include <exception>
#include <future>
#include <iterator>
#include <limits>
#include <memory>
#include <utility>

#ifdef _WIN32
#include <atomic>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/base.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowsapp.lib")
using ArtifactNativeWindowHandle = HWND;
#else
using ArtifactNativeWindowHandle = void*;
#endif

// Captures a native window without activating it. This is a cold-path test
// artifact boundary; it does not participate in frame rendering.
#ifdef _WIN32
inline QImage artifactCaptureWindowOnWorker(HWND window, QString* error,
                                            int timeoutMs)
{
    using namespace winrt::Windows::Graphics::Capture;
    using namespace winrt::Windows::Graphics::DirectX;
    using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

    if (!window || !IsWindow(window)) {
        if (error) *error = QStringLiteral("Invalid target window");
        return {};
    }
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        struct ApartmentGuard {
            ~ApartmentGuard() { winrt::uninit_apartment(); }
        };
        [[maybe_unused]] const ApartmentGuard apartmentGuard;

        winrt::com_ptr<ID3D11Device> d3dDevice;
        winrt::com_ptr<ID3D11DeviceContext> d3dContext;
        D3D_FEATURE_LEVEL featureLevel{};
        constexpr D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        const HRESULT deviceResult = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
            static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            d3dDevice.put(), &featureLevel, d3dContext.put());
        if (FAILED(deviceResult)) {
            if (error) *error = QStringLiteral("Could not create D3D11 capture device");
            return {};
        }

        winrt::com_ptr<IDXGIDevice> dxgiDevice = d3dDevice.as<IDXGIDevice>();
        winrt::com_ptr<::IInspectable> inspectableDevice;
        const HRESULT wrappedDeviceResult = ::CreateDirect3D11DeviceFromDXGIDevice(
            dxgiDevice.get(), inspectableDevice.put());
        if (FAILED(wrappedDeviceResult)) {
            if (error) *error = QStringLiteral("Could not wrap D3D11 capture device");
            return {};
        }
        const auto captureDevice = inspectableDevice.as<IDirect3DDevice>();

        GraphicsCaptureItem item{nullptr};
        const auto interop = winrt::get_activation_factory<
            GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        winrt::check_hresult(interop->CreateForWindow(
            window, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            reinterpret_cast<void**>(winrt::put_abi(item))));
        const auto size = item.Size();
        if (size.Width <= 0 || size.Height <= 0) {
            if (error) *error = QStringLiteral("Target window has no drawable area");
            return {};
        }

        auto framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
            captureDevice, DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, size);
        auto session = framePool.CreateCaptureSession(item);
        auto frameSignal = std::make_shared<std::promise<bool>>();
        auto signaled = std::make_shared<std::atomic_bool>(false);
        auto frameReady = frameSignal->get_future();
        const auto token = framePool.FrameArrived(
            [frameSignal, signaled](const auto&, const auto&) {
                if (!signaled->exchange(true)) frameSignal->set_value(true);
            });
        const auto closedToken = item.Closed(
            [frameSignal, signaled](const auto&, const auto&) {
                if (!signaled->exchange(true)) frameSignal->set_value(false);
            });
        session.StartCapture();
        const bool completed = frameReady.wait_for(
            std::chrono::milliseconds(timeoutMs)) == std::future_status::ready;
        const bool arrived = completed && frameReady.get();
        framePool.FrameArrived(token);
        item.Closed(closedToken);
        if (!completed) {
            if (error) *error = QStringLiteral("Timed out waiting for a window capture frame");
            return {};
        }
        if (!arrived) {
            if (error) *error = QStringLiteral("Target window closed during capture");
            return {};
        }

        auto frame = framePool.TryGetNextFrame();
        if (!frame) {
            if (error) *error = QStringLiteral("Capture frame was not available");
            return {};
        }
        const auto frameSize = frame.ContentSize();
        if (frameSize.Width <= 0 || frameSize.Height <= 0 ||
            frameSize.Width != size.Width ||
            frameSize.Height != size.Height) {
            if (error) *error = QStringLiteral(
                "Target window resized during capture; retry with a fresh frame");
            return {};
        }
        auto surface = frame.Surface().as<
            ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<ID3D11Texture2D> sourceTexture;
        winrt::check_hresult(surface->GetInterface(
            __uuidof(ID3D11Texture2D), sourceTexture.put_void()));
        D3D11_TEXTURE2D_DESC sourceDesc{};
        sourceTexture->GetDesc(&sourceDesc);
        if (sourceDesc.Width == 0 || sourceDesc.Height == 0 ||
            sourceDesc.Width > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            sourceDesc.Height > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            sourceDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM ||
            sourceDesc.ArraySize != 1 || sourceDesc.MipLevels != 1 ||
            sourceDesc.SampleDesc.Count != 1) {
            if (error) *error = QStringLiteral("Unsupported window capture texture layout");
            return {};
        }

        D3D11_TEXTURE2D_DESC stagingDesc = sourceDesc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.MiscFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.SampleDesc.Quality = 0;
        winrt::com_ptr<ID3D11Texture2D> stagingTexture;
        winrt::check_hresult(d3dDevice->CreateTexture2D(
            &stagingDesc, nullptr, stagingTexture.put()));
        d3dContext->CopyResource(stagingTexture.get(), sourceTexture.get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        winrt::check_hresult(d3dContext->Map(
            stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped));
        const size_t rowBytes = static_cast<size_t>(sourceDesc.Width) * 4;
        if (!mapped.pData || mapped.RowPitch < rowBytes) {
            d3dContext->Unmap(stagingTexture.get(), 0);
            if (error) *error = QStringLiteral("Mapped capture texture has an invalid row pitch");
            return {};
        }
        QImage image(static_cast<int>(sourceDesc.Width),
                     static_cast<int>(sourceDesc.Height), QImage::Format_ARGB32);
        if (image.isNull()) {
            d3dContext->Unmap(stagingTexture.get(), 0);
            if (error) *error = QStringLiteral("Could not allocate the capture image");
            return {};
        }
        const auto* sourceBytes = static_cast<const uchar*>(mapped.pData);
        for (UINT row = 0; row < sourceDesc.Height; ++row) {
            std::memcpy(image.scanLine(static_cast<int>(row)),
                        sourceBytes + static_cast<size_t>(row) * mapped.RowPitch,
                        rowBytes);
        }
        d3dContext->Unmap(stagingTexture.get(), 0);
        session.Close();
        framePool.Close();
        return image;
    } catch (const winrt::hresult_error& exception) {
        if (error) *error = QString::fromWCharArray(exception.message().c_str());
        return {};
    } catch (...) {
        if (error) *error = QStringLiteral("Unexpected window capture failure");
        return {};
    }
}
#endif

inline QImage artifactCaptureWindow(ArtifactNativeWindowHandle window,
                                    QString* error = nullptr,
                                    int timeoutMs = 3000)
{
#ifdef _WIN32
    if (timeoutMs <= 0) {
        if (error) *error = QStringLiteral("Capture timeout must be positive");
        return {};
    }
    if (timeoutMs > 60000) timeoutMs = 60000;
    try {
        struct Result { QImage image; QString error; };
        const QString resizeError = QStringLiteral(
            "Target window resized during capture; retry with a fresh frame");
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeoutMs);
        auto future = std::async(std::launch::async,
            [window, timeoutMs, resizeError, deadline] {
            Result result;
            for (int attempt = 0; attempt < 2; ++attempt) {
                const auto remaining = std::chrono::duration_cast<
                    std::chrono::milliseconds>(deadline -
                        std::chrono::steady_clock::now()).count();
                if (remaining <= 0) {
                    result.error = QStringLiteral(
                        "Timed out capturing target window");
                    break;
                }
                result = {};
                result.image = artifactCaptureWindowOnWorker(
                    window, &result.error, static_cast<int>(remaining));
                if (!result.image.isNull() || result.error != resizeError)
                    break;
            }
            return result;
        });
        Result result = future.get();
        if (error) *error = result.error;
        return result.image;
    } catch (const std::exception& exception) {
        if (error) *error = QString::fromUtf8(exception.what());
        return {};
    } catch (...) {
        if (error) *error = QStringLiteral("Unexpected capture dispatch failure");
        return {};
    }
#else
    Q_UNUSED(window)
    Q_UNUSED(timeoutMs)
    if (error) *error = QStringLiteral("Direct native window capture is only implemented on Windows");
    return {};
#endif
}

inline QByteArray artifactEncodePng(const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) return {};
    return bytes;
}
