#include <VarjoXR/VarjoXR.hpp>

#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>

#include "ProgrammableProcessingSampleCommon.hpp"

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

        D3D12CoreLib::D3D12CoreConfig config{};
        config.createDirectQueue = true;
        config.createCopyQueue = true;
        auto d3d = D3D12CoreLib::D3D12Core::CreateShared(config);

        auto backend = VarjoXR::Backends::D3D12::CreateBackend(d3d);
        VarjoXR::XRSpace space({session, std::move(backend)});

        auto& plane = space.createPlane({1.0f, 0.6f});
        plane.setPlacementMode(VarjoXR::PlacementMode::World);
        plane.transform().position = {0.0f, 0.0f, -1.0f};

        constexpr std::uint32_t kWidth = 512;
        constexpr std::uint32_t kHeight = 512;
        auto rgba = ProgrammableProcessingSample::MakeGradientRgba(kWidth, kHeight);
        auto texture = static_cast<VarjoXR::Backends::D3D12::D3D12Backend&>(space.backend())
            .createTextureFromRGBA(rgba.data(), kWidth, kHeight, kWidth * 4u);
        plane.setTexture(texture);

        ProgrammableProcessingSample::CircleDarkenConstants constants{};
        constants.centerX = 0.5f;
        constants.centerY = 0.5f;
        constants.radius = 0.28f;
        constants.outsideBrightness = 0.45f;
        constants.edgeSoftness = 0.035f;
        constants.pulseStrength = 0.035f;

        auto processing = ProgrammableProcessingSample::MakeCircleDarkenProcessing(
            ProgrammableProcessingSample::ShaderDirectory(),
            constants,
            {kWidth, kHeight});
        plane.setProcessing(processing);

        plane.setTint({1.0f, 1.0f, 1.0f, 1.0f});

        while (true) {
            space.frameContext().timeSeconds = ProgrammableProcessingSample::SecondsSinceStart();
            space.update();
        }
    } catch (const std::exception& e) {
        std::cerr << "ProgrammableProcessing_D3D12 failed: " << e.what() << '\n';
        return 1;
    }
}
