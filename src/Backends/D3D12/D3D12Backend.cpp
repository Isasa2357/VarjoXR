#include <VarjoXR/Backends/D3D12/D3D12Backend.hpp>

#include <VarjoXR/Core/XRPlane.hpp>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>

#include <stdexcept>
#include <utility>

#include <windows.h>

namespace VarjoXR::Backends::D3D12 {

D3D12Texture::D3D12Texture(
    D3D12CoreLib::D3D12Resource resource,
    D3D12CoreLib::D3D12DescriptorHandle srv,
    uint32_t width,
    uint32_t height,
    TextureOwnership ownership)
    : XRTexture(BackendType::D3D12, width, height, ownership)
    , resource_(std::move(resource))
    , srv_(srv) {}

struct D3D12Backend::Impl {
    std::shared_ptr<D3D12CoreLib::D3D12Core> core;
    D3D12BackendDesc desc{};
    D3D12CoreLib::D3D12DescriptorAllocator cbvSrvUavAllocator;
    std::shared_ptr<::VarjoSession> session;
    std::unique_ptr<::VarjoFrameInfo> frameInfo;
    std::unique_ptr<::VarjoLayerFrame> layerFrame;
    bool frameBegun = false;

    explicit Impl(std::shared_ptr<D3D12CoreLib::D3D12Core> inCore, D3D12BackendDesc inDesc)
        : core(std::move(inCore)), desc(inDesc) {
        if (!core) {
            throw std::runtime_error("D3D12Backend requires an externally-created D3D12Core shared_ptr.");
        }
        cbvSrvUavAllocator.Initialize(
            core->GetDevice(),
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            desc.cbvSrvUavDescriptorCount,
            true);
    }

    void initialize(std::shared_ptr<::VarjoSession> inSession) {
        if (!inSession || !inSession->valid()) {
            throw std::runtime_error("D3D12Backend::initialize requires a valid external VarjoSession.");
        }
        session = std::move(inSession);
        frameInfo = std::make_unique<::VarjoFrameInfo>(*session);
        layerFrame = std::make_unique<::VarjoLayerFrame>(*session);
        if (!frameInfo || !frameInfo->valid()) {
            throw std::runtime_error("D3D12Backend failed to create VarjoFrameInfo.");
        }
        if (!layerFrame || !layerFrame->valid()) {
            throw std::runtime_error("D3D12Backend failed to create VarjoLayerFrame.");
        }
        OutputDebugStringA("[VarjoXR][D3D12] Backend initialized with external D3D12Core and external VarjoSession.\n");
    }

    void beginFrame() {
        if (frameBegun) {
            throw std::runtime_error("D3D12Backend::beginFrame called while a frame is already active.");
        }
        if (!frameInfo->waitSync()) {
            throw std::runtime_error("D3D12Backend::beginFrame: VarjoFrameInfo::waitSync failed: " + session->lastError());
        }
        if (!layerFrame->begin()) {
            throw std::runtime_error("D3D12Backend::beginFrame: VarjoLayerFrame::begin failed: " + layerFrame->lastError());
        }
        frameBegun = true;
    }

    void render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) {
        (void)planes;
        (void)frameContext;
        // TODO(rewrite/v0.1): create VarjoSwapChain, build D3D12 root signature/PSO,
        // run optional D3D12Processing, draw planes with per-eye Plane HLSL, and submit layer views.
        // This first rewrite commit intentionally fixes ownership/API boundaries before the draw path.
    }

    void endFrame() {
        if (!frameBegun) {
            throw std::runtime_error("D3D12Backend::endFrame requires beginFrame first.");
        }
        if (!layerFrame->endEmpty(frameInfo->frameNumber())) {
            frameBegun = false;
            throw std::runtime_error("D3D12Backend::endFrame: VarjoLayerFrame::endEmpty failed: " + layerFrame->lastError());
        }
        frameBegun = false;
    }
};

D3D12Backend::D3D12Backend(std::shared_ptr<D3D12CoreLib::D3D12Core> core, D3D12BackendDesc desc)
    : impl_(std::make_unique<Impl>(std::move(core), desc)) {}

D3D12Backend::~D3D12Backend() = default;

void D3D12Backend::initialize(std::shared_ptr<::VarjoSession> session) { impl_->initialize(std::move(session)); }
void D3D12Backend::beginFrame() { impl_->beginFrame(); }
void D3D12Backend::render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) { impl_->render(planes, frameContext); }
void D3D12Backend::endFrame() { impl_->endFrame(); }

std::shared_ptr<D3D12Texture> D3D12Backend::createTextureFromRGBA(
    const uint8_t* rgba,
    uint32_t width,
    uint32_t height,
    uint32_t rowPitchBytes) {
    if (!rgba || width == 0 || height == 0) {
        throw std::runtime_error("D3D12Backend::createTextureFromRGBA received invalid input.");
    }
    auto resource = D3D12CoreLib::CreateTexture2DFromMemory(
        *impl_->core,
        rgba,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        rowPitchBytes,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    auto srv = impl_->cbvSrvUavAllocator.Allocate();
    D3D12CoreLib::CreateTexture2DSrv(*impl_->core, resource, srv.cpu);
    return std::make_shared<D3D12Texture>(std::move(resource), srv, width, height, TextureOwnership::Owned);
}

std::shared_ptr<D3D12Texture> D3D12Backend::wrapResource(ID3D12Resource* resource, DXGI_FORMAT srvFormat) {
    if (!resource) {
        throw std::runtime_error("D3D12Backend::wrapResource received null resource.");
    }
    D3D12CoreLib::ComPtr<ID3D12Resource> resourcePtr = resource;
    D3D12CoreLib::D3D12Resource wrapped(resourcePtr, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const auto desc = wrapped.GetDesc();
    auto srv = impl_->cbvSrvUavAllocator.Allocate();
    D3D12CoreLib::CreateTexture2DSrv(*impl_->core, wrapped, srv.cpu, srvFormat);
    return std::make_shared<D3D12Texture>(
        std::move(wrapped),
        srv,
        static_cast<uint32_t>(desc.Width),
        static_cast<uint32_t>(desc.Height),
        TextureOwnership::External);
}

std::unique_ptr<IRenderBackend> CreateBackend(std::shared_ptr<D3D12CoreLib::D3D12Core> core, D3D12BackendDesc desc) {
    return std::make_unique<D3D12Backend>(std::move(core), desc);
}

} // namespace VarjoXR::Backends::D3D12
