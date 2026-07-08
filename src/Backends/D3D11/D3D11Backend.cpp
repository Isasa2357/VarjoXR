#include <VarjoXR/Backends/D3D11/D3D11Backend.hpp>

#include <VarjoXR/Core/XRPlane.hpp>

#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Rendering/VarjoLayerFrame.hpp>

#include <stdexcept>
#include <utility>

#include <windows.h>

namespace VarjoXR::Backends::D3D11 {

D3D11Texture::D3D11Texture(
    D3D11CoreLib::D3D11Resource resource,
    D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv,
    uint32_t width,
    uint32_t height,
    TextureOwnership ownership)
    : XRTexture(BackendType::D3D11, width, height, ownership)
    , resource_(std::move(resource))
    , srv_(std::move(srv)) {}

D3D11Texture::D3D11Texture(
    D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srv,
    uint32_t width,
    uint32_t height)
    : XRTexture(BackendType::D3D11, width, height, TextureOwnership::External)
    , srv_(std::move(srv)) {}

struct D3D11Backend::Impl {
    std::shared_ptr<D3D11CoreLib::D3D11Core> core;
    D3D11BackendDesc desc{};
    std::shared_ptr<::VarjoSession> session;
    std::unique_ptr<::VarjoFrameInfo> frameInfo;
    std::unique_ptr<::VarjoLayerFrame> layerFrame;
    bool frameBegun = false;

    explicit Impl(std::shared_ptr<D3D11CoreLib::D3D11Core> inCore, D3D11BackendDesc inDesc)
        : core(std::move(inCore)), desc(inDesc) {
        if (!core) {
            throw std::runtime_error("D3D11Backend requires an externally-created D3D11Core shared_ptr.");
        }
    }

    void initialize(std::shared_ptr<::VarjoSession> inSession) {
        if (!inSession || !inSession->valid()) {
            throw std::runtime_error("D3D11Backend::initialize requires a valid external VarjoSession.");
        }
        session = std::move(inSession);
        frameInfo = std::make_unique<::VarjoFrameInfo>(*session);
        layerFrame = std::make_unique<::VarjoLayerFrame>(*session);
        if (!frameInfo || !frameInfo->valid()) {
            throw std::runtime_error("D3D11Backend failed to create VarjoFrameInfo.");
        }
        if (!layerFrame || !layerFrame->valid()) {
            throw std::runtime_error("D3D11Backend failed to create VarjoLayerFrame.");
        }
        OutputDebugStringA("[VarjoXR][D3D11] Backend initialized with external D3D11Core and external VarjoSession.\n");
    }

    void beginFrame() {
        if (frameBegun) {
            throw std::runtime_error("D3D11Backend::beginFrame called while a frame is already active.");
        }
        if (!frameInfo->waitSync()) {
            throw std::runtime_error("D3D11Backend::beginFrame: VarjoFrameInfo::waitSync failed: " + session->lastError());
        }
        if (!layerFrame->begin()) {
            throw std::runtime_error("D3D11Backend::beginFrame: VarjoLayerFrame::begin failed: " + layerFrame->lastError());
        }
        frameBegun = true;
    }

    void render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) {
        (void)planes;
        (void)frameContext;
        // TODO(rewrite/v0.1): create VarjoSwapChain, build D3D11 plane graphics pipeline,
        // run optional D3D11Processing, draw planes with per-eye Plane HLSL, and submit layer views.
        // This first rewrite commit intentionally fixes ownership/API boundaries before the draw path.
    }

    void endFrame() {
        if (!frameBegun) {
            throw std::runtime_error("D3D11Backend::endFrame requires beginFrame first.");
        }
        if (!layerFrame->endEmpty(frameInfo->frameNumber())) {
            frameBegun = false;
            throw std::runtime_error("D3D11Backend::endFrame: VarjoLayerFrame::endEmpty failed: " + layerFrame->lastError());
        }
        frameBegun = false;
    }
};

D3D11Backend::D3D11Backend(std::shared_ptr<D3D11CoreLib::D3D11Core> core, D3D11BackendDesc desc)
    : impl_(std::make_unique<Impl>(std::move(core), desc)) {}

D3D11Backend::~D3D11Backend() = default;

void D3D11Backend::initialize(std::shared_ptr<::VarjoSession> session) { impl_->initialize(std::move(session)); }
void D3D11Backend::beginFrame() { impl_->beginFrame(); }
void D3D11Backend::render(const std::vector<std::unique_ptr<XRPlane>>& planes, const FrameContext& frameContext) { impl_->render(planes, frameContext); }
void D3D11Backend::endFrame() { impl_->endFrame(); }

std::shared_ptr<D3D11Texture> D3D11Backend::createTextureFromRGBA(
    const uint8_t* rgba,
    uint32_t width,
    uint32_t height,
    uint32_t rowPitchBytes) {
    if (!rgba || width == 0 || height == 0) {
        throw std::runtime_error("D3D11Backend::createTextureFromRGBA received invalid input.");
    }
    auto resource = D3D11CoreLib::CreateTexture2DFromMemory(
        *impl_->core,
        rgba,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        rowPitchBytes);
    auto srv = D3D11CoreLib::CreateTexture2DSrv(*impl_->core, resource);
    return std::make_shared<D3D11Texture>(std::move(resource), std::move(srv), width, height, TextureOwnership::Owned);
}

std::shared_ptr<D3D11Texture> D3D11Backend::wrapTexture(ID3D11Texture2D* texture, DXGI_FORMAT srvFormat) {
    if (!texture) {
        throw std::runtime_error("D3D11Backend::wrapTexture received null texture.");
    }
    D3D11CoreLib::ComPtr<ID3D11Texture2D> texturePtr = texture;
    D3D11CoreLib::D3D11Resource resource(texturePtr);
    auto srv = D3D11CoreLib::CreateTexture2DSrv(*impl_->core, resource, srvFormat);
    const auto desc = resource.GetTexture2DDesc();
    return std::make_shared<D3D11Texture>(std::move(resource), std::move(srv), desc.Width, desc.Height, TextureOwnership::External);
}

std::shared_ptr<D3D11Texture> D3D11Backend::wrapSrv(ID3D11ShaderResourceView* srv, uint32_t width, uint32_t height) {
    if (!srv) {
        throw std::runtime_error("D3D11Backend::wrapSrv received null SRV.");
    }
    D3D11CoreLib::ComPtr<ID3D11ShaderResourceView> srvPtr = srv;
    return std::make_shared<D3D11Texture>(std::move(srvPtr), width, height);
}

std::unique_ptr<IRenderBackend> CreateBackend(std::shared_ptr<D3D11CoreLib::D3D11Core> core, D3D11BackendDesc desc) {
    return std::make_unique<D3D11Backend>(std::move(core), desc);
}

} // namespace VarjoXR::Backends::D3D11
