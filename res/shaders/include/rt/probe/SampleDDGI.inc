#ifndef HYP_SAMPLE_DDGI
#define HYP_SAMPLE_DDGI

#include "./probe_uniforms.inc"
#include "./shared.inc"

vec4 DDGISampleIrradiance(vec3 P, vec3 N, vec3 V)
{
    const ivec3 base_grid_coord = BaseGridCoord(P);
    const vec3 base_probe_position = GridPositionToWorldPosition(base_grid_coord);
    
    vec3 total_irradiance = vec3(0.0, 0.0, 0.0);
    float total_weight = 0.0;
    
    vec3 alpha = clamp((P - base_probe_position) / PROBE_GRID_STEP, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    
    for (int i = 0; i < 8; i++)
    {
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1, 1, 1);
        ivec3 probe_grid_coord = clamp(base_grid_coord + offset, ivec3(0, 0, 0), ivec3(ddgiConstants.probe_counts.xyz) - ivec3(1, 1, 1));
        
        int probe_index = GridPositionToProbeIndex(probe_grid_coord);
        vec3 probe_position = GridPositionToWorldPosition(probe_grid_coord);
        vec3 probe_to_point = P - probe_position + (N + 3.0 * V) * PROBE_NORMAL_BIAS;
        vec3 dir = normalize(-probe_to_point);

        vec3 trilinear = lerp(vec3(1.0, 1.0, 1.0) - alpha, alpha, vec3(offset));
        float weight = 1.0;
        
        /* Backface test */
        
        vec3 true_direction_to_probe = normalize(probe_position - P);
        weight *= HYP_FMATH_SQR(max(0.0001, (dot(true_direction_to_probe, N) + 1.0) * 0.5)) + 0.2;
        
        /* Visibility test */
        vec2 depth_texcoord = TextureCoordFromDirection(-dir, probe_index, ddgiConstants.probe_counts.xyz, ddgiConstants.image_dimensions.zw, PROBE_SIDE_LENGTH_DEPTH);
        float distance_to_probe = length(probe_to_point);

        vec2 depth_sample = SAMPLE_TEXTURE_2D_LOD(gbuffer_sampler, probe_depth, depth_texcoord, 0.0).rg;
        
        float mean = depth_sample.x;
        float variance = abs(HYP_FMATH_SQR(mean) - depth_sample.y);

        float chebyshev = variance / (variance + HYP_FMATH_SQR(max(distance_to_probe - mean, 0.0)));
        chebyshev = max(HYP_FMATH_CUBE(chebyshev), 0.0);
        weight *= (distance_to_probe <= mean) ? 1.0 : chebyshev;
        weight = max(0.0001, weight);

        vec3 irradiance_dir = N;
        vec2 irradiance_texcoord = TextureCoordFromDirection(normalize(irradiance_dir), probe_index, ddgiConstants.probe_counts.xyz, ddgiConstants.image_dimensions.xy, PROBE_SIDE_LENGTH_IRRADIANCE);
        vec3 irradiance = SAMPLE_TEXTURE_2D_LOD(gbuffer_sampler, probe_irradiance, irradiance_texcoord, 0.0).rgb;
    
        const float crush_threshold = 0.2;
        if (weight < crush_threshold) {
            weight *= weight * weight * (1.0 / HYP_FMATH_SQR(crush_threshold));
        }

        // trilinear
        weight *= trilinear.x * trilinear.y * trilinear.z;

        irradiance = sqrt(irradiance);

        total_irradiance += irradiance * weight;
        total_weight += weight;
    }

    vec3 net_irradiance = total_irradiance / max(total_weight, 0.001);
    net_irradiance = HYP_FMATH_SQR(net_irradiance);

    return vec4(net_irradiance, 1.0);
}

#endif