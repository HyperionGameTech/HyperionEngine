#include "include/Defines.hlsli"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position = float4(input.a_position, 1.0);
    output.texcoord = input.a_texcoord0;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/Scene.hlsli"
#include "include/Shared.hlsli"
#include "include/EnvProbes.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(HeightFog, SamplerLinear) SamplerState SamplerLinear;

DECLARE_SRV(HeightFog, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(HeightFog, EnvProbesColorTexture) TextureCubeArray EnvProbesColorTexture;

DECLARE_SRV(HeightFog, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_BUFFER_DYNAMIC(HeightFog, HeightFogConstants) cbuffer HeightFogConstants
{
    Camera camera;
    Light sun;
    EnvProbe skyProbe;

    uint hasSun;
    uint hasSkyProbe;
    uint _pad0;
    uint _pad1;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color : SV_Target0;
};

static const float SkyInscatterMipLevel = 5.0;

float HeightFogPhase(float cosTheta, float anisotropy)
{
    const float anisotropySquared = anisotropy * anisotropy;
    const float denominator = pow(max(1.0 + anisotropySquared - 2.0 * anisotropy * cosTheta, 1e-4), 1.5);

    return (1.0 - anisotropySquared) / (4.0 * HYP_FMATH_PI * denominator);
}

// density falls off exponentially with height, so its integral along a straight ray has a closed form
float GetHeightFogOpticalDepth(float originHeight, float directionY, float rayLength)
{
    const float density = world_shader_data.height_fog_params.x;
    const float heightFalloff = world_shader_data.height_fog_params.y;
    const float baseHeight = world_shader_data.height_fog_params.z;

    if (density <= 0.0)
    {
        return 0.0;
    }

    const float densityAtOrigin = density * exp(min(-heightFalloff * (originHeight - baseHeight), 60.0));

    // clamped so a long ray descending into the fog saturates instead of overflowing
    const float heightChange = max(heightFalloff * directionY * rayLength, -60.0);
    const float integralFactor = abs(heightChange) > 1e-4 ? (1.0 - exp(-heightChange)) / heightChange : 1.0;

    return densityAtOrigin * rayLength * integralFactor;
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    output.color = float4(0.0, 0.0, 0.0, 1.0);

    const float2 texcoord = input.texcoord;

    uint2 depthDimensions;
    GBufferDepthTexture.GetDimensions(depthDimensions.x, depthDimensions.y);

    const float depth = GBufferDepthTexture.Load(int3(min(uint2(texcoord * float2(depthDimensions)), depthDimensions - 1u), 0)).r;

    const float3 positionWS = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, texcoord, depth).xyz;
    const float3 toPosition = positionWS - camera.position.xyz;
    const float distanceToPosition = length(toPosition);
    const float3 viewDirection = toPosition / max(distanceToPosition, 1e-4);

    const float startDistance = world_shader_data.height_fog_params.w;
    const float rayLength = distanceToPosition - startDistance;

    if (rayLength <= 0.0)
    {
        return output;
    }

    const float fogStartHeight = camera.position.y + viewDirection.y * startDistance;

    float opticalDepth = GetHeightFogOpticalDepth(fogStartHeight, viewDirection.y, rayLength);

    const float aerialPerspectiveDistance = world_shader_data.atmosphere_fog_params.x;

    if (aerialPerspectiveDistance > 0.0)
    {
        opticalDepth += rayLength / aerialPerspectiveDistance;
    }

    const float maxOpacity = world_shader_data.atmosphere_fog_params.w;
    const float transmittance = max(exp(-opticalDepth), 1.0 - maxOpacity);

    float3 inscatter = (float3)0.0;

    const uint skyTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(skyProbe);

    if (hasSkyProbe != 0 && skyTextureIndex != INVALID_ENV_PROBE_TEXTURE)
    {
        // looking down still hazes toward the horizon, not the ground bounce below it
        const float3 skyDirection = normalize(float3(viewDirection.x, max(viewDirection.y, 0.05), viewDirection.z));

        inscatter += EnvProbeSample(SamplerLinear, EnvProbesColorTexture, skyTextureIndex, skyDirection, SkyInscatterMipLevel).rgb
            * world_shader_data.atmosphere_fog_params.y;
    }

    if (hasSun != 0)
    {
        const float3 directionToSun = normalize(sun.position_intensity.xyz);
        const float phase = HeightFogPhase(dot(viewDirection, directionToSun), world_shader_data.fog_phase_params.x);

        // no direct sun through an overcast sky
        const float overcastWeight = smoothstep(0.5, 1.0, world_shader_data.sky_light_params.w);

        inscatter += sun.color.rgb * sun.position_intensity.w * phase
            * world_shader_data.atmosphere_fog_params.z
            * saturate(directionToSun.y * 10.0 + 1.0)
            * (1.0 - overcastWeight);
    }

    output.color = float4(inscatter * (1.0 - transmittance), transmittance);

    return output;
}

#endif // PIXEL_SHADER
