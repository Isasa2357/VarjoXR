#include <VarjoXR/BackendType.hpp>
#include <VarjoXR/Eye.hpp>
#include <VarjoXR/Material.hpp>
#include <VarjoXR/Math.hpp>
#include <VarjoXR/Texture.hpp>
#include <VarjoXR/XRObject.hpp>
#include <VarjoXR/XRPlane.hpp>
#include <VarjoXR/XRSpace.hpp>

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TestFailure final : public std::runtime_error {
public:
    explicit TestFailure(const std::string& message) : std::runtime_error(message) {}
};

struct TestCase {
    std::string name;
    std::function<void()> run;
};

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct RegisterTest {
    RegisterTest(std::string name, std::function<void()> run) {
        Registry().push_back({std::move(name), std::move(run)});
    }
};

int& AssertionCount() {
    static int count = 0;
    return count;
}

void Fail(const char* file, int line, const std::string& message) {
    std::ostringstream oss;
    oss << file << ':' << line << ": " << message;
    throw TestFailure(oss.str());
}

#define VX_CONCAT_IMPL(a, b) a##b
#define VX_CONCAT(a, b) VX_CONCAT_IMPL(a, b)
#define VX_TEST(name) \
    static void VX_CONCAT(Test_, __LINE__)(); \
    static RegisterTest VX_CONCAT(Register_, __LINE__)(name, VX_CONCAT(Test_, __LINE__)); \
    static void VX_CONCAT(Test_, __LINE__)()

