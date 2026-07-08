#include "../Common/RenderingPlaneSampleCommon.hpp"

#include <exception>
#include <memory>
#include <utility>

int main() {
    constexpr const char* kSampleName = "RenderingPlane_02_HeadRelativePlane";
    try {
        auto session = RenderingPlaneSample::CreateSessionOrThrow();
        auto backend = RenderingPlaneSample::CreateBackend();
        VarjoXR::XRSpace space({session, std::move(backend)});

        auto& plane = space.createPlane({0.8f, 0.45f});
        plane.setPlacementMode(VarjoXR::PlacementMode::HeadRelative);
        plane.transform().position = {0.0f, -0.05f, -0.9f};

        auto rgba = RenderingPlaneSample::MakeCheckerRgba(
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight,
            {255, 230, 80, 255},
            {50, 50, 50, 255},
            24);
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
