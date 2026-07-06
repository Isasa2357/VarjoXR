# VarjoXR

VarjoXR は、Varjo Native SDK の上に「Plane / Cube / Sphere などのオブジェクトを作成して、XR 空間へ提出するだけで表示できる」ことを目標にした C++ ライブラリです。

現在の主スコープは **P1: Plane 表示** です。P1 では、1 枚の Plane に対して左右の目へ個別のテクスチャと HLSL ピクセルシェーダを割り当てる立体視表示を扱います。

## Status

- 対象: Windows / Direct3D 11 / Varjo Native SDK
- P1: Plane のみ
- 左右 Eye ごとの Texture / Pixel Shader 切り替え
- Varjo SDK サンプルの `examples/Common/` を再利用
- Cube / Sphere / 動的テクスチャ更新 / SurfaceTexture 相互作用は P2 以降

## 目的

Varjo Native SDK は低レイヤの制御が可能ですが、単純に「空間上へテクスチャ付き平面を置く」だけでも、セッション管理、フレーム同期、ビューごとの描画、D3D11 リソース管理などを毎回扱う必要があります。

VarjoXR はこの処理を薄くラップし、利用者コード側では次のような API で XR/MR 表示を扱える状態を目指します。

```cpp
#include <VarjoXR/XRSpace.hpp>

int main()
{
    VarjoXR::XRSpace space;

    auto* plane = space.createPlane({1.0f, 0.6f});
    plane->transform().position = {0.0f, 0.0f, -1.0f};

    auto leftTex  = space.loadTexture("left.png");
    auto rightTex = space.loadTexture("right.png");

    plane->setTexture(VarjoXR::Eye::Left,  leftTex);
    plane->setTexture(VarjoXR::Eye::Right, rightTex);

    while (space.update()) {
        // application update
    }
}
```

## P1 の設計方針

VarjoXR は、既存の Varjo 公式サンプル資産を可能な限り再利用し、その上に Scene / Object / Material API を追加します。

```text
Application
  └─ VarjoXR public API
      ├─ XRSpace
      ├─ XRObject / XRPlane
      ├─ Transform
      ├─ Material
      └─ ResourceManager
          └─ Varjo examples/Common/
              ├─ Session
              ├─ MultiLayerView
              ├─ D3D11Renderer
              └─ Scene
```

P1 では、Varjo の Context / Focus を直接アプリケーション側で意識せず、`viewIndex` から `Eye::Left` / `Eye::Right` を解決して描画します。これにより、Context-Left / Focus-Left には左目用 Material、Context-Right / Focus-Right には右目用 Material が適用されます。

## 主なクラス

| Class | Role |
|---|---|
| `XRSpace` | セッション、ビュー、レンダラ、シーン、リソースをまとめる公開ファサード |
| `XRSession` | `varjo_Session` の生成、破棄、イベント処理 |
| `XRView` | `D3D11MultiLayerView` の薄い派生。`viewIndex` から `Eye` を解決 |
| `XRScene` | `Scene` 派生。Eye ごとの `Material` を選択して描画 |
| `ResourceManager` | Plane メッシュ、ビルトインシェーダ、カスタム HLSL の管理 |
| `XRObject` | `Transform` と Eye ごとの `Material` を持つ基底クラス |
| `XRPlane` | Plane 形状の XRObject |
| `Material` | 1 つの Eye に対する Texture / Shader の組 |

## ディレクトリ構成

想定する配置は、Varjo Native SDK のルート直下にこのリポジトリを置く構成です。

