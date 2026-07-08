#ifndef VARJOXR_TEXTURE_PROCESSING_HLSLI
#define VARJOXR_TEXTURE_PROCESSING_HLSLI

// VarjoXR programmable texture-processing binding contract.
//
// t0: input texture SRV
// u0: output texture UAV
// b1: VarjoXR-provided frame/texture constants
//
// User-defined constants are intentionally not declared here. By convention they
// are usually bound to b0, but the application may choose another register via
// TextureProcessingDesc::userConstants.registerIndex.

Texture2D<float4> xrInput : register(t0);
RWTexture2D<float4> xrOutput : register(u0);

cbuffer XRTextureProcessingFrameConstants : register(b1)
{
    uint srcWidth;
    uint srcHeight;
    uint dstWidth;
    uint dstHeight;
    float4 frameParams; // gazeUv.x, gazeUv.y, timeSeconds, frameNumber
};

bool VarjoXR_IsOutsideOutput(uint3 dispatchId)
{
    return dispatchId.x >= dstWidth || dispatchId.y >= dstHeight;
}

float2 VarjoXR_OutputUv(uint2 pixel)
{
    return (float2(pixel) + 0.5f) / float2(dstWidth, dstHeight);
}

uint2 VarjoXR_InputPixelFromUv(float2 uv)
{
    const uint x = min((uint)(uv.x * (float)srcWidth), srcWidth - 1u);
    const uint y = min((uint)(uv.y * (float)srcHeight), srcHeight - 1u);
    return uint2(x, y);
}

float4 VarjoXR_LoadNearest(float2 uv)
{
    const uint2 p = VarjoXR_InputPixelFromUv(uv);
    return xrInput.Load(int3((int)p.x, (int)p.y, 0));
}

#endif // VARJOXR_TEXTURE_PROCESSING_HLSLI
