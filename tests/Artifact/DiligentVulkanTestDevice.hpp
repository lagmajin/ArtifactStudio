#pragma once

#include <DeviceContext.h>
#include <EngineFactoryVk.h>
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>

namespace ArtifactTest {

class VulkanGpuDevice final {
public:
    bool initialize()
    {
        destroy();
        auto* factory = Diligent::LoadAndGetEngineFactoryVk();
        if (!factory) {
            return false;
        }

        Diligent::EngineVkCreateInfo createInfo{};
        createInfo.Features.MultithreadedResourceCreation =
            Diligent::DEVICE_FEATURE_STATE_DISABLED;
        createInfo.Features.RayTracing = Diligent::DEVICE_FEATURE_STATE_DISABLED;
        factory->CreateDeviceAndContextsVk(createInfo, &device_, &context_);
        return device_ != nullptr && context_ != nullptr;
    }

    Diligent::IRenderDevice* device() const noexcept { return device_.RawPtr(); }
    Diligent::IDeviceContext* context() const noexcept { return context_.RawPtr(); }

    void flushAndWait()
    {
        if (context_) {
            context_->Flush();
            context_->WaitForIdle();
        }
    }

    void destroy() noexcept
    {
        context_.Release();
        device_.Release();
    }

private:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
};

} // namespace ArtifactTest
