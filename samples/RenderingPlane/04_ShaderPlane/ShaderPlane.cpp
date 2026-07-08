#include "../Common/RenderingPlaneSampleCommon.hpp"

#include <exception>
#include <memory>
#include <utility>

int main() {
    constexpr const char* kSampleName = "RenderingPlane_04_ShaderPlane";
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

        plane.setPixelShaderHLSL(R"hlsl(
float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 c = xrTexture.Sample(xrSampler, uv);
    float2 p = uv * 2.0f - 1.0f;
    float vignette = saturate(1.0f - dot(p, p) * 0.55f);
    float gridX = step(0.985f, frac(uv.x * 10.0f));
    float gridY = step(0.985f, frac(uv.y * 6.0f));
    float grid = max(gridX, gridY) * 0.35f;
    c.rgb = c.rgb * vignette + grid.xxx;
    return c * tint;
}
)hlsl");

        RenderingPlaneSample::PrintStart(kSampleName);
        RenderingPlaneSample::RunForever(space);
    } catch (const std::exception& e) {
        return RenderingPlaneSample::ReportFailure(kSampleName, e);
    }
}