#define VX_REQUIRE(expr) \
    do { \
        ++AssertionCount(); \
        if (!(expr)) { \
            Fail(__FILE__, __LINE__, std::string("Requirement failed: ") + #expr); \
        } \
    } while (false)

#define VX_REQUIRE_EQ(actual, expected) \
    do { \
        ++AssertionCount(); \
        const auto vxActual = (actual); \
        const auto vxExpected = (expected); \
        if (!(vxActual == vxExpected)) { \
            std::ostringstream vxOss; \
            vxOss << "Expected " #actual " == " #expected ", actual=" << vxActual << ", expected=" << vxExpected; \
            Fail(__FILE__, __LINE__, vxOss.str()); \
        } \
    } while (false)

#define VX_REQUIRE_NEAR(actual, expected, eps) \
    do { \
        ++AssertionCount(); \
        const double vxActual = static_cast<double>(actual); \
        const double vxExpected = static_cast<double>(expected); \
        const double vxEps = static_cast<double>(eps); \
        if (std::fabs(vxActual - vxExpected) > vxEps) { \
            std::ostringstream vxOss; \
            vxOss << "Expected near: " #actual " ~= " #expected ", actual=" << vxActual << ", expected=" << vxExpected << ", eps=" << vxEps; \
            Fail(__FILE__, __LINE__, vxOss.str()); \
        } \
    } while (false)

constexpr double kEps = 1.0e-5;

class DummyTexture final : public VarjoXR::XRTexture {
public:
    DummyTexture(VarjoXR::BackendType backend, uint32_t width, uint32_t height)
        : VarjoXR::XRTexture(backend, width, height) {}
};

void RequireIdentity(const VarjoXR::Mat4& m) {
    const float expected[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
    for (int i = 0; i < 16; ++i) {
        VX_REQUIRE_NEAR(m.m[static_cast<size_t>(i)], expected[i], kEps);
    }
}

} // namespace

VX_TEST("EyeIndex returns stable left and right indices") {
    VX_REQUIRE_EQ(VarjoXR::EyeIndex(VarjoXR::Eye::Left), static_cast<size_t>(0));
    VX_REQUIRE_EQ(VarjoXR::EyeIndex(VarjoXR::Eye::Right), static_cast<size_t>(1));
    VX_REQUIRE_EQ(VarjoXR::kEyeCount, 2);
}

VX_TEST("EyeArray can store independent values for both eyes") {
    VarjoXR::EyeArray<int> values{10, 20};
    VX_REQUIRE_EQ(values[VarjoXR::EyeIndex(VarjoXR::Eye::Left)], 10);
    VX_REQUIRE_EQ(values[VarjoXR::EyeIndex(VarjoXR::Eye::Right)], 20);

    values[VarjoXR::EyeIndex(VarjoXR::Eye::Left)] = 30;
    VX_REQUIRE_EQ(values[0], 30);
    VX_REQUIRE_EQ(values[1], 20);
}

VX_TEST("BackendType enum distinguishes D3D11 and D3D12") {
    VX_REQUIRE(static_cast<int>(VarjoXR::BackendType::D3D11) != static_cast<int>(VarjoXR::BackendType::D3D12));
}

VX_TEST("Mat4::Identity returns diagonal identity matrix") {
    RequireIdentity(VarjoXR::Mat4::Identity());
}

VX_TEST("MakeMat4FromVarjoDoubleArray returns identity for null input") {
    RequireIdentity(VarjoXR::MakeMat4FromVarjoDoubleArray(nullptr));
}

VX_TEST("MakeMat4FromVarjoDoubleArray copies all sixteen elements as floats") {
    double input[16] = {};
    for (int i = 0; i < 16; ++i) input[i] = static_cast<double>(i) + 0.25;

    const auto m = VarjoXR::MakeMat4FromVarjoDoubleArray(input);
    for (int i = 0; i < 16; ++i) {
        VX_REQUIRE_NEAR(m.m[static_cast<size_t>(i)], static_cast<float>(input[i]), kEps);
    }
}

VX_TEST("Default Transform values are position zero, identity rotation, scale one") {
    const VarjoXR::Transform t{};
    VX_REQUIRE_NEAR(t.position.x, 0.0, kEps);
    VX_REQUIRE_NEAR(t.position.y, 0.0, kEps);
    VX_REQUIRE_NEAR(t.position.z, 0.0, kEps);
    VX_REQUIRE_NEAR(t.rotation.x, 0.0, kEps);
    VX_REQUIRE_NEAR(t.rotation.y, 0.0, kEps);
    VX_REQUIRE_NEAR(t.rotation.z, 0.0, kEps);
    VX_REQUIRE_NEAR(t.rotation.w, 1.0, kEps);
    VX_REQUIRE_NEAR(t.scale.x, 1.0, kEps);
    VX_REQUIRE_NEAR(t.scale.y, 1.0, kEps);
    VX_REQUIRE_NEAR(t.scale.z, 1.0, kEps);
}

VX_TEST("Default Transform matrix is identity") {
    const VarjoXR::Transform t{};
    RequireIdentity(t.matrix());
}

VX_TEST("Transform translation is stored in transposed HLSL-friendly matrix layout") {
    VarjoXR::Transform t{};
    t.position = {1.5f, -2.0f, 3.25f};

    const auto m = t.matrix();
    VX_REQUIRE_NEAR(m.m[0], 1.0, kEps);
    VX_REQUIRE_NEAR(m.m[5], 1.0, kEps);
    VX_REQUIRE_NEAR(m.m[10], 1.0, kEps);
    VX_REQUIRE_NEAR(m.m[15], 1.0, kEps);
    VX_REQUIRE_NEAR(m.m[3], 1.5, kEps);
    VX_REQUIRE_NEAR(m.m[7], -2.0, kEps);
    VX_REQUIRE_NEAR(m.m[11], 3.25, kEps);
}

VX_TEST("Transform scale affects diagonal matrix elements") {
    VarjoXR::Transform t{};
    t.scale = {2.0f, 3.0f, 4.0f};

    const auto m = t.matrix();
    VX_REQUIRE_NEAR(m.m[0], 2.0, kEps);
    VX_REQUIRE_NEAR(m.m[5], 3.0, kEps);
    VX_REQUIRE_NEAR(m.m[10], 4.0, kEps);
    VX_REQUIRE_NEAR(m.m[15], 1.0, kEps);
}

VX_TEST("Transform combines scale and translation without cross-contamination") {
    VarjoXR::Transform t{};
    t.position = {-1.0f, 2.0f, -3.0f};
    t.scale = {4.0f, 5.0f, 6.0f};

    const auto m = t.matrix();
    VX_REQUIRE_NEAR(m.m[0], 4.0, kEps);
    VX_REQUIRE_NEAR(m.m[5], 5.0, kEps);
    VX_REQUIRE_NEAR(m.m[10], 6.0, kEps);
    VX_REQUIRE_NEAR(m.m[3], -1.0, kEps);
    VX_REQUIRE_NEAR(m.m[7], 2.0, kEps);
    VX_REQUIRE_NEAR(m.m[11], -3.0, kEps);
}

VX_TEST("Transform supports negative scale components") {
    VarjoXR::Transform t{};
    t.scale = {-1.0f, 2.0f, -3.0f};

    const auto m = t.matrix();
    VX_REQUIRE_NEAR(m.m[0], -1.0, kEps);
    VX_REQUIRE_NEAR(m.m[5], 2.0, kEps);
    VX_REQUIRE_NEAR(m.m[10], -3.0, kEps);
}

VX_TEST("Transform rotation quaternion around Z produces orthonormal basis") {
    VarjoXR::Transform t{};
    const float half = 0.70710678118f;
    t.rotation = {0.0f, 0.0f, half, half};

    const auto m = t.matrix();
    const double col0Len = std::sqrt(m.m[0] * m.m[0] + m.m[4] * m.m[4] + m.m[8] * m.m[8]);
    const double col1Len = std::sqrt(m.m[1] * m.m[1] + m.m[5] * m.m[5] + m.m[9] * m.m[9]);
    const double col0DotCol1 = m.m[0] * m.m[1] + m.m[4] * m.m[5] + m.m[8] * m.m[9];
    VX_REQUIRE_NEAR(col0Len, 1.0, 1.0e-4);
    VX_REQUIRE_NEAR(col1Len, 1.0, 1.0e-4);
    VX_REQUIRE_NEAR(col0DotCol1, 0.0, 1.0e-4);
}

VX_TEST("XRTexture stores backend and dimensions") {
    DummyTexture tex(VarjoXR::BackendType::D3D11, 640, 480);
    VX_REQUIRE(tex.backend() == VarjoXR::BackendType::D3D11);
    VX_REQUIRE_EQ(tex.width(), static_cast<uint32_t>(640));
    VX_REQUIRE_EQ(tex.height(), static_cast<uint32_t>(480));
}

VX_TEST("XRTexture accepts zero dimensions for backend-owned placeholder resources") {
    DummyTexture tex(VarjoXR::BackendType::D3D12, 0, 0);
    VX_REQUIRE(tex.backend() == VarjoXR::BackendType::D3D12);
    VX_REQUIRE_EQ(tex.width(), static_cast<uint32_t>(0));
    VX_REQUIRE_EQ(tex.height(), static_cast<uint32_t>(0));
}

VX_TEST("Default Material has no texture, empty shader, and white tint") {
    const VarjoXR::Material material{};
    VX_REQUIRE(material.texture == nullptr);
    VX_REQUIRE(material.pixelShaderHlsl.empty());
    VX_REQUIRE_NEAR(material.tint.x, 1.0, kEps);
    VX_REQUIRE_NEAR(material.tint.y, 1.0, kEps);
    VX_REQUIRE_NEAR(material.tint.z, 1.0, kEps);
    VX_REQUIRE_NEAR(material.tint.w, 1.0, kEps);
}

VX_TEST("XRPlane constructor stores size and reports plane kind") {
    const VarjoXR::XRPlane plane({1.25f, 0.75f});
    VX_REQUIRE(plane.kind() == VarjoXR::XRObjectKind::Plane);
    VX_REQUIRE_NEAR(plane.size().x, 1.25, kEps);
    VX_REQUIRE_NEAR(plane.size().y, 0.75, kEps);
}

VX_TEST("XRPlane setSize updates width and height") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    plane.setSize({2.5f, 3.5f});
    VX_REQUIRE_NEAR(plane.size().x, 2.5, kEps);
    VX_REQUIRE_NEAR(plane.size().y, 3.5, kEps);
}

