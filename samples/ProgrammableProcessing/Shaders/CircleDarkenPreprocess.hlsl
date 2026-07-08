Texture2D<float4> xrInput : register(t0);
RWTexture2D<float4> xrOutput : register(u0);

cbuffer CircleDarkenConstants : register(b0)
{
    float centerX;
    float centerY;
    float radius;
    float outsideBrightness;

    float edgeSoftness;
    float pulseStrength;
    float reserved0;
    float reserved1;
};

cbuffer XRTextureProcessingFrameConstants : register(b1)
{
    uint srcWidth;
    uint srcHeight;
    uint dstWidth;
    uint dstHeight;
    float4 frameParams; // gazeUv.x, gazeUv.y, timeSeconds, frameNumber
};

float4 SampleNearest(float2 uv)
{
    const uint x = min((uint)(uv.x * (float)srcWidth), srcWidth - 1u);
    const uint y = min((uint)(uv.y * (float)srcHeight), srcHeight - 1u);
    return xrInput.Load(int3((int)x, (int)y, 0));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= dstWidth || id.y >= dstHeight) {
        return;
    }

    const float2 uv = (float2(id.xy) + 0.5f) / float2(dstWidth, dstHeight);
    float4 color = SampleNearest(uv);

    const float timeSeconds = frameParams.z;
    const float animatedRadius = radius + sin(timeSeconds * 3.14159265f * 2.0f) * pulseStrength;
    const float d = distance(uv, float2(centerX, centerY));
    const float soft = max(edgeSoftness, 1.0e-5f);
    const float outsideMask = smoothstep(animatedRadius - soft, animatedRadius + soft, d);
    const float brightness = lerp(1.0f, outsideBrightness, outsideMask);
    color.rgb *= brightness;

    xrOutput[id.xy] = color;
}
