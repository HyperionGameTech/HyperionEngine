#include "../include/Defines.hlsli"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.texcoord = input.a_texcoord0;
    output.position_cs = float4(input.a_position, 1.0);

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 output_color : SV_Target0;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SAMPLER(ApplyReflections, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(ApplyReflections, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(ApplyReflections, GBufferAlbedoTexture) Texture2D GBufferAlbedoTexture;
DECLARE_SRV(ApplyReflections, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(ApplyReflections, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(ApplyReflections, GBufferDepthTexture) Texture2D GBufferDepthTexture;

DECLARE_SRV(ApplyReflections, SSAOResultTexture) Texture2D SSAOResultTexture;
DECLARE_SRV(ApplyReflections, ReflectionsResultTexture) Texture2D ReflectionsResultTexture;

#include "../include/Gbuffer.hlsli"
#include "../include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(ApplyReflections, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(ApplyReflections, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "../include/BRDF.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

// Recombines env-probe reflections (already composited with SSR "over" - see ReflectionsPass) with the
// lit scene, applying the same Fresnel / specular-AO / energy-compensation weighting DeferredIndirect.hlsl
// uses for its (non-deferred) reflections path. Additively blended into the lighting framebuffer.
// Split into its own pass so SSR can trace against this frame's fully-lit mip chain instead of last
// frame's - see DeferredPass.cpp for the ordering (runs after the main lighting pass + mip chain generation).
PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    GBufferAlbedoTexture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 pixelCoord = uint2(texcoord * gbufferDimensions);

    const float4 albedo = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferAlbedoTexture, texcoord, 0);
    const float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferNormalsTexture, texcoord, 0);

    const uint materialBits = GBufferMaterialTexture.Load(int3(pixelCoord, 0));

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialBits >> 28u, materialParams);

    const uint mask = materialParams.mask;

    if ((mask & OBJECT_MASK_UNLIT) != 0)
    {
        output.output_color = (float4)0.0;

        return output;
    }

    const float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferDepthTexture, texcoord, 0).r;
    const float3 P = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, texcoord, depth).xyz;

    const float3 N = GBufferUnpackNormal(normalSample);
    const float3 V = normalize(camera.position.xyz - P);

    const float roughness = clamp(materialParams.roughness, 0.001, 0.999);
    const float metalness = materialParams.metalness;
    const float perceptualRoughness = sqrt(roughness);

    const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));

    const float ao = SAMPLE_TEXTURE_2D_LOD(sampler_linear, SSAOResultTexture, texcoord, 0).r;

    const float3 F0 = CalculateF0(albedo.rgb, metalness);
    const float3 dfg = CalculateDFG(perceptualRoughness, NdotV);
    const float3 E = CalculateE(F0, dfg);

    float3 specular_ao = (float3)SpecularAO_Lagarde(NdotV, ao, perceptualRoughness);
    const float3 energy_compensation = CalculateEnergyCompensation(F0, dfg);
    specular_ao *= energy_compensation;

    float4 reflections = SAMPLE_TEXTURE_2D_LOD(sampler_linear, ReflectionsResultTexture, texcoord, 0);
    reflections.rgb *= specular_ao;

    const float3 Fr = E * reflections.rgb;

    output.output_color = float4(Fr, 1.0);

    return output;
}

#endif // PIXEL_SHADER
