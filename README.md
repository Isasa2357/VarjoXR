# VarjoXR

VarjoXR は、Varjo Native SDK 上に、`XRSpace` に `XRPlane` などのオブジェクトを追加して `update()` するだけで XR/MR 表示を行えるようにする C++17 ライブラリです。

この実装では、Varjo 公式サンプルの `examples/Common/` には依存しません。Varjo 側は Native SDK の C API のみを使い、DirectX 側は自作ライブラリの D3D11Helper / D3D12Helper を積極的に使います。

## 現在の実装範囲

- Varjo Native SDK の session / frame / layer / swapchain を独自管理
- D3D11 backend の Plane 描画
- D3D12 backend の Plane 描画
- `XRSpace` / `XRPlane` / `XRObject` / `Material`
- 左右 eye 別 texture / pixel shader HLSL
- CPU RGBA からの texture 作成API
- D3D11 native texture / SRV wrapper
- D3D12 native resource wrapper
- Varjo view index から `Eye::Left` / `Eye::Right` への解決
- Context / Focus view を texture array slice として submit
- Varjo Runtimeなしで実行可能なcore unit tests

## 依存関係

- Windows 10 / 11
- Visual Studio 2026 / Visual Studio 18 以降
- CMake 4.2 以降
  - `Visual Studio 18 2026` generator は CMake 4.2 以降が必要
- C++17
- Varjo Native SDK
- D3D11Helper
- D3D12Helper

使用しないもの:

- Varjo SDK `examples/Common/`
- Varjo サンプル内の `Session`, `MultiLayerView`, `D3D11Renderer`, `Scene`

## ビルドオプション

| Option | Default | Description |
|---|---:|---|
| `VARJOXR_BUILD_RUNTIME` | `ON` | Varjo Native SDK / backend を含む `VarjoXR` target をビルドする |
| `VARJOXR_BUILD_TESTS` | `ON` | CTest用のテストをビルドする |
| `VARJOXR_ENABLE_D3D11` | `ON` | D3D11 backendをビルドする |
| `VARJOXR_ENABLE_D3D12` | `ON` | D3D12 backendをビルドする |
| `VARJOXR_ENABLE_COVERAGE` | `OFF` | coverage計測用の設定・targetを有効化する |
| `VARJOXR_VARJO_INCLUDE_DIR` | empty | `Varjo.h` などがあるinclude directory |
| `VARJOXR_VARJO_LIBRARY` | empty | `VarjoLib.lib` のパス |

## 最小使用例

```cpp
#include <VarjoXR/VarjoXR.hpp>

#include <array>
#include <cstdint>

int main() {
    VarjoXR::XRSpaceConfig config{};
    config.backend = VarjoXR::BackendType::D3D11; // or D3D12

    VarjoXR::XRSpace space(config);
    auto* plane = space.createPlane({1.0f, 0.6f});
    plane->transform().position = {0.0f, 0.0f, -1.0f};

    const std::array<uint8_t, 4> white = {255, 255, 255, 255};
    auto texture = space.createTextureFromRGBA(white.data(), 1, 1, 4);
    plane->setTexture(texture);

    while (true) {
        space.update();
    }
}
```

## native resource を Plane に貼る

D3D11 では `ID3D11Texture2D*` または `ID3D11ShaderResourceView*` を `XRTexture` としてwrapできます。

```cpp
// ID3D11Texture2D* cameraTexture = ...;
auto tex = space.createTextureFromD3D11Resource(cameraTexture);
plane->setTexture(VarjoXR::Eye::Left, tex);

// 既にSRVを持っている場合。
// ID3D11ShaderResourceView* srv = ...;
auto srvTex = space.createTextureFromD3D11Srv(srv, width, height);
plane->setTexture(VarjoXR::Eye::Right, srvTex);
```

D3D12 では `ID3D12Resource*` をwrapできます。SRV descriptor は VarjoXR の内部 heap に作成されます。

```cpp
// ID3D12Resource* cameraTexture = ...;
auto tex = space.createTextureFromD3D12Resource(cameraTexture);
plane->setTexture(tex);
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

## runtime build

D3D11 backendのみをビルドする場合:

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake --preset vs18-x64-runtime-d3d11
cmake --build --preset vs18-x64-runtime-d3d11-debug
```

D3D12 backendのみをビルドする場合:

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake --preset vs18-x64-runtime-d3d12
cmake --build --preset vs18-x64-runtime-d3d12-debug
```

## テスト

Varjo Runtime、HMD、D3D11Helper、D3D12Helperなしでcore testsだけを実行できます。

推奨: GitHub同期込みで CMake preset を使う方法。既存cloneのリポジトリルートで実行してください。

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake --preset vs18-x64-tests
cmake --build --preset vs18-x64-tests-debug
ctest --preset vs18-x64-tests-debug
```

presetを使わない場合:

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake -S . -B out/build/vs18-x64-tests -G "Visual Studio 18 2026" -A x64 -DVARJOXR_BUILD_RUNTIME=OFF -DVARJOXR_BUILD_TESTS=ON
cmake --build out/build/vs18-x64-tests --config Debug
ctest --test-dir out/build/vs18-x64-tests -C Debug --output-on-failure
```

現在のcore testsは、`Eye`、`Math`、`Transform`、`Texture`、`Material`、`XRObject`、`XRPlane`、`XRSpaceConfig` を対象にしています。

## カバレッジ

Windows / MSVC では OpenCppCoverage を使う想定です。OpenCppCoverage が PATH に無い場合、`VarjoXRCoverage` target はcoverageレポートを出さずに通常テストだけを実行します。

推奨: GitHub同期込みで CMake preset を使う方法。既存cloneのリポジトリルートで実行してください。

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake --preset vs18-x64-coverage
cmake --build --preset vs18-x64-coverage-debug
```

OpenCppCoverageが無い環境で、coverageではなく通常テストだけを確実に回す場合:

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake --preset vs18-x64-tests
cmake --build --preset vs18-x64-tests-debug
ctest --preset vs18-x64-tests-debug
```

presetを使わない場合:

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake -S . -B out/build/vs18-x64-coverage -G "Visual Studio 18 2026" -A x64 -DVARJOXR_BUILD_RUNTIME=OFF -DVARJOXR_BUILD_TESTS=ON -DVARJOXR_ENABLE_COVERAGE=ON
cmake --build out/build/vs18-x64-coverage --config Debug --target VarjoXRCoverage
```

目標:

- core層: 90%以上
- runtime/backend層: backend実装後にfake adapter / integration testsを追加して90%以上を目指す

詳細は [`doc/TestPlan.md`](doc/TestPlan.md) を参照してください。
