# ProgrammableProcessing sample

This sample demonstrates the programmable texture-processing prepass used by `XRPlane` materials.

The processing path is:

```text
source texture
  -> compute HLSL prepass
  -> processed texture
  -> default/final Plane pixel shader
  -> Varjo swapchain
```

The sample uses `Shaders/CircleDarkenPreprocess.hlsl`. The common VarjoXR processing declarations are provided by:

```hlsl
#include "VarjoXR/TextureProcessing.hlsli"
```

The binding convention is:

```text
t0: input texture SRV
u0: output texture UAV
b0: user-defined constants
b1: VarjoXR-provided frame/texture constants
```

The user constants are ordinary C++ bytes:

```cpp
struct CircleDarkenConstants {
    float centerX;
    float centerY;
    float radius;
    float outsideBrightness;
    float edgeSoftness;
    float pulseStrength;
    float reserved0;
    float reserved1;
};

VarjoXR::TextureProcessingDesc processing{};
processing.enabled = true;
processing.timing = VarjoXR::ProcessingTiming::BeforeRenderEachFrame;
processing.hlsl = LoadTextFile("CircleDarkenPreprocess.hlsl");
processing.includeDirs.push_back(VarjoXrHlslDirectory());
processing.userConstants.registerIndex = 0;
processing.userConstants.set(constants);
processing.frameConstants.enabled = true;
processing.frameConstants.registerIndex = 1;
```

Build targets:

```text
ProgrammableProcessing_D3D11
ProgrammableProcessing_D3D12
```
