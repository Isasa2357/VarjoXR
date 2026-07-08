# RenderingPlane samples

These samples are split according to the v0.1 design document.

Each sample is built for the enabled backend(s):

| Sample | D3D11 target | D3D12 target | Purpose |
|---|---|---|---|
| 01_SinglePlane | `RenderingPlane_01_SinglePlane_D3D11` | `RenderingPlane_01_SinglePlane_D3D12` | One world-placed textured Plane |
| 02_HeadRelativePlane | `RenderingPlane_02_HeadRelativePlane_D3D11` | `RenderingPlane_02_HeadRelativePlane_D3D12` | One head-relative textured Plane |
| 03_StereoPlane | `RenderingPlane_03_StereoPlane_D3D11` | `RenderingPlane_03_StereoPlane_D3D12` | One Plane with left/right eye textures |
| 04_ShaderPlane | `RenderingPlane_04_ShaderPlane_D3D11` | `RenderingPlane_04_ShaderPlane_D3D12` | Final pixel shader replacement |
| 05_MultiplePlanes | `RenderingPlane_05_MultiplePlanes_D3D11` | `RenderingPlane_05_MultiplePlanes_D3D12` | Multiple world-placed Planes |
| 06_ProcessingPlane | `RenderingPlane_06_ProcessingPlane_D3D11` | `RenderingPlane_06_ProcessingPlane_D3D12` | Programmable texture-processing prepass |

The old `RenderingPlane_D3D11` and `RenderingPlane_D3D12` targets are kept as legacy minimal examples.
