#include "../Common/RenderingPlaneSampleCommon.hpp"

#include <exception>
#include <memory>
#include <utility>

int main() {
    constexpr const char* kSampleName = "RenderingPlane_03_StereoPlane";
    try {
        auto session = RenderingPlaneSample::CreateSessionOrThrow();
        auto backend = RenderingPlaneSample::CreateBackend();
        VarjoXR::XRSpace space({session, std::move(backend)});

        auto& plane = space.createPlane({1.0f, 0.6f});
        plane.setPlacementMode(VarjoXR::PlacementMode::World);
        plane.transform().position = {0.0f, 0.0f, -1.2f};

        auto leftRgba = RenderingPlaneSample::MakeCheckerRgba(
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight,
            {255, 80, 80, 255},
            {40, 40, 40, 255},
            32);
        auto rightRgba = RenderingPlaneSample::MakeCheckerRgba(
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight,
            {80, 160, 255, 255},
            {40, 40, 40, 255},
            32);

        plane.setTexture(VarjoXR::Eye::Left, RenderingPlaneSample::CreateTextureFromRGBA(
            space,
            leftRgba,
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight));
        plane.setTexture(VarjoXR::Eye::Right, RenderingPlaneSample::CreateTextureFromRGBA(
            space,
            rightRgba,
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight));

        RenderingPlaneSample::PrintStart(kSampleName);
        RenderingPlaneSample::RunForever(space);
    } catch (const std::exception& e) {
        return RenderingPlaneSample::ReportFailure(kSampleName, e);
    }
}
