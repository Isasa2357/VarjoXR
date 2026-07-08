#include "../Common/RenderingPlaneSampleCommon.hpp"

#include <cmath>
#include <exception>
#include <memory>
#include <utility>

int main() {
    constexpr const char* kSampleName = "RenderingPlane_06_ProcessingPlane";
    try {
        auto session = RenderingPlaneSample::CreateSessionOrThrow();
        auto backend = RenderingPlaneSample::CreateBackend();
        VarjoXR::XRSpace space({session, std::move(backend)});

        auto& plane = space.createPlane({1.0f, 0.6f});
        plane.setPlacementMode(VarjoXR::PlacementMode::World);
        plane.transform().position = {0.0f, 0.0f, -1.2f};

        auto rgba = RenderingPlaneSample::MakeGradientRgba(
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight);
        plane.setTexture(RenderingPlaneSample::CreateTextureFromRGBA(
            space,
            rgba,
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight));

        RenderingPlaneSample::CircleProcessingConstants constants{};
        constants.radius = 0.28f;
        constants.outsideBrightness = 0.45f;
        constants.edgeSoftness = 0.035f;
        constants.pulseStrength = 0.035f;

        auto processing = RenderingPlaneSample::MakeAnimatedCircleProcessing(
            constants,
            {RenderingPlaneSample::kDefaultTextureWidth, RenderingPlaneSample::kDefaultTextureHeight});

        RenderingPlaneSample::PrintStart(kSampleName);
        while (true) {
            const double t = RenderingPlaneSample::SecondsSinceStart();
            constants.centerX = 0.5f + 0.16f * static_cast<float>(std::sin(t * 0.7));
            constants.centerY = 0.5f + 0.10f * static_cast<float>(std::sin(t * 1.1));
            processing.userConstants.set(constants);
            plane.setProcessing(processing);

            space.frameContext().timeSeconds = t;
            ++space.frameContext().frameNumber;
            space.update();
        }
    } catch (const std::exception& e) {
        return RenderingPlaneSample::ReportFailure(kSampleName, e);
    }
}
