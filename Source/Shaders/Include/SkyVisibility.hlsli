#ifndef HYP_SKY_VISIBILITY_HLSLI
#define HYP_SKY_VISIBILITY_HLSLI

struct SkyVisibilityCapture
{
    float4x4 viewProjMatrix;
    float4 params;
};

static const float2 s_skyVisibilityKernel[8] = {
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554)
};

float EvaluateSkyVisibility(SkyVisibilityCapture skyVisibilityCapture, Texture2D skyVisibilityTexture, float3 positionWS, float3 N, float2 pixelPosition)
{
    const float strength = world_shader_data.sky_occlusion_params.x;

    if (strength <= 0.0 || skyVisibilityCapture.params.z == 0.0)
    {
        return 1.0;
    }

    const float worldExtent = skyVisibilityCapture.params.x;
    const float depthRange = skyVisibilityCapture.params.y;

    const float filterRadius = world_shader_data.sky_occlusion_params.y;
    const float baseBias = world_shader_data.sky_occlusion_params.z;

    // a sloped surface rises by this much across the filter, and would otherwise shadow itself
    const float slopeTangent = min(sqrt(saturate(1.0 - N.y * N.y)) / max(N.y, 0.15), 1.0) * saturate(N.y);

    const float3 samplePosition = positionWS + N * (baseBias + filterRadius * slopeTangent * 0.5);

    const float4 capturePosition = mul(skyVisibilityCapture.viewProjMatrix, float4(samplePosition, 1.0));
    const float3 captureNdc = capturePosition.xyz / capturePosition.w;

    if (any(abs(captureNdc.xy) > 1.0) || captureNdc.z < 0.0 || captureNdc.z > 1.0)
    {
        return 1.0;
    }

    const float2 captureUv = captureNdc.xy * float2(0.5, -0.5) + 0.5;
    const float sampleRadius = filterRadius / worldExtent;
    
    const float referenceDepth = captureNdc.z - (baseBias + filterRadius * slopeTangent) / depthRange;
    
    const float rotation = InterleavedGradientNoise(pixelPosition) * HYP_FMATH_TWO_PI;
    float rotationSin, rotationCos;
    sincos(rotation, rotationSin, rotationCos);

    const float2x2 rotationMatrix = float2x2(rotationCos, -rotationSin, rotationSin, rotationCos);

    uint2 captureDimensions;
    skyVisibilityTexture.GetDimensions(captureDimensions.x, captureDimensions.y);
    
    const float softnessDepth = max(baseBias, 0.25) / depthRange;

    float openSum = 0.0;

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        const float2 offset = mul(s_skyVisibilityKernel[i], rotationMatrix) * sampleRadius;

        const float2 texelPosition = (captureUv + offset) * float2(captureDimensions) - 0.5;
        const float2 texelFraction = frac(texelPosition);
        const float2 gatherUv = (floor(texelPosition) + 1.0) / float2(captureDimensions);

        const float4 occluderDepths = skyVisibilityTexture.GatherRed(sampler_nearest, gatherUv);
        const float4 open = saturate((occluderDepths - referenceDepth) / softnessDepth + 1.0);
        
        openSum += lerp(lerp(open.w, open.z, texelFraction.x), lerp(open.x, open.y, texelFraction.x), texelFraction.y);
    }
    
    const float2 edgeDistance = 1.0 - abs(captureNdc.xy);
    const float edgeFade = saturate(min(edgeDistance.x, edgeDistance.y) * 8.0);

    return lerp(1.0, openSum / 8.0, strength * edgeFade);
}

#endif // HYP_SKY_VISIBILITY_HLSLI
