#include "../Common/RenderingPlaneSampleCommon.hpp"

#include <exception>
#include <memory>
#include <utility>

namespace {

std::string LeftEyePixelShader() {
    return R"hlsl(
float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 c = xrTexture.Sample(xrSampler, uv);
    float stripe = step(0.92f, frac(uv.y * 12.0f));
    c.rgb = c.rgb * float3(1.25f, 0.65f, 0.55f) + stripe.xxx * 0.25f;
    return c * tint;
}
)hlsl";
}

std::string RightEyePixelShader() {
    return R"hlsl(
float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 c = xrTexture.Sample(xrSampler, uv);
    float stripe = step(0.92f, frac(uv.x * 12.0f));
    c.rgb = c.rgb * float3(0.55f, 0.85f, 1.25f) + float3(0.0f, stripe * 0.22f, stripe * 0.35f);
    return c * tint;
}
)hlsl";
}

} // namespace

int main() {
    constexpr const char* kSampleName = "RenderingPlane_07_EyeMaterialVariants";
    try {
        auto session = RenderingPlaneSample::CreateSessionOrThrow();
        auto backend = RenderingPlaneSample::CreateBackend();
        VarjoXR::XRSpace space({session, std::move(backend)});

        auto& plane = space.createPlane({1.0f, 0.6f});
        plane.setPlacementMode(VarjoXR::PlacementMode::World);
        plane.transform().position = {0.0f, 0.0f, -1.2f};

        auto leftRgba = RenderingPlaneSample::MakeGradientRgba(
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight);
        auto rightRgba = RenderingPlaneSample::MakeCheckerRgba(
            RenderingPlaneSample::kDefaultTextureWidth,
            RenderingPlaneSample::kDefaultTextureHeight,
            {60, 220, 255, 255},
            {30, 30, 70, 255},
            24);

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

        plane.setPixelShaderHLSL(VarjoXR::Eye::Left, LeftEyePixelShader());
        plane.setPixelShaderHLSL(VarjoXR::Eye::Right, RightEyePixelShader());

        RenderingPlaneSample::CircleProcessingConstants leftConstants{};
        leftConstants.centerX = 0.35f;
        leftConstants.centerY = 0.50f;
        leftConstants.radius = 0.24f;
        leftConstants.outsideBrightness = 0.52f;
        leftConstants.edgeSoftness = 0.025f;
        leftConstants.pulseStrength = 0.025f;

        RenderingPlaneSample::CircleProcessingConstants rightConstants{};
        rightConstants.centerX = 0.65f;
        rightConstants.centerY = 0.50f;
        rightConstants.radius = 0.30f;
        rightConstants.outsideBrightness = 0.38f;
        rightConstants.edgeSoftness = 0.045f;
        rightConstants.pulseStrength = 0.020f;

        auto leftProcessing = RenderingPlaneSample::MakeAnimatedCircleProcessing(
            leftConstants,
            {RenderingPlaneSample::kDefaultTextureWidth, RenderingPlaneSample::kDefaultTextureHeight});
        auto rightProcessing = RenderingPlaneSample::MakeAnimatedCircleProcessing(
            rightConstants,
            {RenderingPlaneSample::kDefaultTextureWidth, RenderingPlaneSample::kDefaultTextureHeight});

        RenderingPlaneSample::PrintStart(kSampleName);
        while (true) {
            const double t = RenderingPlaneSample::SecondsSinceStart();
            leftConstants.centerX = 0.35f + 0.10f * static_cast<float>(std::sin(t * 0.8));
            leftConstants.centerY = 0.50f + 0.08f * static_cast<float>(std::sin(t * 1.3));
            rightConstants.centerX = 0.65f + 0.10f * static_cast<float>(std::sin(t * 1.0));
            rightConstants.centerY = 0.50f + 0.08f * static_cast<float>(std::sin(t * 1.5));
            leftProcessing.userConstants.set(leftConstants);
            rightProcessing.userConstants.set(rightConstants);
            plane.setProcessing(VarjoXR::Eye::Left, leftProcessing);
            plane.setProcessing(VarjoXR::Eye::Right, rightProcessing);

            space.frameContext().timeSeconds = t;
            ++space.frameContext().frameNumber;
            space.update();
        }
    } catch (const std::exception& e) {
        return RenderingPlaneSample::ReportFailure(kSampleName, e);
    }
}