VX_TEST("XRPlane allows zero and negative size values as raw configuration") {
    VarjoXR::XRPlane plane({0.0f, -1.0f});
    VX_REQUIRE_NEAR(plane.size().x, 0.0, kEps);
    VX_REQUIRE_NEAR(plane.size().y, -1.0, kEps);
}

VX_TEST("XRObject transform returns mutable reference") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    plane.transform().position = {1.0f, 2.0f, 3.0f};

    const VarjoXR::XRPlane& constPlane = plane;
    VX_REQUIRE_NEAR(constPlane.transform().position.x, 1.0, kEps);
    VX_REQUIRE_NEAR(constPlane.transform().position.y, 2.0, kEps);
    VX_REQUIRE_NEAR(constPlane.transform().position.z, 3.0, kEps);
}

VX_TEST("XRObject default materials are independent for left and right eyes") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    plane.materialFor(VarjoXR::Eye::Left).tint = {1.0f, 0.0f, 0.0f, 1.0f};
    plane.materialFor(VarjoXR::Eye::Right).tint = {0.0f, 0.0f, 1.0f, 1.0f};

    VX_REQUIRE_NEAR(plane.materialFor(VarjoXR::Eye::Left).tint.x, 1.0, kEps);
    VX_REQUIRE_NEAR(plane.materialFor(VarjoXR::Eye::Left).tint.z, 0.0, kEps);
    VX_REQUIRE_NEAR(plane.materialFor(VarjoXR::Eye::Right).tint.x, 0.0, kEps);
    VX_REQUIRE_NEAR(plane.materialFor(VarjoXR::Eye::Right).tint.z, 1.0, kEps);
}

