# VarjoXR

VarjoXR は、Varjo Native SDK 上に、`XRSpace` に `XRPlane` などのオブジェクトを追加して `update()` するだけで XR/MR 表示を行えるようにする C++17 ライブラリです。

この実装では、Varjo 公式サンプルの `examples/Common/` には依存しません。Varjo 側は Native SDK の C API のみを使い、DirectX 側は自作ライブラリの D3D11Helper / D3D12Helper を積極的に使います。

## 現在の実装範囲

- Varjo Native SDK の session / frame / layer / swapchain を独自管理
- D3D11 backend
- D3D12 backend
- `XRSpace` / `XRPlane` / `XRObject` / `Material`
- 左右 eye 別 texture / pixel shader HLSL
- CPU RGBA からの texture 作成
- Varjo view index から `Eye::Left` / `Eye::Right` への解決
- Context / Focus view を texture array slice として submit

## 依存関係

- Windows 10 / 11
- Visual Studio 2019 以降
- CMake 3.20 以降
- C++17
- Varjo Native SDK
- D3D11Helper
- D3D12Helper

使用しないもの:

- Varjo SDK `examples/Common/`
- Varjo サンプル内の `Session`, `MultiLayerView`, `D3D11Renderer`, `Scene`

## ディレクトリ構成

```text
VarjoXR/
├── CMakeLists.txt
├── include/VarjoXR/
│   ├── XRSpace.hpp
│   ├── XRPlane.hpp
│   ├── XRObject.hpp
│   ├── Material.hpp
│   ├── Texture.hpp
│   ├── VarjoSession.hpp
│   ├── Detail/
│   │   ├── IRenderBackend.hpp
│   │   └── VarjoLayerSubmit.hpp
│   └── Backends/
│       ├── D3D11/D3D11Backend.hpp
│       └── D3D12/D3D12Backend.hpp
├── src/
│   ├── XRSpace.cpp
│   ├── XRObject.cpp
│   ├── XRPlane.cpp
│   ├── Transform.cpp
│   ├── VarjoSession.cpp
│   ├── Detail/VarjoLayerSubmit.cpp
│   └── Backends/
│       ├── D3D11/D3D11Backend.cpp
│       └── D3D12/D3D12Backend.cpp
└── doc/BackendReplacement.md
```

## 最小使用例

```cpp
#include <VarjoXR/VarjoXR.hpp>

int main() {
    VarjoXR::XRSpaceConfig config{};
    config.backend = VarjoXR::BackendType::D3D11; // or D3D12

    VarjoXR::XRSpace space(config);
    auto* plane = space.createPlane({1.0f, 0.6f});
    plane->transform().position = {0.0f, 0.0f, -1.0f};

    while (true) {
        space.update();
    }
}
```

## カスタム pixel shader

`setPixelShaderHLSL()` には、次の関数だけを含む HLSL を渡します。

```hlsl
float4 main(float2 uv : TEXCOORD0) : SV_TARGET {
    return xrTexture.Sample(xrSampler, uv);
}
```

以下は VarjoXR 側で宣言済みです。

```hlsl
Texture2D xrTexture : register(t0);
SamplerState xrSampler : register(s0);
cbuffer PlaneConstants : register(b0) {
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 tint;
};
```
