#include <VarjoXR/VarjoXR.hpp>

#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {

int g_assertions = 0;

void require(bool condition, const char* message) {
    ++g_assertions;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testCommonBackendApi() {
    static_assert(std::has_virtual_destructor_v<VarjoXR::IRenderBackend>,
                  "IRenderBackend must be safely deletable through its base pointer.");
    require(true, "common backend API smoke");
}

#if defined(VARJOXR_ENABLE_D3D11)
void testD3D11BackendTypes() {
    static_assert(std::is_base_of_v<VarjoXR::IRenderBackend, VarjoXR::Backends::D3D11::D3D11Backend>,
                  "D3D11Backend must implement IRenderBackend.");
    static_assert(std::is_base_of_v<VarjoXR::XRTexture, VarjoXR::Backends::D3D11::D3D11Texture>,
                  "D3D11Texture must derive from XRTexture.");

    VarjoXR::Backends::D3D11::D3D11BackendDesc desc{};
    require(desc.swapchainTextureCount >= 2, "D3D11 swapchainTextureCount default should be at least 2");
    require(desc.varjoTextureFormat == 0, "D3D11 default varjoTextureFormat should be automatic");
    require(desc.enableAlphaBlend, "D3D11 alpha blend should be enabled by default");
}
#endif

#if defined(VARJOXR_ENABLE_D3D12)
void testD3D12BackendTypes() {
    static_assert(std::is_base_of_v<VarjoXR::IRenderBackend, VarjoXR::Backends::D3D12::D3D12Backend>,
                  "D3D12Backend must implement IRenderBackend.");
    static_assert(std::is_base_of_v<VarjoXR::XRTexture, VarjoXR::Backends::D3D12::D3D12Texture>,
                  "D3D12Texture must derive from XRTexture.");

    VarjoXR::Backends::D3D12::D3D12BackendDesc desc{};
    require(desc.swapchainTextureCount >= 2, "D3D12 swapchainTextureCount default should be at least 2");
    require(desc.varjoTextureFormat == 0, "D3D12 default varjoTextureFormat should be automatic");
    require(desc.enableAlphaBlend, "D3D12 alpha blend should be enabled by default");
    require(desc.cbvSrvUavDescriptorCount >= 64, "D3D12 descriptor heap should have a practical default size");
    require(desc.rtvDescriptorCount >= 8, "D3D12 RTV descriptor heap should have a practical default size");
    require(desc.frameResourceCount >= 2, "D3D12 frameResourceCount default should permit non-blocking frame submission");
}
#endif

} // namespace

int main() {
    try {
        testCommonBackendApi();
#if defined(VARJOXR_ENABLE_D3D11)
        testD3D11BackendTypes();
#else
        std::cout << "[INFO] VARJOXR_ENABLE_D3D11 is disabled; skipping D3D11 smoke.\n";
#endif

#if defined(VARJOXR_ENABLE_D3D12)
        testD3D12BackendTypes();
#else
        std::cout << "[INFO] VARJOXR_ENABLE_D3D12 is disabled; skipping D3D12 smoke.\n";
#endif
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return 1;
    }

    std::cout << "[PASS] VarjoXRBackendSmokeTests, assertions=" << g_assertions << '\n';
    return 0;
}
