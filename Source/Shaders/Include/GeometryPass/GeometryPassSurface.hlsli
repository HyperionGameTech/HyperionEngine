#ifndef HYP_GEOMETRY_PASS_SURFACE_HLSLI
#define HYP_GEOMETRY_PASS_SURFACE_HLSLI

#include "Include/GeometryPass/GeometryPassCommon.hlsli"

struct GeometrySurface
{
    float3 P;
    float3 V;
    float3 N;
    float3 geometricNormal;
    float2 texcoord;
    float4 albedo;
    float ao;
    float metalness;
    float perceptualRoughness;
    float transmission;
    float coverMask;
    float coverRoughness;
};

// @TODO!!! Replace with vertex tangents.
bool ComputeUVTangentFrame(float3 N, float3 P, float2 uv, out float3 tangent, out float3 bitangent)
{
    const float3 dpdx = ddx(P);
    const float3 dpdy = ddy(P);
    const float2 duvdx = ddx(uv);
    const float2 duvdy = ddy(uv);

    const float3 dpdyPerp = cross(dpdy, N);
    const float3 dpdxPerp = cross(N, dpdx);

    const float handedness = dot(dpdx, dpdyPerp) < 0.0 ? -1.0 : 1.0;

    tangent = (dpdyPerp * duvdx.x + dpdxPerp * duvdy.x) * handedness;
    bitangent = (dpdyPerp * duvdx.y + dpdxPerp * duvdy.y) * -handedness;

    const float tangentLengthSq = dot(tangent, tangent);
    const float bitangentLengthSq = dot(bitangent, bitangent);

    if (min(tangentLengthSq, bitangentLengthSq) <= 1e-12 * max(tangentLengthSq, bitangentLengthSq))
    {
        tangent = (float3)0.0;
        bitangent = (float3)0.0;

        return false;
    }

    tangent *= rsqrt(tangentLengthSq);
    bitangent *= rsqrt(bitangentLengthSq);

    return true;
}

float3 GetMeshDetailBlending(float3 normal)
{
    const float3 blending = pow(abs(normal), 4.0);
    return blending / max(blending.x + blending.y + blending.z, 0.0001);
}

float4 SampleMeshTriplanar(Texture2D tex, float3 position, float3 positionDdx, float3 positionDdy, float scale, float3 blending, out float4 packedX, out float4 packedY, out float4 packedZ)
{
    packedX = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, position.zy * scale, positionDdx.zy * scale, positionDdy.zy * scale);
    packedY = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, position.xz * scale, positionDdx.xz * scale, positionDdy.xz * scale);
    packedZ = SAMPLE_TEXTURE_2D_GRAD(texture_sampler, tex, position.xy * scale, positionDdx.xy * scale, positionDdy.xy * scale);

    return packedX * blending.x + packedY * blending.y + packedZ * blending.z;
}

/// rg = tangent normal xy
/// b = ao
/// a = height (terrain layer packing).
float3 BlendDetailNormalTriplanar(float4 packedX, float4 packedY, float4 packedZ, float3 baseNormal, float3 blending, float strength)
{
    const float2 tangentX = (packedX.rg * 2.0 - 1.0) * strength;
    const float2 tangentY = (packedY.rg * 2.0 - 1.0) * strength;
    const float2 tangentZ = (packedZ.rg * 2.0 - 1.0) * strength;

    const float3 whiteoutX = float3(tangentX + baseNormal.zy, baseNormal.x);
    const float3 whiteoutY = float3(tangentY + baseNormal.xz, baseNormal.y);
    const float3 whiteoutZ = float3(tangentZ + baseNormal.xy, baseNormal.z);

    return normalize(whiteoutX.zyx * blending.x + whiteoutY.xzy * blending.y + whiteoutZ.xyz * blending.z);
}

// #define DEBUG_RAW_REFLECTIONS

// backFaceSign flips the uv tangent frame, so a back face can mirror the normal map's perturbation
GeometrySurface SampleGeometrySurface(PSInput input, float backFaceSign)
{
    GeometrySurface surface;

    float3x3 tbn_matrix = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));

    const float3 P = input.position.xyz;

    surface.P = P;
    surface.V = normalize(camera.position.xyz - P);

    float3 N = normalize(input.normal);

    surface.geometricNormal = N;

    const float3 positionDdx = ddx(P);
    const float3 positionDdy = ddy(P);

    surface.albedo = CURRENT_MATERIAL.albedo;

    float ao = input.color.a;
    surface.metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    surface.perceptualRoughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    surface.transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);
    const float alpha_threshold = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ALPHA_THRESHOLD);

    float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;

    // derivatives need uniform control flow, so this goes before any branch or clip. degenerate UVs keep the old basis
    float3 uvTangent;
    float3 uvBitangent;

    if (ComputeUVTangentFrame(N, P, texcoord, uvTangent, uvBitangent))
    {
        tbn_matrix = float3x3(uvTangent * backFaceSign, uvBitangent * backFaceSign, N);
    }

    if (HAS_TEXTURE(CURRENT_MATERIAL, ParallaxMap))
    {
        bool flipHeight = bool(GET_MATERIAL_PARAM_BIT(CURRENT_MATERIAL, MATERIAL_FLAG_PARALLAX_INVERSE_HEIGHT));

        float3x3 cotangent_matrix = CotangentFrame(N, P, texcoord);
        float3 tangent_view = mul(cotangent_matrix, surface.V);
        float2 parallax_texcoord = ParallaxMappedTexCoords(
            CURRENT_MATERIAL.parallax_height,
            texcoord,
            normalize(tangent_view),
            flipHeight);

        texcoord = parallax_texcoord;
    }

    surface.texcoord = texcoord;

    if (HAS_TEXTURE(CURRENT_MATERIAL, DiffuseMap))
    {
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, texcoord);

