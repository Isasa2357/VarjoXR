# VarjoXR

VarjoXR は、Varjo Native SDK 上で D3D11 / D3D12 texture を MR 空間上の Plane として表示するための C++17 ライブラリです。

この実装では、Varjo SDK `examples/Common/` には依存しません。Varjo 側は Native SDK / VarjoToolkit の session・frame・layer・swapchain を使い、DirectX 側は D3D11Helper / D3D12Helper を使います。

## 現在の実装範囲

- 外部 `VarjoSession` を使った `XRSpace`
- 外部 D3D11 / D3D12 core を使った backend
- D3D11 backend の Plane 描画
- D3D12 backend の Plane 描画
- D3D12 frame resource ring / fence による `WaitIdle()` 回避
- `XRSpace` / `XRPlane` / `XRMaterial` / `XRTexture`
- World / HeadRelative placement
- 左右 eye 別 texture / final pixel shader HLSL / processing HLSL
- CPU RGBA からの texture 作成
- D3D11 `ID3D11Texture2D` / `ID3D11ShaderResourceView` wrapper
- D3D12 `ID3D12Resource` wrapper
- programmable texture processing prepass
- D3D11 / D3D12 executable output への `dxcompiler.dll` / `dxil.dll` 自動コピー
- Varjo Runtime なしで実行可能な core unit tests
- backend compile/smoke tests
- D3D11 / D3D12 shader linkage + offscreen UV propagation tests
- RenderingPlane samples: 01〜08

## 依存関係

- Windows 10 / 11
- Visual Studio 18 2026
- CMake 4.2 以降
- C++17
- Varjo Native SDK
- VarjoToolkit
- D3D11Helper
- D3D12Helper
- glm
- DirectX Shader Compiler runtime: `dxcompiler.dll`, usually with `dxil.dll`

使用しないもの:

- Varjo SDK `examples/Common/`
- Varjo sample 内の `Session`, `MultiLayerView`, `D3D11Renderer`, `Scene`

## ビルドオプション

| Option | Default | Description |
|---|---:|---|
| `VARJOXR_BUILD_RUNTIME` | `ON` | Varjo Native SDK / backend を含む `VarjoXR` target をビルドする |
| `VARJOXR_BUILD_SAMPLES` | `ON` | samples をビルドする |
| `VARJOXR_BUILD_TESTS` | `ON` | CTest 用の core tests をビルドする |
| `VARJOXR_ENABLE_D3D11` | `ON` | D3D11 backend をビルドする |
| `VARJOXR_ENABLE_D3D12` | `ON` | D3D12 backend をビルドする |
| `VARJOXR_COPY_DXC_RUNTIME` | `ON` | `dxcompiler.dll` / `dxil.dll` を helper 使用 executable の出力先へコピーする |
| `VARJOXR_DXC_RUNTIME_DIR` | empty | `dxcompiler.dll` / `dxil.dll` があるディレクトリ。自動検出できない場合に指定する |
| `D3D12HELPER_DXC_RUNTIME_DIR` | empty | D3D12Helper 側の DXC runtime 探索にも使える互換指定 |
| `VARJOXR_VARJO_INCLUDE_DIR` | empty | `Varjo.h` などがある include directory |
| `VARJOXR_VARJO_LIBRARY` | empty | `VarjoLib.lib` のパス |
| `VARJOXR_VARJO_RUNTIME_DIR` | empty | `VarjoLib.dll` などがある runtime directory |

`VARJOXR_DXC_RUNTIME_DIR` を指定しない場合、CMake は次の場所から `dxcompiler.dll` / `dxil.dll` を探します。

```text
- VARJOXR_DXC_RUNTIME_DIR
- D3D12HELPER_DXC_RUNTIME_DIR
- D3D12Helper が検出済みの DXC runtime directory
- repo/packages/Microsoft.Direct3D.DXC*/...
- build/packages/Microsoft.Direct3D.DXC*/...
- %USERPROFILE%/.nuget/packages/microsoft.direct3d.dxc/...
- Windows SDK bin directory
- PATH
```

