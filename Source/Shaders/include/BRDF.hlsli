#ifndef BRDF_HLSLI
#define BRDF_HLSLI

float GetSquareFalloffAttenuation(float3 P, float3 L, float radius)
{
    const float3 position_to_light = P - L;

    const float dist = length(position_to_light);
    const float distance_square = dot(position_to_light, position_to_light);
    // const float radius_square = radius * radius;

    // return 2.0 / (distance_square + radius_square + (dist * sqrt(distance_square + radius_square)));

    float inv_radius = 1.0 / radius;
    float factor = distance_square * HYP_FMATH_SQR(inv_radius);
    float smooth_factor = max(1.0 - HYP_FMATH_SQR(factor), 0.0);
    return HYP_FMATH_SQR(smooth_factor) / max(distance_square, 1e-4);
}

// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
float RightPyramidSolidAngle(float dist, float half_width, float half_height)
{
    float a = half_width;
    float b = half_height;
    float h = dist;
    
    return 4.0 * asin(a * b / sqrt((a * a + h * h) * (b * b + h * h)));
}

// https://github.com/turanszkij/WickedEngine/blob/62d1d02691286cc6c25da61294bfb416d018782b/WickedEngine/lightingHF.hlsli#L251C1-L257C2
float3 ClosestPointOnSegment(float3 a, float3 b, float3 c)
{
    float3 ab = b - a;
    float t = dot(c - a, ab) / dot(ab, ab);

    return a + clamp(t, 0.0, 1.0) * ab;
}

float RectangleSolidAngle(float3 world_position, float3 p0, float3 p1, float3 p2, float3 p3)
{
    float3 v0 = p0 - world_position;
    float3 v1 = p1 - world_position;
    float3 v2 = p2 - world_position;
    float3 v3 = p3 - world_position;

    float3 n0 = normalize(cross(v0, v1));
    float3 n1 = normalize(cross(v1, v2));
    float3 n2 = normalize(cross(v2, v3));
    float3 n3 = normalize(cross(v3, v0));

    float g0 = acos(dot(-n0, n1));
    float g1 = acos(dot(-n1, n2));
    float g2 = acos(dot(-n2, n3));
    float g3 = acos(dot(-n3, n0));

    return g0 + g1 + g2 + g3 - 2.0 * HYP_FMATH_PI;
}

