#include "VarjoXR/TextureProcessing.hlsli"

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

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (VarjoXR_IsOutsideOutput(id)) {
        return;
    }

    const float2 uv = VarjoXR_OutputUv(id.xy);
    float4 color = VarjoXR_LoadNearest(uv);

    const float timeSeconds = frameParams.z;
    const float animatedRadius = radius + sin(timeSeconds * 3.14159265f * 2.0f) * pulseStrength;
    const float d = distance(uv, float2(centerX, centerY));
    const float soft = max(edgeSoftness, 1.0e-5f);
    const float outsideMask = smoothstep(animatedRadius - soft, animatedRadius + soft, d);
    const float brightness = lerp(1.0f, outsideBrightness, outsideMask);
    color.rgb *= brightness;

    xrOutput[id.xy] = color;
}
