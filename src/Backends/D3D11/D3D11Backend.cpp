#include <VarjoXR/Backends/D3D11/D3D11Backend.hpp>

#include <VarjoXR/Detail/VarjoLayerSubmit.hpp>
#include <VarjoXR/VarjoSession.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace VarjoXR::Backends::D3D11 {

struct D3D11Backend::Impl {
    std::shared_ptr<D3D11CoreLib::D3D11Core> core;
    Detail::VarjoLayerSubmit layer;

    void initialize(VarjoSession& session, const XRSpaceConfig& config) {
        D3D11CoreLib::D3D11CoreConfig cfg{};
        cfg.enableDebugLayer = config.enableDebug;
        cfg.enableInfoQueue = config.enableDebug;
        cfg.enableMultithreadProtection = true;
        core = D3D11CoreLib::D3D11Core::CreateShared(cfg);
        layer.initialize(session.get(), config);

        // TODO: Create Varjo D3D11 swapchain from core->GetDevice().
        // This file intentionally uses D3D11Helper for device/resource/view creation
        // and does not depend on Varjo SDK examples/Common.
    }

    void renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) {
        (void)objects;
        layer.waitSync(session.get());
        // TODO: acquire Varjo swapchain image, draw planes, submit layer, and release image.
        // This placeholder is the replacement entry point; concrete drawing code will be
        // filled using D3D11Helper GraphicsPipeline / texture helpers.
        if (core) core->Flush();
    }

    std::shared_ptr<XRTexture> createTextureFromRGBA(const uint8_t* rgba, uint32_t width, uint32_t height, uint32_t rowPitchBytes) {
        if (!core) throw std::runtime_error("D3D11Backend is not initialized.");
        if (!rgba || width == 0 || height == 0) throw std::runtime_error("Invalid RGBA texture input.");
        auto res = D3D11CoreLib::CreateTexture2DFromMemory(*core, rgba, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, rowPitchBytes);
        auto srv = D3D11CoreLib::CreateTexture2DSrv(*core, res);
        return std::make_shared<D3D11Texture>(std::move(res), std::move(srv), width, height);
    }
};

D3D11Backend::D3D11Backend() : m_impl(std::make_unique<Impl>()) {}
D3D11Backend::~D3D11Backend() = default;

void D3D11Backend::initialize(VarjoSession& session, const XRSpaceConfig& config) { m_impl->initialize(session, config); }
void D3D11Backend::renderFrame(VarjoSession& session, const std::vector<std::unique_ptr<XRObject>>& objects) { m_impl->renderFrame(session, objects); }
std::shared_ptr<XRTexture> D3D11Backend::createTextureFromRGBA(const uint8_t* rgba, uint32_t width, uint32_t height, uint32_t rowPitchBytes) {
    return m_impl->createTextureFromRGBA(rgba, width, height, rowPitchBytes);
}

} // namespace VarjoXR::Backends::D3D11
