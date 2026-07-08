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
- D3D12 executable outputへの `dxcompiler.dll` / `dxil.dll` 自動コピー
- Varjo Runtimeなしで実行可能な core unit tests
- backend compile/smoke tests
- RenderingPlane samples: 01〜06

## 依存関係

- Windows 10 / 11
- Visual Studio 2022
- CMake
- C++17
- Varjo Native SDK
- VarjoToolkit
- D3D11Helper
- D3D12Helper
- glm
- DirectX Shader Compiler runtime: `dxcompiler.dll`, usually with `dxil.dll`

使用しないもの:

- Varjo SDK `examples/Common/`
- Varjo sample内の `Session`, `MultiLayerView`, `D3D11Renderer`, `Scene`

## ビルドオプション

| Option | Default | Description |
|---|---:|---|
| `VARJOXR_BUILD_RUNTIME` | `ON` | Varjo Native SDK / backend を含む `VarjoXR` target をビルドする |
| `VARJOXR_BUILD_SAMPLES` | `ON` | samples をビルドする |
| `VARJOXR_BUILD_TESTS` | `ON` | CTest用のcore testsをビルドする |
| `VARJOXR_ENABLE_D3D11` | `ON` | D3D11 backendをビルドする |
| `VARJOXR_ENABLE_D3D12` | `ON` | D3D12 backendをビルドする |
| `VARJOXR_COPY_DXC_RUNTIME` | `ON` | D3D12 executableの出力先へ `dxcompiler.dll` / `dxil.dll` をコピーする |
| `VARJOXR_DXC_RUNTIME_DIR` | empty | `dxcompiler.dll` / `dxil.dll` があるディレクトリ。自動検出できない場合に指定する |
| `D3D12HELPER_DXC_RUNTIME_DIR` | empty | D3D12Helper側のDXC runtime探索にも使える互換指定 |
| `VARJOXR_VARJO_INCLUDE_DIR` | empty | `Varjo.h` などがあるinclude directory |
| `VARJOXR_VARJO_LIBRARY` | empty | `VarjoLib.lib` のパス |

`VARJOXR_DXC_RUNTIME_DIR` を指定しない場合、CMakeは次の場所から `dxcompiler.dll` / `dxil.dll` を探します。

```text
- VARJOXR_DXC_RUNTIME_DIR
- D3D12HELPER_DXC_RUNTIME_DIR
- D3D12Helperが検出済みのDXC runtime directory
- repo/packages/Microsoft.Direct3D.DXC*/...
- build/packages/Microsoft.Direct3D.DXC*/...
- %USERPROFILE%/.nuget/packages/microsoft.direct3d.dxc/...
- Windows SDK bin directory
```

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

## 左右eye別Material

同じPlane形状・同じTransformに対して、左右eye別にtexture / HLSL / processingを設定できます。

```cpp
plane.setTexture(VarjoXR::Eye::Left, leftTexture);
plane.setTexture(VarjoXR::Eye::Right, rightTexture);

plane.setPixelShaderHLSL(VarjoXR::Eye::Left, leftPixelShader);
plane.setPixelShaderHLSL(VarjoXR::Eye::Right, rightPixelShader);

plane.setProcessing(VarjoXR::Eye::Left, leftProcessing);
plane.setProcessing(VarjoXR::Eye::Right, rightProcessing);
```

## Final pixel shader

`setPixelShaderHLSL()` は、PlaneをVarjo swapchainへ描画する最後のpixel shaderを差し替える advanced API です。

渡すHLSLには `main(float2 uv : TEXCOORD0) : SV_TARGET` だけを書きます。次の宣言はVarjoXR側が前置します。

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

画像処理は、final pixel shaderではなく `TextureProcessingDesc` による texture -> texture compute prepass として行います。

```text
source texture
  -> compute HLSL prepass
  -> processed texture
  -> Plane final pixel shader
  -> Varjo swapchain
```

binding規約:

```text
t0: input texture SRV
u0: output texture UAV
b0: user-defined constant buffer bytes
b1: VarjoXR frame/texture constants
```

C++側:

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

HLSL側:

```hlsl
Texture2D<float4> xrInput : register(t0);
RWTexture2D<float4> xrOutput : register(u0);

cbuffer MyConstants : register(b0)
{
    float centerX;
    float centerY;
    float radius;
    float outsideBrightness;
};

cbuffer XRTextureProcessingFrameConstants : register(b1)
{
    uint srcWidth;
    uint srcHeight;
    uint dstWidth;
    uint dstHeight;
    float4 frameParams;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= dstWidth || id.y >= dstHeight) return;
    float2 uv = (float2(id.xy) + 0.5f) / float2(dstWidth, dstHeight);
    xrOutput[id.xy] = xrInput.Load(int3(uint2(uv * float2(srcWidth, srcHeight)), 0));
}
```