D3D11Helper / D3D12Helper が DXC API をリンクしている場合、D3D11 単体 executable でも `dxcompiler.dll` がプロセス起動時に必要になることがあります。そのため D3D11 sample に対しても DXC runtime copy を行います。

## 最小使用例: D3D11

```cpp
#include <VarjoXR/VarjoXR.hpp>

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>

#include <array>
#include <cstdint>
#include <memory>

int main() {
    auto session = std::make_shared<VarjoSession>();
    if (!session->valid()) {
        session->initialize();
    }

    auto d3d = D3D11CoreLib::D3D11Core::CreateShared();
    auto backend = VarjoXR::Backends::D3D11::CreateBackend(d3d);
    VarjoXR::XRSpace space({session, std::move(backend)});

    auto& plane = space.createPlane({1.0f, 0.6f});
    plane.setPlacementMode(VarjoXR::PlacementMode::World);
    plane.transform().position = {0.0f, 0.0f, -1.0f};

    const std::array<std::uint8_t, 4> white = {255, 255, 255, 255};
    auto texture = static_cast<VarjoXR::Backends::D3D11::D3D11Backend&>(space.backend())
        .createTextureFromRGBA(white.data(), 1, 1, 4);
    plane.setTexture(texture);

    while (true) {
        space.update();
    }
}
```

## 最小使用例: D3D12

```cpp
#include <VarjoXR/VarjoXR.hpp>

#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>

#include <array>
#include <cstdint>
#include <memory>

int main() {
    auto session = std::make_shared<VarjoSession>();
    if (!session->valid()) {
        session->initialize();
    }

    D3D12CoreLib::D3D12CoreConfig config{};
    config.createDirectQueue = true;
    config.createCopyQueue = true;
    auto d3d = D3D12CoreLib::D3D12Core::CreateShared(config);

    auto backend = VarjoXR::Backends::D3D12::CreateBackend(d3d);
    VarjoXR::XRSpace space({session, std::move(backend)});

    auto& plane = space.createPlane({1.0f, 0.6f});
    plane.setPlacementMode(VarjoXR::PlacementMode::World);
    plane.transform().position = {0.0f, 0.0f, -1.0f};

    const std::array<std::uint8_t, 4> white = {255, 255, 255, 255};
    auto texture = static_cast<VarjoXR::Backends::D3D12::D3D12Backend&>(space.backend())
        .createTextureFromRGBA(white.data(), 1, 1, 4);
    plane.setTexture(texture);

    while (true) {
        space.update();
    }
}
```

## Native resource を Plane に貼る

D3D11:

```cpp
// ID3D11Texture2D* cameraTexture = ...;
auto tex = static_cast<VarjoXR::Backends::D3D11::D3D11Backend&>(space.backend())
    .wrapTexture(cameraTexture, DXGI_FORMAT_R8G8B8A8_UNORM);
plane.setTexture(VarjoXR::Eye::Left, tex);

// ID3D11ShaderResourceView* srv = ...;
auto srvTex = static_cast<VarjoXR::Backends::D3D11::D3D11Backend&>(space.backend())
    .wrapSrv(srv, width, height);
plane.setTexture(VarjoXR::Eye::Right, srvTex);
```

D3D12:

```cpp
// ID3D12Resource* cameraTexture = ...;
auto tex = static_cast<VarjoXR::Backends::D3D12::D3D12Backend&>(space.backend())
    .wrapResource(cameraTexture, DXGI_FORMAT_R8G8B8A8_UNORM);
plane.setTexture(tex);
```

## Placement

`XRPlane` は `World` と `HeadRelative` の2種類の配置をサポートします。

