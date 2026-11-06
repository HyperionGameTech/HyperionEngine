#ifndef HYP_GEOMETRY_PASS_SHADING_HLSLI
#define HYP_GEOMETRY_PASS_SHADING_HLSLI

#include "Include/GeometryPass/GeometryPassSurface.hlsli"

// Includers must define the following functions.
//   float GetRefractionTransmission(GeometrySurface surface)
//   float3 EvaluateDirectLight(GeometrySurface surface, float3 N, float3 V, float3 L, float NdotL, float3 diffuseLobe, float3 specularLobe, float3 energyCompensation)

#if SHADING_TYPE_FORWARD
float4 ShadeForward(PSInput input, GeometrySurface surface)
{
    const float3 P = surface.P;
    const float3 V = surface.V;
    const float3 N = surface.N;

    const float ao = surface.ao;
    const float metalness = surface.metalness;
    const float perceptualRoughness = surface.perceptualRoughness;
    const float roughness = perceptualRoughness * perceptualRoughness;

    float4 result = surface.albedo;

    const float3 albedo = surface.albedo.rgb;

    const bool drawUnlit = (input.object_mask & OBJECT_MASK_UNLIT) != 0;

    if (drawUnlit)
    {
        result = float4(albedo, 1.0);
    }
    else
    {
        const float refractionTransmission = GetRefractionTransmission(surface);

        float3 diffuseColor = CalculateDiffuseColor(albedo, metalness) * (1.0 - refractionTransmission);

        {
            float3 indirect_lighting = 0;
            float3 direct_lighting = 0;

            const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));

            const float3 R = normalize(reflect(-V, N));

            const float ior = CURRENT_MATERIAL.refraction.x;

            const float3 F0 = CalculateF0(albedo, metalness, ior);
            const float3 dfg = CalculateDFG(perceptualRoughness, NdotV);
            const float3 E = CalculateE(F0, dfg);

            const float3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);

#ifdef FORWARD_CLUSTERED
            const float2 screenUV = (input.position_ndc.xy / input.position_ndc.w) * 0.5 + 0.5;

            float4 positionVS = mul(camera.view, float4(P, 1.0));
            positionVS /= positionVS.w;
