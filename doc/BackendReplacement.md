# Backend replacement note

このコミットでは、VarjoXR の土台を Varjo 公式サンプル `examples/Common/` 非依存に置き換える。

## 方針

- 使用する Varjo 側依存は Varjo Native SDK の C API のみ。
- `Session`, `MultiLayerView`, `D3D11Renderer`, `Scene` など、サンプル `Common/` のクラスは使用しない。
- D3D11 の device / resource / view / pipeline / shader compile は D3D11Helper を使う。
- D3D12 の device / queue / command context / resource / descriptor / pipeline / shader compile は D3D12Helper を使う。
- Context / Focus を含む Varjo の複数 view は、`varjo_GetViewDescription(...).eye` で `Eye::Left` / `Eye::Right` に解決する。

## 初期実装範囲

- `XRSpace` public facade
- `XRPlane` / `XRObject` / eye 別 `Material`
- Varjo session RAII
- Varjo frame wait / begin / acquire / submit / release / end の独自実装の入口
- D3D11 backend の入口
- D3D12 backend の入口
- CPU RGBA からのテクスチャ作成 API
- カスタム pixel shader HLSL 登録 API

D3D12 backend の root signature 作成だけは D3D12Helper 側に対応ヘルパがまだ無いため、今後の実装では最小限の Native D3D12 API を直接呼ぶ可能性がある。