```cpp
plane.setPlacementMode(VarjoXR::PlacementMode::World);
plane.transform().position = {0.0f, 0.0f, -1.2f};

plane.setPlacementMode(VarjoXR::PlacementMode::HeadRelative);
plane.transform().position = {0.0f, -0.05f, -0.9f};
```

## 左右 eye 別 Material

同じ Plane 形状・同じ Transform に対して、左右 eye 別に texture / HLSL / processing を設定できます。

```cpp
plane.setTexture(VarjoXR::Eye::Left, leftTexture);
plane.setTexture(VarjoXR::Eye::Right, rightTexture);

plane.setPixelShaderHLSL(VarjoXR::Eye::Left, leftPixelShader);
plane.setPixelShaderHLSL(VarjoXR::Eye::Right, rightPixelShader);

plane.setProcessing(VarjoXR::Eye::Left, leftProcessing);
plane.setProcessing(VarjoXR::Eye::Right, rightProcessing);
```

## Final pixel shader

`setPixelShaderHLSL()` は、Plane を Varjo swapchain へ描画する最後の pixel shader を差し替える advanced API です。

渡す HLSL には `main(float2 uv : TEXCOORD0) : SV_TARGET` だけを書けます。D3D11 / D3D12 backend は必要に応じて VS/PS linkage 互換 wrapper を挿入します。次の宣言は VarjoXR 側が前置します。

```hlsl
Texture2D xrTexture : register(t0);
SamplerState xrSampler : register(s0);
cbuffer PlaneConstants : register(b0) {
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 tint;
    float4 params0;
    float4 params1;
    float4 frameParams;
};
```

例:

```hlsl
float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 c = xrTexture.Sample(xrSampler, uv);
    float2 p = uv * 2.0f - 1.0f;
    float vignette = saturate(1.0f - dot(p, p) * 0.55f);
    c.rgb *= vignette;
    return c * tint;
}
```

## Programmable texture processing

画像処理は、final pixel shader ではなく `TextureProcessingDesc` による texture -> texture compute prepass として行います。

```text
source texture
  -> compute HLSL prepass
  -> processed texture
  -> Plane final pixel shader
  -> Varjo swapchain
```

binding 規約:

```text
t0: input texture SRV
u0: output texture UAV
b0: user-defined constant buffer bytes
b1: VarjoXR frame/texture constants
```

C++ 側:

```cpp
struct MyConstants {
    float centerX;
    float centerY;
    float radius;
    float outsideBrightness;
};

MyConstants constants{0.5f, 0.5f, 0.28f, 0.45f};

VarjoXR::TextureProcessingDesc processing{};
processing.enabled = true;
processing.timing = VarjoXR::ProcessingTiming::BeforeRenderEachFrame;
processing.hlsl = myComputeShaderSource;
processing.entryPoint = "main";
processing.target = "cs_5_0";
processing.outputSize = {1280, 720};
processing.userConstants.registerIndex = 0;
processing.userConstants.set(constants);
processing.frameConstants.enabled = true;
processing.frameConstants.registerIndex = 1;

plane.setProcessing(processing);
```

`hlsl/VarjoXR/TextureProcessing.hlsli` には、`xrInput` / `xrOutput` / `XRTextureProcessingFrameConstants` と補助関数をまとめています。ユーザー定数 `b0` は任意構造体にするため、この `.hlsli` には含めていません。

## Samples

`samples/RenderingPlane` は設計書に合わせて 01〜08 に分割されています。

