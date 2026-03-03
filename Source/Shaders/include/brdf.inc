#ifndef HYP_BRDF_GLSL
#define HYP_BRDF_GLSL

float GetSquareFalloffAttenuation(vec3 P, vec3 L, float radius)
{
    const vec3 position_to_light = P - L;

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
vec3 ClosestPointOnSegment(vec3 a, vec3 b, vec3 c)
{
    vec3 ab = b - a;
    float t = dot(c - a, ab) / dot(ab, ab);

    return a + clamp(t, 0.0, 1.0) * ab;
}

float RectangleSolidAngle(vec3 world_position, vec3 p0, vec3 p1, vec3 p2, vec3 p3)
{
    vec3 v0 = p0 - world_position;
    vec3 v1 = p1 - world_position;
    vec3 v2 = p2 - world_position;
    vec3 v3 = p3 - world_position;

    vec3 n0 = normalize(cross(v0, v1));
    vec3 n1 = normalize(cross(v1, v2));
    vec3 n2 = normalize(cross(v2, v3));
    vec3 n3 = normalize(cross(v3, v0));

    float g0 = acos(dot(-n0, n1));
    float g1 = acos(dot(-n1, n2));
    float g2 = acos(dot(-n2, n3));
    float g3 = acos(dot(-n3, n0));

    return g0 + g1 + g2 + g3 - 2.0 * HYP_FMATH_PI;
}

vec3 ImportanceSampleGTR2(float rgh, float r1, float r2)
{
    float a = max(0.001, rgh);

    float phi = r1 * HYP_FMATH_PI * 2.0;

    float cosTheta = sqrt((1.0 - r2) / (1.0 + (a * a - 1.0) * r2));
    float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
    float sinPhi   = sin(phi);
    float cosPhi   = cos(phi);

    return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

float V_SmithGGXCorrelated(float roughness, float NoV, float NoL)
{
    // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
    float a2 = roughness * roughness;
    // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
    float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    float v = 0.5 / max(lambdaV + lambdaL, 0.00001);
    // a2=0 => v = 1 / 4*NoL*NoV   => min=1/4, max=+inf
    // a2=1 => v = 1 / 2*(NoL+NoV) => min=1/4, max=+inf
    // clamp to the maximum value representable in mediump
    return v;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    // float a = roughness*roughness;
    // float a2 = a*a;
    // float NdotH = max(dot(N, H), 0.0);
    // float NdotH2 = NdotH*NdotH;

    // float nom   = a2;
    // float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    // denom = HYP_FMATH_PI * denom * denom;

    // return nom / denom;


    // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"

    // In mediump, there are two problems computing 1.0 - NoH^2
    // 1) 1.0 - NoH^2 suffers floating point cancellation when NoH^2 is close to 1 (highlights)
    // 2) NoH doesn't have enough precision around 1.0
    // Both problem can be fixed by computing 1-NoH^2 in highp and providing NoH in highp as well

    // However, we can do better using Lagrange's identity:
    //      ||a x b||^2 = ||a||^2 ||b||^2 - (a . b)^2
    // since N and H are unit vectors: ||N x H||^2 = 1.0 - NoH^2
    // This computes 1.0 - NoH^2 directly (which is close to zero in the highlights and has
    // enough precision).
    // Overall this yields better performance, keeping all computations in mediump
//     const float NoH = dot(N, H);
// #if defined(TARGET_MOBILE)
//     vec3 NxH = cross(N, H);
//     float oneMinusNoHSquared = dot(NxH, NxH);
// #else
//     float oneMinusNoHSquared = 1.0 - NoH * NoH;
// #endif

//     float a = NoH * roughness;
//     float k = roughness / (oneMinusNoHSquared + a * a);
//     float d = k * k * HYP_FMATH_ONE_OVER_PI;
//     return d;


    float NdotH = max(dot(N, H), 0.0);

    const float r2 = HYP_FMATH_SQR(roughness);
    const float d = (NdotH * r2 - NdotH) * NdotH + 1.0;
    return r2 / (d * d * HYP_FMATH_PI);
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (HYP_FMATH_PI * d * d);
}

float GGX_PDF(float NdotH, float HdotV, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float D = a2 / (HYP_FMATH_PI * pow(NdotH * NdotH * (a2 - 1.0) + 1.0, 2.0));
    return (D * NdotH) / (4.0 * HdotV);
}

vec3 SampleGGX(vec2 xi, float roughness, vec3 N)
{
    float a = roughness * roughness;

    // Sample in polar coordinates
    float phi = 2.0 * HYP_FMATH_PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Convert to cartesian
    vec3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

    // Transform to world space
    vec3 T1, T2;
    ComputeOrthonormalBasis(N, T1, T2);
    return normalize(T1 * H.x + T2 * H.y + N * H.z);
}

float G_SchlickGGX(float NdotX, float k)
{
    return NdotX / (NdotX * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    float a = roughness;
    float k = (a + 1.0);
    k = (k * k) * 0.125; // (a+1)^2 / 8
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
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

vec3 F_Schlick(vec3 f0, float cosTheta)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

vec3 SchlickFresnel(vec3 f0, vec3 f90, float u)
{
    //const float Fc = pow(1 - u, 5.0);
    //return f0 * (1.0 - Fc) + f90 * Fc;

    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

vec4 SchlickFresnel(vec4 f0, vec4 f90, float u)
{
    const float Fc = pow(1 - u, 5.0);
    return f0 * (1.0 - Fc) + f90 * Fc;

    // return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

vec4 SchlickFresnelRoughness(vec4 f0, float roughness, float u)
{
    // const float Fc = pow(1 - u, 5.0);
    // return f0 * (1.0 - Fc) + f90 * Fc;

    return f0 + (max(vec4((1.0 - roughness).xxxx), f0) - f0) * pow(1.0 - u, 5.0);
}

vec3 SchlickFresnelRoughness(vec3 f0, float roughness, float u)
{
    // const float Fc = pow(1 - u, 5.0);
    // return f0 * (1.0 - Fc) + f90 * Fc;

    return f0 + (max(vec3((1.0 - roughness).xxx), f0) - f0) * pow(1.0 - u, 5.0);
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

vec2 BRDFMap(float roughness, float NdotV)
{
    // Same as EnvBRDFApprox( 0.04, Roughness, NoV )
    // const vec2 c0 = { -1.0, -0.0275 };
    // const vec2 c1 = { 1.0, 0.0425 };
    // vec2 r = roughness * c0 + c1;
    // float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    // return vec2( -1.04, 1.04 ) * a004;

    const vec4 c0 = vec4( -1, -0.0275, -0.572, 0.022 );
    const vec4 c1 = vec4( 1, 0.0425, 1.04, -0.04 );
    vec4 r = roughness * c0 + c1;
    float a004 = min( r.x * r.x, exp2( -9.28 * NdotV ) ) * r.x + r.y;
    return vec2( -1.04, 1.04 ) * a004 + r.zw;
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

float SpecularAO_Cones(vec3 bentNormal, float visibility, float roughness, vec3 shading_reflected)
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

vec3 GTAOMultiBounce(float visibility, const vec3 albedo) {
    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"
    vec3 a =  2.0404 * albedo - 0.3324;
    vec3 b = -4.7951 * albedo + 0.6417;
    vec3 c =  2.7552 * albedo + 0.6903;

    return max(vec3(visibility.xxx), ((visibility * a + b) * visibility + c) * visibility);
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
      
vec3 CosWeightedRandomHemisphereDirectionHammersley(uint sample_index, uint num_samples, vec3 n)
{
    float x = float(sample_index) / float(num_samples);

    uint i = sample_index;
    i = (i << 16u) | (i >> 16u);
    i = ((i & 0x55555555u) << 1u) | ((i & 0xAAAAAAAAu) >> 1u);
    i = ((i & 0x33333333u) << 2u) | ((i & 0xCCCCCCCCu) >> 2u);
    i = ((i & 0x0F0F0F0Fu) << 4u) | ((i & 0xF0F0F0F0u) >> 4u);
    i = ((i & 0x00FF00FFu) << 8u) | ((i & 0xFF00FF00u) >> 8u);

    vec2 r = vec2(x, (float(i) * 2.32830643653086963e-10 * 6.2831));
    vec3 uu = normalize(cross(n, vec3(1.0,1.0,0.0))), vv = cross(uu, n);

    float sqrtx = sqrt(r.x);

    return normalize(vec3(sqrtx * cos(r.y) * uu + sqrtx * sin(r.y) * vv + sqrt(1.0 - r.x) * n));
}

// Hammersley sequence
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), VanDerCorpus(i + 1u));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
	
    float phi = 2.0 * HYP_FMATH_PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha2 - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    return H;
}



float ApplyIORToRoughness(float ior, float roughness)
{
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}

vec3 CalculateDiffuseColor(vec3 albedo, float metalness)
{
    return albedo * (1.0 - metalness);
}

vec4 CalculateDiffuseColor(vec4 albedo, float metalness)
{
    return vec4(CalculateDiffuseColor(albedo.rgb, metalness), albedo.a);
}

vec3 CalculateF0(vec3 albedo, float metalness)
{
    const float IOR = 1.5;
    vec3 F0 = vec3((pow(IOR - 1.0, 2.0) / pow(IOR + 1.0, 2.0)).xxx);
    F0 = lerp(F0, albedo, metalness);
    return F0;
}

vec3 CalculateFresnelTerm(vec3 F0, float roughness, float NdotV)
{
    return SchlickFresnelRoughness(F0, roughness, NdotV);
}

vec4 CalculateFresnelTerm(vec4 F0, float roughness, float NdotV)
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

vec3 CalculateDFG(vec3 F, float perceptual_roughness, float NdotV)
{
    const vec2 AB = BRDFMap(perceptual_roughness, NdotV);

    return F * AB.x + AB.y;
}

vec4 CalculateDFG(vec4 F, float perceptual_roughness, float NdotV)
{
    const vec2 AB = BRDFMap(perceptual_roughness, NdotV);

    return F * AB.x + AB.y;
}

vec3 CalculateE(vec3 F0, vec3 dfg)
{
    return lerp(dfg.xxx, dfg.yyy, F0);
}

vec4 CalculateE(vec4 F0, vec4 dfg)
{
    return lerp(dfg.xxxx, dfg.yyyy, F0);
}

vec3 CalculateEnergyCompensation(vec3 F0, vec3 dfg)
{
    return 1.0 + F0 * ((1.0 / max(dfg.y, 0.0001)) - 1.0);
}

vec4 CalculateEnergyCompensation(vec4 F0, vec4 dfg)
{
    return 1.0 + F0 * ((1.0 / max(dfg.y, 0.0001)) - 1.0);
}

vec3 SampleCosineWeightedHemisphere(vec2 Xi)
{
    float r = sqrt(Xi.x);
    float phi = 6.28318530718 * Xi.y;

    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(1.0 - r * r);

    return vec3(x, y, z);
}

vec3 SampleCosineDir(in vec2 xi, in vec3 N)
{
    vec3 dir = SampleCosineWeightedHemisphere(xi);

    vec3 T1, T2;
    ComputeOrthonormalBasis(N, T1, T2);

    return normalize(T1 * dir.x + T2 * dir.y + N * dir.z);
}

#endif