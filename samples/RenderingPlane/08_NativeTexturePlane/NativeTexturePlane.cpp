#include "../Common/RenderingPlaneSampleCommon.hpp"

#include <exception>
#include <memory>
#include <utility>
#include <vector>

int main() {
    constexpr const char* kSampleName = "RenderingPlane_08_NativeTexturePlane";
    try {
        auto session = RenderingPlaneSample::CreateSessionOrThrow();

#if defined(RENDERING_PLANE_SAMPLE_D3D11)
        auto d3d = D3D11CoreLib::D3D11Core::CreateShared();
        auto backend = VarjoXR::Backends::D3D11::CreateBackend(d3d);
#elif defined(RENDERING_PLANE_SAMPLE_D3D12)
        D3D12CoreLib::D3D12CoreConfig config{};
        config.createDirectQueue = true;
        config.createCopyQueue = true;
        auto d3d = D3D12CoreLib::D3D12Core::CreateShared(config);
        auto backend = VarjoXR::Backends::D3D12::CreateBackend(d3d);
#endif

        VarjoXR::XRSpace space({session, std::move(backend)});

        auto& plane = space.createPlane({1.0f, 0.6f});
        plane.setPlacementMode(VarjoXR::PlacementMode::World);
        plane.transform().position = {0.0f, 0.0f, -1.2f};

        auto rgba = RenderingPlaneSample::MakeCheckerRgba(
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight,
            {255, 255, 255, 255},
            {180, 70, 255, 255},
            32);

#if defined(RENDERING_PLANE_SAMPLE_D3D11)
        auto nativeTexture = D3D11CoreLib::CreateTexture2DFromRGBA(
            *d3d,
            rgba.data(),
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight,
            D3D11_BIND_SHADER_RESOURCE);
        auto& d3d11Backend = static_cast<VarjoXR::Backends::D3D11::D3D11Backend&>(space.backend());
        plane.setTexture(d3d11Backend.wrapTexture(nativeTexture.AsTexture2D(), DXGI_FORMAT_R8G8B8A8_UNORM));
#elif defined(RENDERING_PLANE_SAMPLE_D3D12)
        auto nativeTexture = D3D12CoreLib::CreateTexture2DFromRGBA(
            *d3d,
            rgba.data(),
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        auto& d3d12Backend = static_cast<VarjoXR::Backends::D3D12::D3D12Backend&>(space.backend());
        plane.setTexture(d3d12Backend.wrapResource(nativeTexture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM));
#endif

        RenderingPlaneSample::PrintStart(kSampleName);
        RenderingPlaneSample::RunForever(space);
    } catch (const std::exception& e) {
        return RenderingPlaneSample::ReportFailure(kSampleName, e);
    }
}