`hlsl/VarjoXR/TextureProcessing.hlsli` には、`xrInput` / `xrOutput` / `XRTextureProcessingFrameConstants` と補助関数をまとめています。ユーザー定数 `b0` は任意構造体にするため、この `.hlsli` には含めていません。

## Samples

`samples/RenderingPlane` は設計書に合わせて 01〜06 に分割されています。

| Target | 内容 |
|---|---|
| `RenderingPlane_01_SinglePlane_D3D11` / `D3D12` | World配置の単一Plane |
| `RenderingPlane_02_HeadRelativePlane_D3D11` / `D3D12` | HeadRelative配置のPlane |
| `RenderingPlane_03_StereoPlane_D3D11` / `D3D12` | 左右eye別texture |
| `RenderingPlane_04_ShaderPlane_D3D11` / `D3D12` | final pixel shader差し替え |
| `RenderingPlane_05_MultiplePlanes_D3D11` / `D3D12` | 複数Plane同時表示 |
| `RenderingPlane_06_ProcessingPlane_D3D11` / `D3D12` | programmable texture processing |

追加のprocessing専用sample:

| Target | 内容 |
|---|---|
| `ProgrammableProcessing_D3D11` | processing constantsを毎frame更新するD3D11 sample |
| `ProgrammableProcessing_D3D12` | processing constantsを毎frame更新するD3D12 sample |

## Build examples

D3D11:

```bat
git fetch --prune origin
git checkout rewrite/v0.1
git pull --ff-only origin rewrite/v0.1

cmake -S . -B out/build/rewrite-v01-d3d11 -G "Visual Studio 17 2022" -A x64 ^
  -DVARJOXR_ENABLE_D3D11=ON ^
  -DVARJOXR_ENABLE_D3D12=OFF ^
  -DVARJOXR_BUILD_SAMPLES=ON ^
  -DVARJOXR_BUILD_TESTS=ON

cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_01_SinglePlane_D3D11
cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_02_HeadRelativePlane_D3D11
cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_03_StereoPlane_D3D11
cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_04_ShaderPlane_D3D11
cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_05_MultiplePlanes_D3D11
cmake --build out/build/rewrite-v01-d3d11 --config Debug --target RenderingPlane_06_ProcessingPlane_D3D11
```

D3D12:

```bat
git fetch --prune origin
git checkout rewrite/v0.1
git pull --ff-only origin rewrite/v0.1

cmake -S . -B out/build/rewrite-v01-d3d12 -G "Visual Studio 17 2022" -A x64 ^
  -DVARJOXR_ENABLE_D3D11=OFF ^
  -DVARJOXR_ENABLE_D3D12=ON ^
  -DVARJOXR_BUILD_SAMPLES=ON ^
  -DVARJOXR_BUILD_TESTS=ON

cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_01_SinglePlane_D3D12
cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_02_HeadRelativePlane_D3D12
cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_03_StereoPlane_D3D12
cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_04_ShaderPlane_D3D12
cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_05_MultiplePlanes_D3D12
cmake --build out/build/rewrite-v01-d3d12 --config Debug --target RenderingPlane_06_ProcessingPlane_D3D12
```

When CMake cannot find `dxcompiler.dll` automatically, add:

```bat
-DVARJOXR_DXC_RUNTIME_DIR="C:\path\to\directory\containing\dxcompiler.dll"
```

## Tests

Core tests and backend compile/smoke tests can run without HMD. `VarjoXRBackendSmokeTests` validates backend types and default backend descriptors without creating a Varjo session or a D3D device.

When `VARJOXR_BUILD_SAMPLES=ON`, CTest also registers `Build_<target>` tests for the RenderingPlane and ProgrammableProcessing sample targets. These tests invoke `cmake --build` for each sample target and are marked `RUN_SERIAL` to avoid concurrent builds in the same build directory.

```bat
git fetch --prune origin
git checkout rewrite/v0.1
git pull --ff-only origin rewrite/v0.1

cmake --build out/build/rewrite-v01-d3d11 --config Debug
ctest --test-dir out/build/rewrite-v01-d3d11 -C Debug --output-on-failure
```

To run only backend smoke tests:

```bat
ctest --test-dir out/build/rewrite-v01-d3d11 -C Debug -R "VarjoXRBackendSmokeTests|Build_RenderingPlane" --output-on-failure
ctest --test-dir out/build/rewrite-v01-d3d12 -C Debug -R "VarjoXRBackendSmokeTests|Build_RenderingPlane" --output-on-failure
```

HMD / Varjo Runtime / actual presentation integration tests are still not automated. Runtime behavior is currently verified through samples on a machine with Varjo Runtime and HMD.

## 設計上まだ未実装のもの

- `XRFrame` public class
- `XRScene` public class
- `XRObject` base class
- Circle / Cube / Sphere / custom mesh
- depth buffer / alpha sort / occlusion
- HMD presentation integration tests
