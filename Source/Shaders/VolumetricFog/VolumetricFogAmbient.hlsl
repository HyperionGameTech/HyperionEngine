#include "../Include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#include "../Include/Shared.hlsli"
#include "../Include/EnvProbes.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "VolumetricFogCommon.hlsli"

DECLARE_UAV(VolumetricFog, OutAmbientTexture) RWTexture3D<float4> OutAmbientTexture;

DECLARE_SRV(VolumetricFog, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(VolumetricFog, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;

#include "../Glimmer/GlimmerApply.hlsli"
#include "../Glimmer/GlimmerGround.hlsli"

DECLARE_BUFFER_DYNAMIC(VolumetricFog, VolumetricFogAmbientConstants) cbuffer VolumetricFogAmbientConstants
{
    Camera camera;
    VolumetricFogGrid grid;
    uint4 ambientDimensions; // xyz = cells, w = 1 with a sky probe

    GlimmerGroundParams ground;

    EnvProbe skyProbe;
    GlimmerApply glimmer;
};

#define GLIMMER_APPLY_WITH_SAMPLING
#include "../Glimmer/GlimmerApply.hlsli"

float3 GetSkyAmbient()
{
    if (ambientDimensions.w == 0u)
    {
        return (float3)0.0;
    }

    float upBands[9];
    float downBands[9];

    ProjectSHBands(float3(0.0, 1.0, 0.0), upBands);
    ProjectSHBands(float3(0.0, -1.0, 0.0), downBands);

    return max(0.5 * (EnvProbeSH(skyProbe, upBands) + EnvProbeSH(skyProbe, downBands)), (float3)0.0);
}

[numthreads(4, 4, 4)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint3 cell = dispatchThreadId;

    [branch]
    if (any(cell >= ambientDimensions.xyz))
    {
        return;
    }

    const float3 uvw = (float3(cell) + 0.5) / float3(ambientDimensions.xyz);

    const float depth = VolumetricFogSliceToDepth(grid, uvw.z * float(grid.dimensions.z));

    const float3 rayDirection = VolumetricFogRayDirection(camera.invProjMat, camera.invViewMat, camera.position.xyz, uvw.xy);
    const float3 positionWS = VolumetricFogCellPosition(camera.viewProjMat, camera.position.xyz, rayDirection, depth);

    const float4 up = EvaluateGlimmer(glimmer, positionWS, float3(0.0, 1.0, 0.0));
    const float4 down = EvaluateGlimmer(glimmer, positionWS, float3(0.0, -1.0, 0.0));

    const float weight = saturate(min(up.a, down.a));

    const float3 ambient = lerp(GetSkyAmbient(), 0.5 * (up.rgb + down.rgb), weight) * world_shader_data.atmosphere_fog_params.y;

    float groundHeight;
    uint groundLevel;

    bool b = GlimmerSampleGround(ground, positionWS.xz, 0u, groundHeight, groundLevel);
    groundHeight = select(b, groundHeight, 0.0);

    OutAmbientTexture[cell] = float4(ambient, groundHeight);
}