VX_TEST("XRObject setTexture without eye assigns the same texture to both eyes") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    auto texture = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D11, 10, 20);

    plane.setTexture(texture);

    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Left).texture == texture);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Right).texture == texture);
}

VX_TEST("XRObject setTexture for one eye does not touch the other eye") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    auto leftTexture = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D11, 10, 20);
    auto rightTexture = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D11, 30, 40);

    plane.setTexture(VarjoXR::Eye::Right, rightTexture);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Left).texture == nullptr);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Right).texture == rightTexture);

    plane.setTexture(VarjoXR::Eye::Left, leftTexture);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Left).texture == leftTexture);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Right).texture == rightTexture);
}

VX_TEST("XRObject setTexture can overwrite and clear textures") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    auto first = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D11, 1, 1);
    auto second = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D12, 2, 2);

    plane.setTexture(first);
    plane.setTexture(VarjoXR::Eye::Left, second);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Left).texture == second);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Right).texture == first);

    plane.setTexture(nullptr);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Left).texture == nullptr);
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Right).texture == nullptr);
}

VX_TEST("XRObject setPixelShaderHLSL without eye assigns shader source to both eyes") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    const std::string shader = "float4 main(float2 uv : TEXCOORD0) : SV_TARGET { return 1; }";

    plane.setPixelShaderHLSL(shader);

    VX_REQUIRE_EQ(plane.materialFor(VarjoXR::Eye::Left).pixelShaderHlsl, shader);
    VX_REQUIRE_EQ(plane.materialFor(VarjoXR::Eye::Right).pixelShaderHlsl, shader);
}

VX_TEST("XRObject setPixelShaderHLSL for one eye preserves the other eye shader") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    const std::string base = "base";
    const std::string left = "left";
    const std::string right = "right";

    plane.setPixelShaderHLSL(base);
    plane.setPixelShaderHLSL(VarjoXR::Eye::Left, left);
    VX_REQUIRE_EQ(plane.materialFor(VarjoXR::Eye::Left).pixelShaderHlsl, left);
    VX_REQUIRE_EQ(plane.materialFor(VarjoXR::Eye::Right).pixelShaderHlsl, base);

    plane.setPixelShaderHLSL(VarjoXR::Eye::Right, right);
    VX_REQUIRE_EQ(plane.materialFor(VarjoXR::Eye::Left).pixelShaderHlsl, left);
    VX_REQUIRE_EQ(plane.materialFor(VarjoXR::Eye::Right).pixelShaderHlsl, right);
}

VX_TEST("XRObject setPixelShaderHLSL accepts empty source to clear shader override") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    plane.setPixelShaderHLSL("custom");
    plane.setPixelShaderHLSL(VarjoXR::Eye::Right, "");

    VX_REQUIRE_EQ(plane.materialFor(VarjoXR::Eye::Left).pixelShaderHlsl, std::string("custom"));
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Right).pixelShaderHlsl.empty());

    plane.setPixelShaderHLSL("");
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Left).pixelShaderHlsl.empty());
    VX_REQUIRE(plane.materialFor(VarjoXR::Eye::Right).pixelShaderHlsl.empty());
}

VX_TEST("Const materialFor exposes read-only material state") {
    VarjoXR::XRPlane plane({1.0f, 1.0f});
    plane.setPixelShaderHLSL(VarjoXR::Eye::Left, "left shader");
    const VarjoXR::XRPlane& constPlane = plane;

    VX_REQUIRE_EQ(constPlane.materialFor(VarjoXR::Eye::Left).pixelShaderHlsl, std::string("left shader"));
    VX_REQUIRE(constPlane.materialFor(VarjoXR::Eye::Right).pixelShaderHlsl.empty());
}

