# VarjoXR Test Plan

## 目的

VarjoXR は Varjo Native SDK / D3D11Helper / D3D12Helper と連携するため、全機能を通常のCIだけで確認することは難しい。そこで、テストを次の2層に分ける。

1. **Core unit tests**
   - Varjo Runtime、HMD、D3D11Helper、D3D12Helperなしで実行できる。
   - `XRObject`、`XRPlane`、`Material`、`Texture`、`Transform`、`XRSpaceConfig` を対象にする。
   - カバレッジ9割以上をまずこのcore層で目指す。

2. **Runtime / backend integration tests**
   - Varjo Runtime、HMD、D3D11/D3D12環境が必要。
   - swapchain作成、acquire/release、layer submit、Plane描画、左右別texture/shaderを対象にする。
   - backend実装が進んだ段階で追加する。

## 現在追加済みのテスト

`tests/VarjoXRCoreTests.cpp` に、外部テストフレームワークに依存しない小さなテストランナーを実装している。

対象:

- `Eye` / `EyeIndex` / `EyeArray`
- `BackendType`
- `Mat4::Identity`
- `MakeMat4FromVarjoDoubleArray`
- `Transform` default / translation / scale / rotation / combined matrix
- `XRTexture` base class
- `Material` default values
- `XRPlane` constructor / size mutation / object kind
- `XRObject` transform access
- `XRObject` eye別 material
- `XRObject::setTexture`
- `XRObject::setPixelShaderHLSL`
- polymorphic `XRObject` usage
- multiple object independence
- `XRSpaceConfig` defaults and custom values

## 実行方法

Varjo RuntimeやD3DHelperなしでcore testsだけを実行する場合:

```bat
cmake -S . -B out/build/tests -G "Visual Studio 17 2022" -A x64 -DVARJOXR_BUILD_RUNTIME=OFF -DVARJOXR_BUILD_TESTS=ON
cmake --build out/build/tests --config Debug
ctest --test-dir out/build/tests -C Debug --output-on-failure
```

## カバレッジ計測

### Windows / MSVC / OpenCppCoverage

OpenCppCoverage をPATHに入れた状態で実行する。

```bat
cmake -S . -B out/build/coverage -G "Visual Studio 17 2022" -A x64 -DVARJOXR_BUILD_RUNTIME=OFF -DVARJOXR_BUILD_TESTS=ON -DVARJOXR_ENABLE_COVERAGE=ON
cmake --build out/build/coverage --config Debug --target VarjoXRCoverage
```

出力:

```text
out/build/coverage/coverage/html
out/build/coverage/coverage/VarjoXRCoreCoverage.xml
```

### GCC / Clang

`VARJOXR_ENABLE_COVERAGE=ON` で `--coverage` を付与する。lcov/gcovr等でレポート化する。

```bat
cmake -S . -B out/build/coverage -DVARJOXR_BUILD_RUNTIME=OFF -DVARJOXR_BUILD_TESTS=ON -DVARJOXR_ENABLE_COVERAGE=ON
cmake --build out/build/coverage
ctest --test-dir out/build/coverage --output-on-failure
```

## カバレッジ目標

- Core層: 90%以上
- Runtime/backend層: backend実装後にintegration testsを追加し、実機依存部分を除いたロジックで90%以上を目指す。

現時点ではbackend描画処理が未実装のため、backend全体のカバレッジ9割は対象外とする。backend実装時には、Varjo API呼び出しを薄いadapterへ分離し、fake adapterによるテストを追加する。

## 今後追加すべきテスト

backend実装後に次を追加する。

- Varjo swapchain config生成の境界値
- viewIndex -> Eye 解決のfake session test
- D3D11 plane mesh生成 test
- D3D12 plane mesh生成 test
- shader preamble合成 test
- shader compile失敗時fallback test
- material cache invalidation test
- left/right texture binding test
- acquire/release順序 test
- layer submit info生成 test