#endif // FORWARD_CLUSTERED

            { // Indirect part.
                float3 Ft = CalculateRefraction(
                    camera.dimensions.xy,
                    P, N, V,
                    surface.texcoord,
                    F0, E,
                    refractionTransmission, perceptualRoughness, ior,
                    float4(0.0, 0.0, 0.0, 0.0),
                    surface.albedo,
                    float3(ao, ao, ao));

                float3 Fr = (float3)0;
                float3 Fd = (float3)0;

#ifdef FORWARD_CLUSTERED
                {
                    float4 reflections = (float4)0;
                    float4 irradiance = (float4)0;

                    g_skyVisibility = EvaluateSkyVisibility(skyVisibilityCapture, SkyVisibilityTexture, P, N);

                    EvaluateEnvProbes(
                        positionVS.xyz, P,
                        N, V, R,
                        camera.near, camera.far,
                        roughness, perceptualRoughness,
                        screenUV, camera.dimensions.xy,
                        input.object_mask,
                        /* inout */ reflections,
                        /* inout */ irradiance);

                    reflections.a = saturate(reflections.a);
                    irradiance.a = saturate(irradiance.a);

#ifdef RT_GI
                    // ddgi alpha fades out past the last cascade, falling back to probe and sky irradiance
                    const float4 ddgiIrradiance = DDGISampleIrradiance(P, N, V);
                    irradiance = lerp(irradiance, ddgiIrradiance, ddgiIrradiance.a);
#endif // RT_GI

                    Fd = diffuseColor * irradiance.rgb * (1.0 - E) * ao;

                    const float3 specular_ao = (float3) SpecularAO_Lagarde(NdotV, ao, perceptualRoughness);
                    Fr = E * (reflections.rgb * specular_ao * energy_compensation) * reflections.a;
                }
#endif // FORWARD_CLUSTERED

                Ft *= refractionTransmission * (1.0 - metalness);

                indirect_lighting = Ft + Fd + Fr;
            }

    #ifdef FORWARD_CLUSTERED
            const uint2 pixelCoord = uint2(screenUV * max(0, int2(camera.dimensions.xy) - 1));

            const float viewSpaceZ = positionVS.z;

            // Clustered shading
            const uint gridIndex = Cluster_GetGridIndex(
                camera.dimensions.xy, pixelCoord,
                viewSpaceZ,
                camera.near, camera.far);

            const uint2 clusterData = ClusterGridBuffer.Load2(gridIndex * sizeof(uint2));

            const uint clusterIndexOffset = clusterData.x;

            const uint numLights = (clusterData.y & 0xFFFF);
            const uint numEnvProbes = (clusterData.y >> 16) & 0xFFFF;

            for (uint i = 0; i < numLights; ++i)
            {
                const uint lightIndex = Cluster_LoadLightIndex(clusterIndexOffset, i);

                Light currentLight = LightsBuffer.Load(lightIndex);

                float3 L = currentLight.position_intensity.xyz;
                L -= P * float(min(currentLight.type, 1));

                L = normalize(L);

                const float3 H = normalize(L + V);

                const float NdotL = max(0.000001, dot(N, L));
                const float LdotH = max(0.000001, dot(L, H));
                const float NdotH = max(0.000001, dot(N, H));
                const float HdotV = max(0.000001, dot(H, V));

                float3 light_color = currentLight.color.rgb;

                float attenuation = 1.0;
                float shadow = 1.0;

                const float D = CalculateDistributionTerm(perceptualRoughness, NdotH);
                const float G = V_SmithGGXCorrelated(roughness * roughness, NdotV, NdotL);
                const float3 F = CalculateFresnelTerm(F0, LdotH);

                const float3 specular_lobe = D * G * F;

                if (currentLight.type == HYP_LIGHT_TYPE_POINT || currentLight.type == HYP_LIGHT_TYPE_SPOT)
                {
                    const float2 radiusFalloff = float2(f16tof32(currentLight.radiusFalloffPacked), f16tof32(currentLight.radiusFalloffPacked >> 16));
                    const float radius = radiusFalloff.x;
                    const float falloff = radiusFalloff.y;

                    attenuation = GetSquareFalloffAttenuation(P, currentLight.position_intensity.xyz, radius);

                    if (currentLight.type == HYP_LIGHT_TYPE_SPOT)
                    {
                        float theta = max(dot(-L, normalize(currentLight.normal.xyz)), 0.0);
                        float2 spot_angles = currentLight.area_size.xy;

                        attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0])) * step(spot_angles[0], theta);

                        // @TODO Spot shadows - add here when adding to DeferredDirect.hlsl

                    }
                    else
                    {
                        // @TODO Forward clustered needs shadows.
                        if ((currentLight.flags & LF_SHADOW_CASTER) != 0)
                        {
                            //uint shadowMapIndex = GetShadowMapIndexForLight(lightIndex);
                            //ShadowMap shadowMap = shadowMaps[shadowMapIndex];

                            float3 worldToLight = P - currentLight.position_intensity.xyz;

                            //shadow = GetPointShadow(shadowMap, currentLight.flags, worldToLight, NdotL);
                        }
                    }
                }

                const float3 diffuse_lobe = diffuseColor * HYP_FMATH_ONE_OVER_PI;

                const float3 direct_component = EvaluateDirectLight(surface, N, V, L, NdotL, diffuse_lobe, specular_lobe, energy_compensation);

                direct_lighting += direct_component * (light_color * ao * shadow * currentLight.position_intensity.w * attenuation);
            }
    #endif // CLUSTERED

    #ifdef FORWARD_SHADING
            // Bounded light list from ForwardShadingConstants; only directional lights are consumed here,
            // point/spot are already covered (unbounded, tile-culled) by the cluster loop above.
            for (uint fsLightIdx = 0; fsLightIdx < min(numBoundLights, MAX_LIGHTS); fsLightIdx++)
            {
                Light currentLight = lights[fsLightIdx];

                if (currentLight.type != HYP_LIGHT_TYPE_DIRECTIONAL)
                {
                    continue;
                }

                const float3 L = normalize(currentLight.position_intensity.xyz);
                const float3 H = normalize(L + V);

                const float NdotL = max(0.000001, dot(N, L));
                const float LdotH = max(0.000001, dot(L, H));
                const float NdotH = max(0.000001, dot(N, H));

                float shadow = 1.0;

                if ((currentLight.flags & LF_SHADOW_CASTER) != 0 && fsLightIdx == directionalCSMLightIndex)
                {
                    shadow = GetDirectionalCSMShadow(P, N, NdotL);
                }

                const float D = CalculateDistributionTerm(perceptualRoughness, NdotH);
                const float G = V_SmithGGXCorrelated(roughness * roughness, NdotV, NdotL);
                const float3 F = CalculateFresnelTerm(F0, LdotH);

                const float3 specular_lobe = D * G * F;
                const float3 diffuse_lobe = diffuseColor * HYP_FMATH_ONE_OVER_PI;

                const float3 direct_component = EvaluateDirectLight(surface, N, V, L, NdotL, diffuse_lobe, specular_lobe, energy_compensation);

                direct_lighting += direct_component * (currentLight.color.rgb * currentLight.atmosphere_tint.rgb * ao * shadow * currentLight.position_intensity.w);
            }
    #endif // FORWARD_SHADING

            result.rgb = indirect_lighting + direct_lighting;
        }
    }

    result.rgb += GET_MATERIAL_EMISSIVE(CURRENT_MATERIAL);