VX_TEST("XRObject can be used polymorphically through base pointer") {
    std::unique_ptr<VarjoXR::XRObject> object = std::make_unique<VarjoXR::XRPlane>(VarjoXR::Vec2{1.0f, 2.0f});
    object->setPixelShaderHLSL("shader");

    VX_REQUIRE(object->kind() == VarjoXR::XRObjectKind::Plane);
    VX_REQUIRE_EQ(object->materialFor(VarjoXR::Eye::Left).pixelShaderHlsl, std::string("shader"));
    VX_REQUIRE_EQ(object->materialFor(VarjoXR::Eye::Right).pixelShaderHlsl, std::string("shader"));
}

VX_TEST("Multiple planes keep transforms independent") {
    VarjoXR::XRPlane a({1.0f, 1.0f});
    VarjoXR::XRPlane b({2.0f, 2.0f});

    a.transform().position = {1.0f, 0.0f, 0.0f};
    b.transform().position = {0.0f, 2.0f, 0.0f};

    VX_REQUIRE_NEAR(a.transform().position.x, 1.0, kEps);
    VX_REQUIRE_NEAR(a.transform().position.y, 0.0, kEps);
    VX_REQUIRE_NEAR(b.transform().position.x, 0.0, kEps);
    VX_REQUIRE_NEAR(b.transform().position.y, 2.0, kEps);
}

VX_TEST("Multiple planes keep materials independent") {
    VarjoXR::XRPlane a({1.0f, 1.0f});
    VarjoXR::XRPlane b({1.0f, 1.0f});
    auto texA = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D11, 100, 100);
    auto texB = std::make_shared<DummyTexture>(VarjoXR::BackendType::D3D12, 200, 200);

    a.setTexture(texA);
    b.setTexture(texB);
    a.setPixelShaderHLSL("a");
    b.setPixelShaderHLSL("b");

    VX_REQUIRE(a.materialFor(VarjoXR::Eye::Left).texture == texA);
    VX_REQUIRE(b.materialFor(VarjoXR::Eye::Left).texture == texB);
    VX_REQUIRE_EQ(a.materialFor(VarjoXR::Eye::Right).pixelShaderHlsl, std::string("a"));
    VX_REQUIRE_EQ(b.materialFor(VarjoXR::Eye::Right).pixelShaderHlsl, std::string("b"));
}

VX_TEST("XRSpaceConfig default values are stable") {
    const VarjoXR::XRSpaceConfig cfg{};
    VX_REQUIRE(cfg.backend == VarjoXR::BackendType::D3D11);
    VX_REQUIRE_EQ(cfg.swapChainTextureCount, 3);
    VX_REQUIRE_EQ(cfg.layerFlags, static_cast<int64_t>(0));
    VX_REQUIRE_EQ(cfg.colorFormat, static_cast<int64_t>(0));
    VX_REQUIRE(cfg.enableDebug);
}

VX_TEST("XRSpaceConfig can select D3D12 backend and custom swapchain count") {
    VarjoXR::XRSpaceConfig cfg{};
    cfg.backend = VarjoXR::BackendType::D3D12;
    cfg.swapChainTextureCount = 5;
    cfg.layerFlags = 123;
    cfg.colorFormat = 456;
    cfg.enableDebug = false;

    VX_REQUIRE(cfg.backend == VarjoXR::BackendType::D3D12);
    VX_REQUIRE_EQ(cfg.swapChainTextureCount, 5);
    VX_REQUIRE_EQ(cfg.layerFlags, static_cast<int64_t>(123));
    VX_REQUIRE_EQ(cfg.colorFormat, static_cast<int64_t>(456));
    VX_REQUIRE(!cfg.enableDebug);
}

int main() {
    int failed = 0;
    for (const auto& test : Registry()) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << '\n' << "       " << e.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << '\n' << "       unknown exception\n";
        }
    }

    std::cout << "Executed " << Registry().size() << " test cases and " << AssertionCount() << " assertions.\n";
    if (failed != 0) {
        std::cerr << failed << " test case(s) failed.\n";
        return 1;
    }
    return 0;
}
