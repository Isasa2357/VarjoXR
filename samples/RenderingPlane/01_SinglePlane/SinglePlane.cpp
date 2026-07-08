#include "../Common/RenderingPlaneSampleCommon.hpp"

#include <exception>
#include <memory>
#include <utility>

int main() {
    constexpr const char* kSampleName = "RenderingPlane_01_SinglePlane";
    try {
        auto session = RenderingPlaneSample::CreateSessionOrThrow();
        auto backend = RenderingPlaneSample::CreateBackend();
        VarjoXR::XRSpace space({session, std::move(backend)});

        auto& plane = space.createPlane({1.0f, 0.6f});
        plane.setPlacementMode(VarjoXR::PlacementMode::World);
        plane.transform().position = {0.0f, 0.0f, -1.2f};

        auto rgba = RenderingPlaneSample::MakeCheckerRgba(
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight,
            {240, 240, 240, 255},
            {40, 120, 255, 255});
        plane.setTexture(RenderingPlaneSample::CreateTextureFromRGBA(
            space,
            rgba,
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight));

        RenderingPlaneSample::PrintStart(kSampleName);
        RenderingPlaneSample::RunForever(space);
    } catch (const std::exception& e) {
        return RenderingPlaneSample::ReportFailure(kSampleName, e);
    }
}
