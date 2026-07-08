#include <VarjoXR/VarjoXR.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>

int main() {
    try {
        auto session = std::make_shared<VarjoSession>();
        if (!session->valid() && !session->initialize()) {
            std::cerr << "Failed to initialize Varjo session: " << session->lastError() << '\n';
            return 1;
        }

        auto d3d = D3D11CoreLib::D3D11Core::CreateShared();
        auto backend = VarjoXR::Backends::D3D11::CreateBackend(d3d);
        VarjoXR::XRSpace space({session, std::move(backend)});

        auto& plane = space.createPlane({1.0f, 0.6f});
        plane.setPlacementMode(VarjoXR::PlacementMode::World);
        plane.transform().position = {0.0f, 0.0f, -1.0f};

        const std::array<std::uint8_t, 4> white = {255, 255, 255, 255};
        auto texture = static_cast<VarjoXR::Backends::D3D11::D3D11Backend&>(space.backend())
            .createTextureFromRGBA(white.data(), 1, 1, 4);
        plane.setTexture(texture);

        plane.setPixelShaderHLSL(R"hlsl(
float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 c = xrTexture.Sample(xrSampler, uv);
    return c * tint;
}
)hlsl");

        while (true) {
            space.update();
        }
    } catch (const std::exception& e) {
        std::cerr << "RenderingPlane_D3D11 failed: " << e.what() << '\n';
        return 1;
    }
}