| Target | 内容 |
|---|---|
| `RenderingPlane_01_SinglePlane_D3D11` / `D3D12` | World 配置の単一 Plane |
| `RenderingPlane_02_HeadRelativePlane_D3D11` / `D3D12` | HeadRelative 配置の Plane |
| `RenderingPlane_03_StereoPlane_D3D11` / `D3D12` | 左右 eye 別 texture |
| `RenderingPlane_04_ShaderPlane_D3D11` / `D3D12` | final pixel shader 差し替え |
| `RenderingPlane_05_MultiplePlanes_D3D11` / `D3D12` | 複数 Plane 同時表示 |
| `RenderingPlane_06_ProcessingPlane_D3D11` / `D3D12` | programmable texture processing |
| `RenderingPlane_07_EyeMaterialVariants_D3D11` / `D3D12` | 左右 eye 別 texture / shader / processing |
| `RenderingPlane_08_NativeTexturePlane_D3D11` / `D3D12` | 外部 native D3D texture/resource wrapper |

追加の processing 専用 sample:

| Target | 内容 |
|---|---|
| `ProgrammableProcessing_D3D11` | processing constants を毎 frame 更新する D3D11 sample |
| `ProgrammableProcessing_D3D12` | processing constants を毎 frame 更新する D3D12 sample |

## Build examples

D3D11:

```bat
git fetch --prune origin
git checkout rewrite/v0.1
git pull --ff-only origin rewrite/v0.1

cmake -S . -B out/build/rewrite-v01-d3d11 -G "Visual Studio 18 2026" -A x64 ^
  -DVARJOXR_ENABLE_D3D11=ON ^
  -DVARJOXR_ENABLE_D3D12=OFF ^
  -DVARJOXR_BUILD_SAMPLES=ON ^
  -DVARJOXR_BUILD_TESTS=ON ^
  -DVARJOXR_DXC_RUNTIME_DIR="C:\path\to\directory\containing\dxcompiler.dll"

cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_01_SinglePlane_D3D11
cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_07_EyeMaterialVariants_D3D11
cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_08_NativeTexturePlane_D3D11
```

D3D12:

```bat
git fetch --prune origin
git checkout rewrite/v0.1
git pull --ff-only origin rewrite/v0.1

cmake -S . -B out/build/rewrite-v01-d3d12 -G "Visual Studio 18 2026" -A x64 ^
  -DVARJOXR_ENABLE_D3D11=OFF ^
  -DVARJOXR_ENABLE_D3D12=ON ^
  -DVARJOXR_BUILD_SAMPLES=ON ^
  -DVARJOXR_BUILD_TESTS=ON ^
  -DVARJOXR_DXC_RUNTIME_DIR="C:\path\to\directory\containing\dxcompiler.dll"

cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_01_SinglePlane_D3D12
cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_07_EyeMaterialVariants_D3D12
cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_08_NativeTexturePlane_D3D12
```

## Tests

Core tests and backend compile/smoke tests can run without HMD. `VarjoXRBackendSmokeTests` validates backend types and default backend descriptors without creating a Varjo session or a D3D device.

`VarjoXRShaderLinkageTests` creates D3D11 / D3D12 devices without HMD, compiles the legacy `main(float2 uv : TEXCOORD0)` pixel shader form, creates a graphics pipeline, draws a 4x4 offscreen render target, reads it back, and verifies that UV-dependent red/green values vary across the image. This catches VS/PS linkage and UV propagation regressions that compile-only tests can miss.

When `VARJOXR_BUILD_SAMPLES=ON`, CTest also registers `Build_<target>` tests for the RenderingPlane and ProgrammableProcessing sample targets. These tests invoke `cmake --build` for each sample target and are marked `RUN_SERIAL` to avoid concurrent builds in the same build directory.

```bat
cmake --build out/build/rewrite-v01-d3d11 --config Debug
ctest --test-dir out/build/rewrite-v01-d3d11 -C Debug --output-on-failure
```

HMD / Varjo Runtime / actual presentation integration tests are still not automated. Runtime behavior is verified through samples on a machine with Varjo Runtime and HMD.

## 設計上まだ未実装のもの

- `XRFrame` public class
- `XRScene` public class
- `XRObject` base class
- Circle / Cube / Sphere / custom mesh
- depth buffer / alpha sort / occlusion
- HMD presentation integration tests
