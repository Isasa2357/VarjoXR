#include "../Common/RenderingPlaneSampleCommon.hpp"

#include <exception>
#include <memory>
#include <utility>

namespace {

void AddPlane(
    VarjoXR::XRSpace& space,
    glm::vec2 sizeMeters,
    glm::vec3 position,
    RenderingPlaneSample::Rgba a,
    RenderingPlaneSample::Rgba b) {
    auto& plane = space.createPlane(sizeMeters);
    plane.setPlacementMode(VarjoXR::PlacementMode::World);
    plane.transform().position = position;
    auto rgba = RenderingPlaneSample::MakeCheckerRgba(
        RenderingPlaneSample::kDefaultTextureWidth,
        RenderingPlaneSample::kDefaultTextureHeight,
        a,
        b,
        32);
    plane.setTexture(RenderingPlaneSample::CreateTextureFromRGBA(
        space,
        rgba,
        RenderingPlaneSample::kDefaultTextureWidth,
        RenderingPlaneSample::kDefaultTextureHeight));
}

} // namespace

int main() {
    constexpr const char* kSampleName = "RenderingPlane_05_MultiplePlanes";
    try {
        auto session = RenderingPlaneSample::CreateSessionOrThrow();
        auto backend = RenderingPlaneSample::CreateBackend();
        VarjoXR::XRSpace space({session, std::move(backend)});

        AddPlane(space, {0.48f, 0.32f}, {-0.55f, 0.05f, -1.3f}, {255, 80, 80, 255}, {50, 30, 30, 255});
        AddPlane(space, {0.52f, 0.34f}, {0.00f, -0.03f, -1.1f}, {80, 255, 120, 255}, {25, 50, 35, 255});
        AddPlane(space, {0.48f, 0.32f}, {0.55f, 0.05f, -1.3f}, {80, 160, 255, 255}, {30, 35, 55, 255});

        RenderingPlaneSample::PrintStart(kSampleName);
        RenderingPlaneSample::RunForever(space);
    } catch (const std::exception& e) {
        return RenderingPlaneSample::ReportFailure(kSampleName, e);
    }
}
