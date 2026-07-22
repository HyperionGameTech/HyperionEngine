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
    float2 ndc : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.ndc = input.a_position.xy;
    output.position_cs = float4(input.a_position.xy, 0.0, 1.0);

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

struct PSOutput
{
    float4 output_color : SV_Target0;
    float output_depth : SV_Depth;
};

#include "../include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(EditorGrid, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

float3 UnprojectPoint(float2 ndc, float z)
{
    float4 viewSpace = mul(camera.invProjMat, float4(ndc, z, 1.0));
    viewSpace /= viewSpace.w;

    float4 worldSpace = mul(camera.invViewMat, viewSpace);

    return worldSpace.xyz;
}

float4 GridColor(float2 coord, float scale, float lineWidth, float3 baseColor)
{
    float2 gridCoord = coord / scale;
    float2 derivative = fwidth(gridCoord);

    float2 lines = abs(frac(gridCoord - 0.5) - 0.5) / max(derivative, 1e-6);
    float lineIntensity = 1.0 - saturate(min(lines.x, lines.y) - (lineWidth - 1.0));

    return float4(baseColor, lineIntensity);
}

PSOutput PSMain(PSInput i)
{
    float3 rayOrigin = UnprojectPoint(i.ndc, 0.0);
    float3 rayEnd = UnprojectPoint(i.ndc, 1.0);
    float3 rayDir = rayEnd - rayOrigin;

    PSOutput output;
    output.output_color = float4(0.0, 0.0, 0.0, 0.0);
    output.output_depth = 1.0;

    if (abs(rayDir.y) < 1e-6)
    {
        discard;
    }

    float t = -rayOrigin.y / rayDir.y;

    if (t <= 0.0 || t >= 1.0)
    {
        discard;
    }

    float3 hitPos = rayOrigin + rayDir * t;

    float distanceToCamera = length(hitPos - camera.position.xyz);
    float fade = saturate(1.0 - (distanceToCamera / camera.far));

    if (fade <= 0.0)
    {
        discard;
    }

    float4 minorGrid = GridColor(hitPos.xz, 1.0, 1.0, float3(0.5, 0.5, 0.5));
    float4 majorGrid = GridColor(hitPos.xz, 10.0, 1.5, float3(0.75, 0.75, 0.75));

    float4 gridColor = lerp(minorGrid, majorGrid, majorGrid.a);
    gridColor.a *= fade * fade;

    float2 axisDerivative = fwidth(hitPos.xz);
    
    float xAxis = 1.0 - saturate(abs(hitPos.z) / max(axisDerivative.y, 1e-6) - 1.0);
    float zAxis = 1.0 - saturate(abs(hitPos.x) / max(axisDerivative.x, 1e-6) - 1.0);

    gridColor.rgb = lerp(gridColor.rgb, float3(0.6, 0.1, 0.1), xAxis * fade);
    gridColor.rgb = lerp(gridColor.rgb, float3(0.1, 0.1, 0.6), zAxis * fade);
    gridColor.a = max(gridColor.a, max(xAxis, zAxis) * fade);

    if (gridColor.a <= 0.0)
    {
        discard;
    }

    float4 clipPos = mul(camera.viewProjMat, float4(hitPos, 1.0));

    output.output_color = gridColor;
    output.output_depth = clipPos.z / clipPos.w;

    return output;
}

#endif // PIXEL_SHADER