#ifdef ALPHA_DISCARD
        const float diffuseMipLevel = HYP_TEXTURE_LOD(texture_sampler, GET_TEXTURE(CURRENT_MATERIAL, DiffuseMap), texcoord) + MATERIAL_TEXTURE_MIP_BIAS;

        const float cutoutCoverage = AlphaCutoutCoverage(albedo_texture.a, alpha_threshold, diffuseMipLevel);
        const float cutoutNoise = world_shader_data.cutout_params.x
            * InterleavedGradientNoiseAnimated(
                input.position_cs.xy,
                world_shader_data.frame_counter % 64u);

        if (ShouldDiscardCutout(cutoutCoverage, cutoutNoise, input.cutout_seed))
        {
            discard;
        }
#endif
        surface.albedo *= albedo_texture;
    }

    if (GET_MATERIAL_PARAM_BIT(CURRENT_MATERIAL, MATERIAL_FLAG_TERRAIN_COLOR_MATCH))
    {
        surface.albedo.rgb *= GetTerrainMacroColorGain(SampleTerrainMacroNoise(P.xz));
    }

    surface.albedo.rgb = min(surface.albedo.rgb * input.color.rgb, 1.0);
    surface.albedo.a = max(surface.albedo.a, 0.005);

    float4 normals_texture = float4(0.0, 0.0, 0.0, 0.0);

 #ifndef DEBUG_RAW_REFLECTIONS
    if (HAS_TEXTURE(CURRENT_MATERIAL, NormalMap))
    {
        normals_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, NormalMap, texcoord) * 2.0 - 1.0;
        normals_texture.y *= select((float) GET_MATERIAL_PARAM_BIT(CURRENT_MATERIAL, MATERIAL_FLAG_NORMAL_MAP_FLIP_Y), -1.0f, 1.0f);

        N = normalize(mul(normals_texture.xyz, tbn_matrix));
    }
 #endif

    const float viewDistance = length(camera.position.xyz - P);
    const float3 detailBlending = GetMeshDetailBlending(surface.geometricNormal);

    float detailHeight = 0.5;

    if (HAS_TEXTURE(CURRENT_MATERIAL, DetailNormalMap))
    {
        const float coarseFade = 1.0 - smoothstep(MESH_DETAIL_FADE.x, MESH_DETAIL_FADE.y, viewDistance);
        const float fineFade = 1.0 - smoothstep(MESH_DETAIL_FINE_FADE.x, MESH_DETAIL_FINE_FADE.y, viewDistance);

        float4 packedX;
        float4 packedY;
        float4 packedZ;

        const float4 coarseDetail = SampleMeshTriplanar(GET_TEXTURE(CURRENT_MATERIAL, DetailNormalMap), P, positionDdx, positionDdy, MESH_DETAIL_SCALE, detailBlending, packedX, packedY, packedZ);
        N = BlendDetailNormalTriplanar(packedX, packedY, packedZ, N, detailBlending, MESH_DETAIL_NORMAL_STRENGTH * coarseFade);

        float detailAo = coarseDetail.b;
        detailHeight = coarseDetail.a;

        if (fineFade > 0.0)
        {
            const float4 fineDetail = SampleMeshTriplanar(GET_TEXTURE(CURRENT_MATERIAL, DetailNormalMap), P, positionDdx, positionDdy, MESH_DETAIL_FINE_SCALE, detailBlending, packedX, packedY, packedZ);
            N = BlendDetailNormalTriplanar(packedX, packedY, packedZ, N, detailBlending, MESH_DETAIL_FINE_NORMAL_STRENGTH * fineFade);

            detailAo *= lerp(1.0, fineDetail.b, fineFade);
        }

        ao *= lerp(1.0, detailAo, MESH_DETAIL_AO_STRENGTH * coarseFade);

        if (HAS_TEXTURE(CURRENT_MATERIAL, DetailMap))
        {
            const float3 detailAlbedo = SampleMeshTriplanar(GET_TEXTURE(CURRENT_MATERIAL, DetailMap), P, positionDdx, positionDdy, MESH_DETAIL_SCALE, detailBlending, packedX, packedY, packedZ).rgb;
            const float3 meanAlbedo = SAMPLE_TEXTURE_2D_LOD(texture_sampler, GET_TEXTURE(CURRENT_MATERIAL, DetailMap), float2(0.5, 0.5), 12.0).rgb;

            const float albedoRatio = clamp(Luminance(detailAlbedo) / max(Luminance(meanAlbedo), 0.01), 0.3, 2.0);

            surface.albedo.rgb = min(surface.albedo.rgb * lerp(1.0, albedoRatio, MESH_DETAIL_ALBEDO_STRENGTH * coarseFade), 1.0);
        }
    }

    surface.coverMask = 0.0;
    surface.coverRoughness = 1.0;

    if (HAS_TEXTURE(CURRENT_MATERIAL, CoverMap))
    {
        const float upMask = smoothstep(MESH_COVER_SLOPE.x, MESH_COVER_SLOPE.y, N.y);
        surface.coverMask = smoothstep(0.35, 0.65, upMask + (detailHeight - 0.5) * MESH_COVER_HEIGHT_BREAKUP);

        if (surface.coverMask > 0.0)
        {
            float4 packedX;
            float4 packedY;
            float4 packedZ;

            const float4 coverSample = SampleMeshTriplanar(GET_TEXTURE(CURRENT_MATERIAL, CoverMap), P, positionDdx, positionDdy, TERRAIN_LAYER0_SCALE, detailBlending, packedX, packedY, packedZ);

            const float3 coverAlbedo = min(
                coverSample.rgb * GetTerrainLayerTintAtDistance(0u, viewDistance) * GetTerrainMacroColorGain(SampleTerrainMacroNoise(P.xz)),
                TERRAIN_MAX_ALBEDO);

            surface.albedo.rgb = lerp(surface.albedo.rgb, coverAlbedo, surface.coverMask);
            surface.coverRoughness = lerp(0.78, 1.0, coverSample.a);

            float3 coverNormal = normalize(N + float3(0.0, 1.0, 0.0));

            if (HAS_TEXTURE(CURRENT_MATERIAL, CoverNormalMap))
            {
                const float4 coverNormalSample = SampleMeshTriplanar(GET_TEXTURE(CURRENT_MATERIAL, CoverNormalMap), P, positionDdx, positionDdy, TERRAIN_LAYER0_SCALE, detailBlending, packedX, packedY, packedZ);
                coverNormal = BlendDetailNormalTriplanar(packedX, packedY, packedZ, coverNormal, detailBlending, 1.0);

                ao *= lerp(1.0, coverNormalSample.b, surface.coverMask);
            }

            N = normalize(lerp(N, coverNormal, surface.coverMask));
        }
    }

    surface.N = N;
    surface.ao = ao;

    if (HAS_TEXTURE(CURRENT_MATERIAL, MetalnessMap))
    {
        float4 metalness_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, MetalnessMap, texcoord);
        float metalness_sample = SelectMaterialChannel(metalness_texture, GET_MATERIAL_METALNESS_CHANNEL(CURRENT_MATERIAL));

        surface.metalness = metalness_sample;
    }

    if (HAS_TEXTURE(CURRENT_MATERIAL, RoughnessMap))
    {
        float4 roughness_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, RoughnessMap, texcoord);
        float roughness_sample = SelectMaterialChannel(roughness_texture, GET_MATERIAL_ROUGHNESS_CHANNEL(CURRENT_MATERIAL));

        surface.perceptualRoughness = roughness_sample;

        const uint transmissionMask = GET_MATERIAL_TRANSMISSION_MASK(CURRENT_MATERIAL);

        if (transmissionMask != 0u)
        {
            surface.transmission *= SelectMaterialChannel(roughness_texture, transmissionMask - 1u);
        }
    }

    return surface;
}

void FinishGeometrySurface(inout GeometrySurface surface)
{
    surface.perceptualRoughness = lerp(surface.perceptualRoughness, surface.coverRoughness, surface.coverMask);

#if SHADING_TYPE_FORWARD
    // geometric specular AA (Tokuyoshi & Kaplanyan 2019)
    // smooth curved surfaces like glass alias badly at a distance once the normal turns a lot within a pixel
    // and TAA can't hold on to it as the jitter lands on a different lobe every frame
    {
        const float3 dNdx = ddx(surface.N);
        const float3 dNdy = ddy(surface.N);
        const float variance = 0.15 * (dot(dNdx, dNdx) + dot(dNdy, dNdy));
        const float kernelRoughness = min(2.0 * variance, 0.2);
        const float alpha = surface.perceptualRoughness * surface.perceptualRoughness;

        surface.perceptualRoughness = sqrt(sqrt(saturate(alpha * alpha + kernelRoughness)));
    }
#endif // SHADING_TYPE_FORWARD
}

#endif // HYP_GEOMETRY_PASS_SURFACE_HLSLI