#ifdef FORWARD_CLUSTERED
    result.rgb = ApplyVolumetricFogSample(
        result.rgb,
        SampleVolumetricFog(VolumetricFogTexture, sampler_linear, volumetricFogGrid, camera.viewProjMat, P));
#endif // FORWARD_CLUSTERED

    // premultiplied and additive blends take the source as-is, so fade it by coverage here
    if (GET_MATERIAL_PARAM_BIT(CURRENT_MATERIAL, MATERIAL_FLAG_PREMULTIPLIED_ALPHA))
    {
        result.rgb *= result.a;
    }

    return result;
}
#endif // SHADING_TYPE_FORWARD

PSOutput WriteGeometryOutput(PSInput input, GeometrySurface surface)
{
    PSOutput output;

#if SHADING_TYPE_FORWARD
    output.gbuffer_albedo = ShadeForward(input, surface);
#else
    // gbuffer albedo alpha carries material ambient occlusion, which only attenuates indirect light.
    // The deferred path never consumed the opacity that was written here.
    output.gbuffer_albedo = float4(surface.albedo.rgb, surface.ao);
#endif // SHADING_TYPE_FORWARD

    float roughness = surface.perceptualRoughness * surface.perceptualRoughness;

#ifdef DEBUG_RAW_REFLECTIONS
    roughness = 0.0001;
#endif

    // https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/
    // see: "Motion Vectors" section
    // uv y runs opposite to ndc y, and consumers reproject with uv - velocity
    const float2 velocity = ((input.position_ndc.xy / input.position_ndc.w) - (input.previous_position_ndc.xy / input.previous_position_ndc.w)) * float2(0.5, -0.5);

    uint mask = input.object_mask;

#ifdef ALPHA_DISCARD
    mask |= OBJECT_MASK_CUTOUT;
#endif // ALPHA_DISCARD

    GBufferMaterialParams materialParams;
    materialParams.roughness = roughness;
    materialParams.metalness = surface.metalness;
    materialParams.mask = mask;

    output.gbuffer_normals = GBufferPackNormal(surface.N);

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);

    output.gbuffer_normals.x = roughnessAndMetalPacked;

#ifdef SHADING_TYPE_LIGHTMAPPED
    // atlas UV, 14 bits per channel (0-16383)
    output.gbuffer_material = min((uint)round(saturate(input.texcoord1.x) * 16384.0), 16383u)
        | (min((uint)round(saturate(input.texcoord1.y) * 16384.0), 16383u) << 14u);
#else
    output.gbuffer_material = 0u;

#ifndef SHADING_TYPE_FORWARD
    output.gbuffer_material |= GBufferPackEmissive(GET_MATERIAL_EMISSIVE(CURRENT_MATERIAL));
#endif // !SHADING_TYPE_FORWARD
#endif // SHADING_TYPE_LIGHTMAPPED

    output.gbuffer_material |= GBufferPackObjectMask(mask);

    output.gbuffer_velocity = velocity;

    return output;
}

#endif // HYP_GEOMETRY_PASS_SHADING_HLSLI