```text
VarjoSDK/
├── include/
│   └── Varjo_*.h
├── examples/
│   └── Common/
│       ├── Session.*
│       ├── MultiLayerView.*
│       ├── D3D11Renderer.*
│       └── Scene.*
└── VarjoXR/
    ├── CMakeLists.txt
    ├── README.md
    ├── include/
    │   └── VarjoXR/
    │       ├── Eye.hpp
    │       ├── Transform.hpp
    │       ├── Material.hpp
    │       ├── ResourceManager.hpp
    │       ├── XRObject.hpp
    │       ├── XRPlane.hpp
    │       ├── XRView.hpp
    │       ├── XRSession.hpp
    │       ├── XRScene.hpp
    │       └── XRSpace.hpp
    └── src/
        ├── ResourceManager.cpp
        ├── XRObject.cpp
        ├── XRPlane.cpp
        ├── XRView.cpp
        ├── XRSession.cpp
        ├── XRScene.cpp
        └── XRSpace.cpp
```

## ビルド想定

### Requirements

- Windows 10 / 11
- Visual Studio 2022
- CMake
- Varjo Base
- Varjo Native SDK
- Direct3D 11

### CMake example

VarjoSDK 直下に `VarjoXR` を置いた場合の想定です。

```bat
cmake -S .\VarjoXR -B .\VarjoXR\out\build\default -G "Visual Studio 17 2022" -A x64
cmake --build .\VarjoXR\out\build\default --config Debug
```

VarjoSDK と別の場所に置く場合は、実装側の CMake で `VARJO_SDK_ROOT` のようなルート指定を受け取れるようにする予定です。

```bat
set "VARJO_SDK_ROOT=C:\path\to\VarjoSDK"
cmake -S . -B out\build\default -G "Visual Studio 17 2022" -A x64 -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%"
cmake --build out\build\default --config Debug
```

## カスタム HLSL

`XRPlane` には、左右 Eye ごとに HLSL ピクセルシェーダを登録できます。

```cpp
plane->setPixelShaderHLSL(
    space.resources(),
    VarjoXR::Eye::Left,
    R"hlsl(
float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 c = xrTexture.Sample(xrSampler, uv);
    return float4(c.rgb, 1.0);
}
)hlsl");
```

ユーザー HLSL には、以下のリソースが事前定義される想定です。

```hlsl
Texture2D xrTexture : register(t0);
SamplerState xrSampler : register(s0);
cbuffer XRPlanePSConstants : register(b0) { float4 tint; };
```

ユーザーが定義する必要があるのは、次のシグネチャを持つ `main` のみです。

```hlsl
float4 main(float2 uv : TEXCOORD0) : SV_TARGET
```

## HLSL コンパイル失敗時の方針

Varjo SDK サンプル由来の `D3D11Renderer::createShader()` は、シェーダのコンパイル失敗を致命的エラーとして扱う設計です。VarjoXR では、ユーザー入力の HLSL だけは別経路で `D3DCompile` を呼び、コンパイル失敗時にプロセス全体を落とさない方針にします。

P1 の想定動作は次の通りです。

- カスタム HLSL のコンパイルに成功した場合、その Eye に適用する
- コンパイルに失敗した場合、`OutputDebugStringA` へエラーを出す
- 失敗した Eye はデフォルトシェーダへフォールバックする
- もう片方の Eye とフレームループは継続する

## Encoding policy

このリポジトリでは、既存プロジェクトとの整合のため、ソースファイルのエンコーディングを次のように扱います。

| File | Encoding |
|---|---|
| `.hpp` / `.cpp` | UTF-16 LE with BOM / LF |
| `CMakeLists.txt` | UTF-8 without BOM / CRLF |
| `README.md` / `.gitignore` | UTF-8 without BOM |

## Roadmap

### P1

- `XRSpace` によるフレームループ統括
- `XRPlane` の作成
- Plane メッシュ共有
- 左右 Eye ごとの Texture / Pixel Shader
- カスタム HLSL の安全なコンパイルとフォールバック

### P2 以降

- 動的テクスチャ更新 API
- Cube / Sphere / Circle などの形状追加
- RenderTexture / SurfaceTexture
- オブジェクト間相互作用用の InteractionSystem
- D3D11Helper / D3DInterop / D3DVideoEncoder など周辺ライブラリとの連携

## License

未定です。公開リポジトリとして運用する場合は、用途に合わせて `LICENSE` を追加してください。
