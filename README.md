# VarjoXR

VarjoXRは、Varjo Native SDK上でD3D11 / D3D12 textureをMR空間のPlaneとして表示するC++17ライブラリです。

現在のバージョンは **0.3.0** です。

## 0.3.0のレンダリングモデル

`XRSpace`は従来の同期`update()`に加えて、専用レンダリングスレッドを持てるようになりました。

```text
main / control thread
    +-- HLSL constants publish
    +-- latest FrameInfo read

XRSpace render thread
    +-- latest constants apply
    +-- camera resource update callback
    +-- varjo_WaitSync 1回
    +-- render / submit
    +-- FrameInfo publish
```

FrameInfoとHLSL定数は、immutableな値を交互のslotへ公開する`AsyncDoubleBuffer`でlatest-only共有します。consumerが保持中の値は`shared_ptr<const T>`によりproducerのslot再利用から保護されます。

詳細は[`docs/AsyncRendering.md`](docs/AsyncRendering.md)を参照してください。

## 非同期レンダリング例

```cpp
VarjoXR::XRSpace space({session, std::move(backend)});
auto& plane = space.createPlane({1.0f, 0.6f});
plane.setPlacementMode(VarjoXR::PlacementMode::HeadRelative);
plane.transform().position = {0.0f, 0.0f, -1.0f};

VarjoXR::XRSpaceRenderThreadDesc threadDesc;
threadDesc.beforeFrame = [&](VarjoXR::XRSpace& renderSpace) {
    // Render threadで最新のcamera D3D resourceを取得してPlaneへ設定する。
};
threadDesc.afterFrame = [&](VarjoXR::XRSpace&,
                            const VarjoFrameInfoSnapshot& frameInfo) {
    // Render thread専用のfence管理などを行う。
};
space.startRenderThread(std::move(threadDesc));

std::int64_t lastFrameNumber = -1;
while (space.renderThreadRunning()) {
    if (const auto frameInfo = space.latestFrameInfoSnapshot();
        frameInfo && frameInfo->valid &&
        frameInfo->frameNumber != lastFrameNumber) {
        eyeTrackingService.submitFrameInfo(*frameInfo);
        imuService.submitFrameInfo(*frameInfo);
        lastFrameNumber = frameInfo->frameNumber;
    }

    space.rethrowRenderThreadException();
}

space.stopRenderThread();
space.rethrowRenderThreadException();
```

`startRenderThread()`後は、`XRSpace`、backend、Plane、`FrameContext`をレンダースレッドが所有します。mainスレッドから直接変更せず、`beforeFrame`または非同期state publishを使用してください。

## HLSL定数の非同期更新

左右の定数を同じrevisionへ入れることで、同一フレーム境界でまとめて反映できます。

```cpp
VarjoXR::XRSpaceAsyncRenderState state;
state.revision = ++revision;
state.processingConstants.push_back({
    0,
    VarjoXR::Eye::Left,
    leftConstantBytes});
state.processingConstants.push_back({
    0,
    VarjoXR::Eye::Right,
    rightConstantBytes});

space.publishAsyncRenderState(std::move(state));
```

新しいrevisionがなければ、レンダリングスレッドは前回の定数をそのまま使用します。

## 0.2.0以降のFrameInfo同期

D3D11/D3D12レンダリングバックエンドが、アプリケーション内で唯一の`varjo_WaitSync`所有者です。

```text
XRSpace::update()
    backend::beginFrame()
        varjo_WaitSync 1回
        VarjoFrameInfoSnapshot保存
    render
    endFrame
```

同期モードでは次のgetterを使用します。

```cpp
space.update();
const auto frameInfo = space.frameInfoSnapshot();
```

非同期モードではレンダースレッドが保存済みsnapshotをダブルバッファへ公開し、mainは`latestFrameInfoSnapshot()`で読みます。どちらのgetterも、新しい`varjo_WaitSync`や`varjo_FrameGetPose`を実行しません。

詳細は[`docs/FrameInfoSnapshot.md`](docs/FrameInfoSnapshot.md)を参照してください。

## 実装範囲

- 外部`VarjoSession`を使う`XRSpace`
- 同期および専用スレッドによる非同期レンダリング
- latest-only immutable double buffer
- 非同期HLSL user constant更新
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

## 同期レンダリングの基本例

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