float3 ImportanceSampleGTR2(float rgh, float r1, float r2)
{
    float a = max(0.001, rgh);

    float phi = r1 * HYP_FMATH_PI * 2.0;

    float cosTheta = sqrt((1.0 - r2) / (1.0 + (a * a - 1.0) * r2));
    float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
    float sinPhi   = sin(phi);
    float cosPhi   = cos(phi);

    return float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

float V_SmithGGXCorrelated(float roughness, float NoV, float NoL)
{
    // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
    float a2 = roughness;
    // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
    float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    float v = 0.5 / max(lambdaV + lambdaL, 0.00001);
    // a2=0 => v = 1 / 4*NoL*NoV   => min=1/4, max=+inf
    // a2=1 => v = 1 / 2*(NoL+NoV) => min=1/4, max=+inf
    // clamp to the maximum value representable in mediump
    return v;
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (HYP_FMATH_PI * denom * denom);
}

float GGX_PDF(float NdotH, float HdotV, float roughness)
{
    float D = DistributionGGX(NdotH, roughness);
    return D * NdotH / (4.0 * HdotV);
}

float3 SampleGGX(float2 xi, float roughness, float3 N)
{
    float a = roughness * roughness;

    // Sample in polar coordinates
    float phi = 2.0 * HYP_FMATH_PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Convert to cartesian
    float3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

    // Transform to world space
    float3 T1, T2;
    ComputeOrthonormalBasis(N, T1, T2);
    return normalize(T1 * H.x + T2 * H.y + N * H.z);
}

float G_SchlickGGX(float NdotX, float k)
{
    return NdotX / (NdotX * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    float a = (roughness * roughness) / 2.0;
    return (NdotV / (NdotV * (1.0 - a) + a)) * (NdotL / (NdotL * (1.0 - a) + a));
}

float Trowbridge(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH2 = NdotH * NdotH;
    float denominator = HYP_FMATH_PI * pow((alpha2 - 1) * NdotH2 + 1, 2);
    return alpha2 / denominator;
}

float CookTorranceG(float NdotL, float NdotV, float HdotV, float NdotH)
{
    float first = 2 * NdotH * NdotV / HdotV;
    float second = 2 * NdotH * NdotL / HdotV;
    return min(1, min(first, second));
    //return min(1, 2 * (NdotH / LdotH) * min(NdotL, NdotV));
}

float3 F_Schlick(float3 f0, float cosTheta)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

float3 SchlickFresnel(float3 f0, float3 f90, float u)
{
    //const float Fc = pow(1 - u, 5.0);
    //return f0 * (1.0 - Fc) + f90 * Fc;

    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

float4 SchlickFresnel(float4 f0, float4 f90, float u)
{
    const float Fc = pow(1 - u, 5.0);
    return f0 * (1.0 - Fc) + f90 * Fc;

    // return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

float4 SchlickFresnelRoughness(float4 f0, float roughness, float u)
{
    // const float Fc = pow(1 - u, 5.0);
    // return f0 * (1.0 - Fc) + f90 * Fc;

    return f0 + (max(float4((1.0 - roughness).xxxx), f0) - f0) * pow(1.0 - u, 5.0);
}

float3 SchlickFresnelRoughness(float3 f0, float roughness, float u)
{
    // const float Fc = pow(1 - u, 5.0);
    // return f0 * (1.0 - Fc) + f90 * Fc;

    return f0 + (max(float3((1.0 - roughness).xxx), f0) - f0) * pow(1.0 - u, 5.0);
}

float3 mon2lin(float3 x)
{
    return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

/// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
float2 BRDFMap(float roughness, float NdotV)
{
    const float4 c0 = { -1.0, -0.0275, -0.572,  0.022 };
    const float4 c1 = {  1.0,  0.0425,  1.04,  -0.04  };
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    return float2( -1.04, 1.04 ) * a004 + r.zw;
}

float sqr(float x)
{
    return x * x;
}

/* From Google Filament */

/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid in the range -1..1.
 */
float acosFast(float x)
{
    // Lagarde 2014, "Inverse trigonometric functions GPU optimization for AMD GCN architecture"
    // This is the approximation of degree 1, with a max absolute error of 9.0x10^-3
    float y = abs(x);
    float p = -0.1565827 * y + 1.570796;
    p *= sqrt(1.0 - y);
    return x >= 0.0 ? p : HYP_FMATH_PI - p;
}

/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid only in the range 0..1.
 */
float acosFastPositive(float x)
{
    float p = -0.1565827 * x + 1.570796;
    return p * sqrt(1.0 - x);
}

float sphericalCapsIntersection(float cosCap1, float cosCap2, float cosDistance)
{
    // Oat and Sander 2007, "Ambient Aperture Lighting"
    // Approximation mentioned by Jimenez et al. 2016
    float r1 = acosFastPositive(cosCap1);
    float r2 = acosFastPositive(cosCap2);
    float d  = acosFast(cosDistance);

    // We work with cosine angles, replace the original paper's use of
    // cos(min(r1, r2)) with max(cosCap1, cosCap2)
    // We also remove a multiplication by 2 * PI to simplify the computation
    // since we divide by 2 * PI in computeBentSpecularAO()

    if (min(r1, r2) <= max(r1, r2) - d) {
        return 1.0 - max(cosCap1, cosCap2);
    } else if (r1 + r2 <= d) {
        return 0.0;
    }

    float delta = abs(r1 - r2);
    float x = 1.0 - clamp((d - delta) / max(r1 + r2 - delta, 1e-4), 0.0, 1.0);
    // simplified smoothstep()
    float area = sqr(x) * (-2.0 * x + 3.0);
    return area * (1.0 - max(cosCap1, cosCap2));
}

#ifdef PIXEL_SHADER
/// Compute perceptual roughness a la filament
float ComputePerceptualRoughness(float roughness, float3 normal)
{
    float perceptualRoughness = roughness;

    float3 dndx = ddx(normal);
    float3 dndy = ddy(normal);
    
    float variance = max(dot(dndx, dndx), dot(dndy, dndy));
    
    float geometricRoughness = pow(saturate(variance), 0.333f);
    
    perceptualRoughness = max(perceptualRoughness, geometricRoughness);

    const float minPerceptualRoughness = 0.089f;
    perceptualRoughness = clamp(perceptualRoughness, minPerceptualRoughness, 1.0f);

    return perceptualRoughness;
}
#endif

float SpecularAO_Cones(float3 bentNormal, float visibility, float roughness, float3 shading_reflected)
{
    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"

    // aperture from ambient occlusion
    float cosAv = sqrt(1.0 - visibility);
    // aperture from roughness, log(10) / log(2) = 3.321928
    float cosAs = exp2(-3.321928 * sqr(roughness));
    // angle betwen bent normal and reflection direction
    float cosB  = dot(bentNormal, shading_reflected);

    // Remove the 2 * PI term from the denominator, it cancels out the same term from
    // sphericalCapsIntersection()
    float ao = sphericalCapsIntersection(cosAv, cosAs, cosB) / (1.0 - cosAs);
    // Smoothly kill specular AO when entering the perceptual roughness range [0.1..0.3]
    // Without this, specular AO can remove all reflections, which looks bad on metals
    return lerp(1.0, ao, smoothstep(0.01, 0.09, roughness));
}

float SpecularAO_Lagarde(float NoV, float visibility, float roughness)
{
    // Lagarde and de Rousiers 2014, "Moving Frostbite to PBR"
    return clamp(pow(NoV + visibility, exp2(-16.0 * roughness - 1.0)) - 1.0 + visibility, 0.0, 1.0);
}

float3 GTAOMultiBounce(float visibility, const float3 albedo) {
    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"
    float3 a =  2.0404 * albedo - 0.3324;
    float3 b = -4.7951 * albedo + 0.6417;
    float3 c =  2.7552 * albedo + 0.6903;

    return max(float3(visibility.xxx), ((visibility * a + b) * visibility + c) * visibility);
}

float VanDerCorpus(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
      
float3 CosWeightedRandomHemisphereDirectionHammersley(uint sample_index, uint num_samples, float3 n)
{
    float x = float(sample_index) / float(num_samples);

    uint i = sample_index;
    i = (i << 16u) | (i >> 16u);
    i = ((i & 0x55555555u) << 1u) | ((i & 0xAAAAAAAAu) >> 1u);
    i = ((i & 0x33333333u) << 2u) | ((i & 0xCCCCCCCCu) >> 2u);
    i = ((i & 0x0F0F0F0Fu) << 4u) | ((i & 0xF0F0F0F0u) >> 4u);
    i = ((i & 0x00FF00FFu) << 8u) | ((i & 0xFF00FF00u) >> 8u);

    float2 r = float2(x, (float(i) * 2.32830643653086963e-10 * 6.2831));
    float3 uu = normalize(cross(n, float3(1.0,1.0,0.0))), vv = cross(uu, n);

    float sqrtx = sqrt(r.x);

    return normalize(float3(sqrtx * cos(r.y) * uu + sqrtx * sin(r.y) * vv + sqrt(1.0 - r.x) * n));
}

// Hammersley sequence
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), VanDerCorpus(i + 1u));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
	
    float phi = 2.0 * HYP_FMATH_PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha2 - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    return H;
}



float ApplyIORToRoughness(float ior, float roughness)
{
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}

float3 CalculateDiffuseColor(float3 albedo, float metalness)
{
    return albedo * (1.0 - metalness);
}

float4 CalculateDiffuseColor(float4 albedo, float metalness)
{
    return float4(CalculateDiffuseColor(albedo.rgb, metalness), albedo.a);
}

float3 CalculateF0(float3 albedo, float metalness)
{
    const float IOR = 1.5;
    float3 F0 = float3((pow(IOR - 1.0, 2.0) / pow(IOR + 1.0, 2.0)).xxx);
    F0 = lerp(F0, albedo, metalness);
    return F0;
}

float3 CalculateFresnelTerm(float3 F0, float roughness, float NdotV)
{
    return SchlickFresnelRoughness(F0, roughness, NdotV);
}

float4 CalculateFresnelTerm(float4 F0, float roughness, float NdotV)
{
    return SchlickFresnelRoughness(F0, roughness, NdotV);
}

float CalculateGeometryTerm(float NdotL, float NdotV, float HdotV, float NdotH)
{
    return CookTorranceG(NdotL, NdotV, HdotV, NdotH);
}

float CalculateDistributionTerm(float roughness, float NdotH)
{
    return Trowbridge(NdotH, roughness);
}

float3 CalculateDFG(float3 F, float roughness, float NdotV)
{
    const float2 AB = BRDFMap(roughness, NdotV);

    return F * AB.x + AB.y;
}

float4 CalculateDFG(float4 F, float roughness, float NdotV)
{
    const float2 AB = BRDFMap(roughness, NdotV);

    return F * AB.x + AB.y;
}

float3 CalculateE(float3 F0, float3 dfg)
{
    return lerp(dfg.xxx, dfg.yyy, F0);
}

float4 CalculateE(float4 F0, float4 dfg)
{
    return lerp(dfg.xxxx, dfg.yyyy, F0);
}

float3 CalculateEnergyCompensation(float3 F0, float3 dfg)
{
    return 1.0 + F0 * ((1.0 / max(dfg.y, 0.0001)) - 1.0);
}

float4 CalculateEnergyCompensation(float4 F0, float4 dfg)
{
    return 1.0 + F0 * ((1.0 / max(dfg.y, 0.0001)) - 1.0);
}

float3 SampleCosineWeightedHemisphere(float2 Xi)
{
    float r = sqrt(Xi.x);
    float phi = 6.28318530718 * Xi.y;

    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(1.0 - r * r);

    return float3(x, y, z);
}

float3 SampleCosineDir(in float2 xi, in float3 N)
{
    float3 dir = SampleCosineWeightedHemisphere(xi);

    float3 T1, T2;
    ComputeOrthonormalBasis(N, T1, T2);

    return normalize(T1 * dir.x + T2 * dir.y + N * dir.z);
}

/// https://www.rorydriscoll.com/2012/01/15/cubemap-texel-solid-angle/
float AreaElement(float x, float y )
{
	return atan2(x * y, sqrt(x * x + y * y + 1));
}

float TexelCoordSolidAngle(int a_FaceIdx, float a_U, float a_V, int a_Size)
{
    //scale up to [-1, 1] range (inclusive), offset by 0.5 to point to texel center.
    float U = (2.0f * ((float)a_U + 0.5f) / (float)a_Size) - 1.0f;
    float V = (2.0f * ((float)a_V + 0.5f) / (float)a_Size) - 1.0f;

    float InvResolution = 1.0 / (float)a_Size;

	// U and V are the -1..1 texture coordinate on the current face.
	// Get projected area for this texel
	float x0 = U - InvResolution;
	float y0 = V - InvResolution;
	float x1 = U + InvResolution;
	float y1 = V + InvResolution;
	float SolidAngle = AreaElement(x0, y0) - AreaElement(x0, y1) - AreaElement(x1, y0) + AreaElement(x1, y1);

	return SolidAngle;
}

#endif // BRDF_HLSLI