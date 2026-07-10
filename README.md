# VarjoXR

VarjoXRは、Varjo Native SDK上でD3D11 / D3D12 textureをMR空間のPlaneとして表示するC++17ライブラリです。

現在のバージョンは **0.2.0** です。

## 0.2.0の同期モデル

D3D11/D3D12レンダリングバックエンドが、アプリケーション内で唯一の`varjo_WaitSync`所有者になります。

```text
XRSpace::update()
    backend::beginFrame()
        varjo_WaitSync 1回
        VarjoFrameInfoSnapshot保存
    render
    endFrame
```

保存されたFrameInfoは次のgetterで取得できます。

```cpp
VarjoFrameInfoSnapshot XRSpace::frameInfoSnapshot() const;
VarjoFrameInfoSnapshot IRenderBackend::frameInfoSnapshot() const;
```

getterは保存済みsnapshotを返すだけで、新しい`varjo_WaitSync`や`varjo_FrameGetPose`を実行しません。

```cpp
space.update();

const auto frameInfo = space.frameInfoSnapshot();
eyeTrackingService.submitFrameInfo(frameInfo);
imuService.submitFrameInfo(frameInfo);
```

これによりレンダリング、Eye Trackingの投影、IMU/head-poseログが同じ`frameNumber`、`displayTime`、Center Poseを共有できます。

詳細は[`docs/FrameInfoSnapshot.md`](docs/FrameInfoSnapshot.md)を参照してください。

## 実装範囲

- 外部`VarjoSession`を使う`XRSpace`
- 外部D3D11 / D3D12 coreを使うbackend
- D3D11 / D3D12 Plane描画
- D3D12 frame-resource ring / fence
- `XRSpace` / `XRPlane` / `XRMaterial` / `XRTexture`
- World / HeadRelative placement
- 左右eye別texture
- final pixel shader HLSL
- compute HLSL texture processing
- D3D11 `ID3D11Texture2D` / `ID3D11ShaderResourceView` wrapper
- D3D12 `ID3D12Resource` wrapper
- CPU RGBA texture作成
- D3D11 / D3D12 sampleとunit/smoke tests

## 依存関係

- Windows 10 / 11 x64
- Visual Studio 2022
- CMake 3.20以上
- Varjo Native SDK / Varjo Runtime
- VarjoToolkit 0.5.0
- glm 1.0.3
- D3D11Helper（D3D11 backend使用時）
- D3D12Helper（D3D12 backend使用時）

VarjoToolkitは外部snapshot入力版の固定コミットをFetchContentで取得します。

## 基本的な使用例

```cpp
#include <VarjoXR/VarjoXR.hpp>
#include <VarjoXR/Backends/D3D12/D3D12Backend.hpp>

std::shared_ptr<VarjoSession> session = std::make_shared<VarjoSession>();
auto core = D3D12CoreLib::D3D12Core::CreateShared({});
auto backend = VarjoXR::Backends::D3D12::CreateBackend(core);

VarjoXR::XRSpace space({session, std::move(backend)});
auto& plane = space.createPlane({1.0f, 0.6f});
plane.setPlacementMode(VarjoXR::PlacementMode::HeadRelative);
plane.transform().position = {0.0f, 0.0f, -1.0f};

while (running) {
    space.update();
    const auto frameInfo = space.frameInfoSnapshot();
}
```

## FrameInfo getter

`frameInfoSnapshot()`は最初の`beginFrame()`前には`valid == false`を返します。

```cpp
space.beginFrame();
const auto frameInfo = space.frameInfoSnapshot();
space.render();
space.endFrame();
```

`beginFrame()`直後にも、`update()`完了後にも同じ同期フレームのsnapshotを取得できます。

## Planeのtexture processing

```cpp
VarjoXR::TextureProcessingDesc processing{};
processing.enabled = true;
processing.timing = VarjoXR::ProcessingTiming::OnTextureChanged;
processing.hlsl = shaderSource;
processing.entryPoint = "main";
processing.target = "cs_5_0";
processing.userConstants.registerIndex = 0;
processing.userConstants.set(constants);
processing.frameConstants.enabled = true;
processing.frameConstants.registerIndex = 1;

plane.setProcessing(VarjoXR::Eye::Left, processing);
plane.setProcessing(VarjoXR::Eye::Right, processing);
```

## ビルド

```bat
set "VARJO_SDK_ROOT=C:\path\to\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

rmdir /s /q out\build\default 2>nul

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJOXR_VARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOXR_ENABLE_D3D11=ON ^
  -DVARJOXR_ENABLE_D3D12=ON ^
  -DVARJOXR_BUILD_SAMPLES=ON ^
  -DVARJOXR_BUILD_TESTS=ON

cmake --build out\build\default --config Release --parallel
ctest --test-dir out\build\default -C Release --output-on-failure
```

## D3D12だけを使用する場合

```bat
-DVARJOXR_ENABLE_D3D11=OFF ^
-DVARJOXR_ENABLE_D3D12=ON
```

## 主なディレクトリ

```text
include/VarjoXR/
src/Backends/D3D11/
src/Backends/D3D12/
samples/
tests/
docs/
```
