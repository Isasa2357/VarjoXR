# Rendering FrameInfo Snapshot

VarjoXR 0.2.0では、D3D11/D3D12レンダリングバックエンドがVarjoフレーム同期の唯一の所有者です。

## API

```cpp
VarjoFrameInfoSnapshot IRenderBackend::frameInfoSnapshot() const;
VarjoFrameInfoSnapshot XRSpace::frameInfoSnapshot() const;
```

## 動作

`beginFrame()`は内部で1回だけ`varjo_WaitSync`を実行します。その直後に次の情報をsnapshotとして保存します。

- view matrices / projection matrices
- preferred view sizes
- display time
- frame number
- Center Pose

`frameInfoSnapshot()`は保存済みsnapshotのコピーを返します。getter内では`varjo_WaitSync`も`varjo_FrameGetPose`も呼びません。

```cpp
space.update();
const auto frameInfo = space.frameInfoSnapshot();
```

`beginFrame()`前は`valid == false`のsnapshotを返します。

## VarjoToolkitサービスへの配布

```cpp
space.update();

const auto frameInfo = space.frameInfoSnapshot();
eyeTrackingService.submitFrameInfo(frameInfo);
imuService.submitFrameInfo(frameInfo);
```

Eye TrackingとIMUサービスは同じsnapshotを参照するため、レンダリング、視線投影、頭部姿勢ログのフレーム番号と表示時刻を一致させられます。

## 同期所有権

```text
XRSpace / backend
    varjo_WaitSync       1回
    snapshot保存
        ├─ rendering
        ├─ EyeTrackingService
        └─ IMUService
```

サービスや別スレッドから追加の`varjo_WaitSync`を呼ばないでください。
