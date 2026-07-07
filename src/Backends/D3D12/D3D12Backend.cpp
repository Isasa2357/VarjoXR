#include <VarjoXR/Backends/D3D12/D3D12Backend.hpp>

#include <VarjoXR/Detail/VarjoLayerSubmit.hpp>
#include <VarjoXR/VarjoSession.hpp>

#include <stdexcept>

namespace VarjoXR::Backends::D3D12 {

struct D3D12Backend::Impl {
    std::shared_ptr<D3D12CoreLib::D3D12Core> core;
    D3D12CoreLib::D3D12DescriptorAllocator cbvSrvUav;
    Detail::VarjoLayerSubmit layer;

    void initialize(VarjoSession& session, const XRSpaceConfig& config) {
        D3D12CoreLib::D3D12CoreConfig cfg{};
        cfg.enableDebugLayer = config.enableDebug;
        cfg.enableInfoQueue = config.enableDebug;
        cfg.createDirectQueue = true;
        cfg.createCopyQueue = true;
        cfg.createComputeQueue = false;
        core = D3D12CoreLib::D3D12Core::CreateShared(cfg);
        cbvSrvUav.Initialize(core->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true);
        layer.initialize(session.get(), config);

        // TODO: Create Varjo D3D12 swapchain from core->GetDirectCommandQueue().
        // This file intentionally uses D3D12Helper for device/resource/view/descriptor creation
        // and does not depend on Varjo SDK examples/Common.
    }

    void renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) {
        (void)objects;
        layer.waitSync(session.get());
        // TODO: acquire Varjo swapchain image, record plane draw commands, submit layer, release image.
        if (core) core->WaitIdle();
    }

    std::shared_ptr<XRTexture> createTextureFromRGBA(const uint8_t* rgba, uint32_t width, uint32_t height, uint32_t rowPitchBytes) {
        if (!core) throw std::runtime_error("D3D12Backend is not initialized.");
        if (!rgba || width == 0 || height == 0) throw std::runtime_error("Invalid RGBA texture input.");
        auto res = D3D12CoreLib::CreateTexture2DFromMemory(*core, rgba, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, rowPitchBytes);
        auto handle = cbvSrvUav.Allocate();
        D3D12CoreLib::CreateTexture2DSrv(*core, res, handle.cpu);
        return std::make_shared<D3D12Texture>(std::move(res), handle, width, height);
    }
};

D3D12Backend::D3D12Backend() : m_impl(std::make_unique<Impl>()) {}
D3D12Backend::~D3D12Backend() = default;

void D3D12Backend::initialize(VarjoSession& session, const XRSpaceConfig& config) { m_impl->initialize(session, config); }
void D3D12Backend::renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) { m_impl->renderFrame(session, objects); }
std::shared_ptr<XRTexture> D3D12Backend::createTextureFromRGBA(const uint8_t* rgba, uint32_t width, uint32_t height, uint32_t rowPitchBytes) {
    return m_impl->createTextureFromRGBA(rgba, width, height, rowPitchBytes);
}

} // namespace VarjoXR::Backends::D3D12
