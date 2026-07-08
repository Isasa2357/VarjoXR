#include <VarjoXR/VarjoXR.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

int g_assertions = 0;

void require(bool condition, const char* message) {
    ++g_assertions;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireNear(float actual, float expected, float eps, const char* message) {
    ++g_assertions;
    if (std::fabs(actual - expected) > eps) {
        throw std::runtime_error(message);
    }
}

class DummyTexture final : public VarjoXR::XRTexture {
public:
    DummyTexture(VarjoXR::BackendType backend, uint32_t width, uint32_t height)
        : VarjoXR::XRTexture(backend, width, height, VarjoXR::TextureOwnership::External) {}
};

void testTransformMatrix() {
    VarjoXR::Transform t{};
    t.position = {1.0f, 2.0f, 3.0f};
    t.scale = {2.0f, 3.0f, 4.0f};
    const glm::mat4 m = t.matrix();
    requireNear(m[3][0], 1.0f, 1.0e-5f, "translation x mismatch");
    requireNear(m[3][1], 2.0f, 1.0e-5f, "translation y mismatch");
    requireNear(m[3][2], 3.0f, 1.0e-5f, "translation z mismatch");
    requireNear(m[0][0], 2.0f, 1.0e-5f, "scale x mismatch");
    requireNear(m[1][1], 3.0f, 1.0e-5f, "scale y mismatch");
    requireNear(m[2][2], 4.0f, 1.0e-5f, "scale z mismatch");
}

void testPlaneMaterialsAreEyeIndependent() {
    VarjoXR::XRPlane plane({1.0f, 0.6f});
    auto left = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D11, 10, 20);
    auto right = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D11, 30, 40);

    plane.setTexture(VarjoXR::Eye::Left, left);
    plane.setTexture(VarjoXR::Eye::Right, right);
    plane.setPixelShaderHLSL(VarjoXR::Eye::Left, "float4 main(float2 uv:TEXCOORD0):SV_TARGET{return 1;}\n");
    plane.setPixelShaderHLSL(VarjoXR::Eye::Right, "float4 main(float2 uv:TEXCOORD0):SV_TARGET{return 0;}\n");

    require(plane.material(VarjoXR::Eye::Left).texture == left, "left texture mismatch");
    require(plane.material(VarjoXR::Eye::Right).texture == right, "right texture mismatch");
    require(plane.material(VarjoXR::Eye::Left).planePixelShaderHlsl != plane.material(VarjoXR::Eye::Right).planePixelShaderHlsl,
            "left/right shader should be independent");
}

void testPlacementAndProcessingDesc() {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    plane.setPlacementMode(VarjoXR::PlacementMode::HeadRelative);
    require(plane.placementMode() == VarjoXR::PlacementMode::HeadRelative, "placement mode mismatch");

    VarjoXR::TextureProcessingDesc processing{};
    processing.enabled = true;
    processing.timing = VarjoXR::ProcessingTiming::BeforeRenderEachFrame;
    processing.hlsl = "[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){}\n";
    processing.outputSize = {640, 480};
    processing.params0 = {1.0f, 2.0f, 3.0f, 4.0f};
    plane.setProcessing(VarjoXR::Eye::Left, processing);
    require(plane.material(VarjoXR::Eye::Left).processing.enabled, "processing enabled mismatch");
    require(plane.material(VarjoXR::Eye::Left).processing.timing == VarjoXR::ProcessingTiming::BeforeRenderEachFrame,
            "processing timing mismatch");
    require(plane.material(VarjoXR::Eye::Left).processing.outputSize.x == 640, "processing output width mismatch");
    require(!plane.material(VarjoXR::Eye::Right).processing.enabled, "right processing should remain default");
}

} // namespace

int main() {
    try {
        testTransformMatrix();
        testPlaneMaterialsAreEyeIndependent();
        testPlacementAndProcessingDesc();
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return 1;
    }

    std::cout << "[PASS] VarjoXRCoreTests, assertions=" << g_assertions << '\n';
    return 0;
}
