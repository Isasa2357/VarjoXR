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

## 必要環境

- Visual Studio 2026 / Visual Studio 18 以降
- CMake 4.2 以降
  - `Visual Studio 18 2026` generator は CMake 4.2 以降が必要

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

Varjo RuntimeやD3DHelperなしでcore testsだけを実行する場合。既存cloneのリポジトリルートで実行する。

推奨: GitHub同期込みで CMake preset を使う方法。

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

## カバレッジ計測

### Windows / MSVC / OpenCppCoverage

OpenCppCoverage をPATHに入れた状態で実行する。既存cloneのリポジトリルートで実行する。

推奨: GitHub同期込みで CMake preset を使う方法。

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake --preset vs18-x64-coverage
cmake --build --preset vs18-x64-coverage-debug
```

OpenCppCoverage が PATH に無い場合、`VarjoXRCoverage` target は coverage レポートを出さずに通常テストだけを実行する。

presetを使わない場合:

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
cmake -S . -B out/build/vs18-x64-coverage -G "Visual Studio 18 2026" -A x64 -DVARJOXR_BUILD_RUNTIME=OFF -DVARJOXR_BUILD_TESTS=ON -DVARJOXR_ENABLE_COVERAGE=ON
cmake --build out/build/vs18-x64-coverage --config Debug --target VarjoXRCoverage
```

出力:

```text
out/build/vs18-x64-coverage/coverage/html
out/build/vs18-x64-coverage/coverage/VarjoXRCoreCoverage.xml
```

### GCC / Clang

`VARJOXR_ENABLE_COVERAGE=ON` で `--coverage` を付与する。lcov/gcovr等でレポート化する。

```bat
git fetch --prune origin
git checkout main
git pull --ff-only origin main
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
