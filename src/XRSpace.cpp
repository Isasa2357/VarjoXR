#include <VarjoXR/XRSpace.hpp>

#include <VarjoXR/Detail/IRenderBackend.hpp>
#include <VarjoXR/VarjoSession.hpp>

#if defined(VARJOXR_ENABLE_D3D11)
#include <VarjoXR/Backends/D3D11/D3D11Backend.hpp>
#endif
#if defined(VARJOXR_ENABLE_D3D12)
#include <VarjoXR/Backends/D3D12/D3D12Backend.hpp>
#endif

#include <stdexcept>

namespace VarjoXR {

XRSpace::XRSpace(const XRSpaceConfig& config)
    : m_config(config), m_session(std::make_unique<VarjoSession>()) {
    switch (m_config.backend) {
#if defined(VARJOXR_ENABLE_D3D11)
    case BackendType::D3D11:
        m_backend = std::make_unique<Backends::D3D11::D3D11Backend>();
        break;
#endif
#if defined(VARJOXR_ENABLE_D3D12)
    case BackendType::D3D12:
        m_backend = std::make_unique<Backends::D3D12::D3D12Backend>();
        break;
#endif
    default:
        throw std::runtime_error("Requested VarjoXR backend is not enabled in this build.");
    }

    m_backend->initialize(*m_session, m_config);
}

XRSpace::~XRSpace() = default;

XRPlane* XRSpace::createPlane(Vec2 size) {
    auto plane = std::make_unique<XRPlane>(size);
    auto* ptr = plane.get();
    m_objects.emplace_back(std::move(plane));
    return ptr;
}

std::shared_ptr<XRTexture> XRSpace::createTextureFromRGBA(
    const uint8_t* rgba,
    uint32_t width,
    uint32_t height,
    uint32_t rowPitchBytes) {
    if (!m_backend) throw std::runtime_error("XRSpace backend is not initialized.");
    return m_backend->createTextureFromRGBA(rgba, width, height, rowPitchBytes);
}

#if defined(VARJOXR_ENABLE_D3D11)
std::shared_ptr<XRTexture> XRSpace::createTextureFromD3D11Resource(
    ID3D11Texture2D* texture,
    DXGI_FORMAT srvFormat) {
    if (!m_backend) throw std::runtime_error("XRSpace backend is not initialized.");
    return m_backend->createTextureFromD3D11Resource(texture, srvFormat);
}

std::shared_ptr<XRTexture> XRSpace::createTextureFromD3D11Srv(
    ID3D11ShaderResourceView* srv,
    uint32_t width,
    uint32_t height) {
    if (!m_backend) throw std::runtime_error("XRSpace backend is not initialized.");
    return m_backend->createTextureFromD3D11Srv(srv, width, height);
}
#endif

#if defined(VARJOXR_ENABLE_D3D12)
std::shared_ptr<XRTexture> XRSpace::createTextureFromD3D12Resource(
    ID3D12Resource* resource,
    DXGI_FORMAT srvFormat) {
    if (!m_backend) throw std::runtime_error("XRSpace backend is not initialized.");
    return m_backend->createTextureFromD3D12Resource(resource, srvFormat);
}
#endif

void XRSpace::update() {
    if (!m_backend) return;
    m_session->pollEvents();
    m_backend->renderFrame(*m_session, m_objects);
}

VarjoSession& XRSpace::session() {
    return *m_session;
}

} // namespace VarjoXR
