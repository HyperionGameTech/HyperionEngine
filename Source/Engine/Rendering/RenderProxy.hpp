/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Mat3f.hpp>
#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Frustum.hpp>

#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class Entity;
class Mesh;
class Light;
class Camera;
class View;
class Texture;
class LightmapVolume;
class ParticleVolume;
class FogVolume;
class Material;
class Skeleton;
class EnvProbe;
class ShadowMap;
class InstancedMeshData;
class Sprite;

enum AtlasTextureType : uint32;

HYP_STRUCT()
struct MeshRayTracingData
{
    HYP_STRUCT_BODY(MeshRayTracingData);

    HYP_FIELD(NoScriptBindings)
    BottomLevelASRef blas;

    ~MeshRayTracingData();
};

struct IRenderProxy
{
    bool forceRebind = false;
};

struct NullProxy final
{
};

struct WorldShaderData
{
    float gameTime;
    uint32 frameCounter;
    uint32 _pad0;
    uint32 _pad1;
};

struct EntityShaderData
{
    Mat4f modelMatrix;
    Mat4f previousModelMatrix;
    Mat3f normalMatrix;

    Vec3f worldAabbMax;
    Vec3f worldAabbMin;

    uint32 entityIndex = ~0u;
    uint32 lightmapVolumeIndex = ~0u;
    uint32 materialIndex = ~0u;
    uint32 skeletonIndex = ~0u;

    uint32 bucket;
    uint32 flags;
    uint32 _pad0;
    uint32 _pad1;

    Vec4f _pad2;
};

static_assert(sizeof(EntityShaderData) % 64 == 0);

enum class LightmapElementId : uint32;

struct InstanceData
{
    static constexpr uint32 MaxBuffers = 5;

    ByteBuffer buffers[MaxBuffers];

    uint32 bufferStructSizes[MaxBuffers] {};
    uint32 bufferStructAlignments[MaxBuffers] {};
};

/*! \brief Proxy for a renderable Entity with a valid Mesh and Material assigned */
struct RenderProxyMesh final : IRenderProxy
{
    Entity* entity = nullptr;

    Mesh* mesh = nullptr;
    Material* material = nullptr;
    Skeleton* skeleton = nullptr;

    uint32 numIndices = 0;
    uint32 numInstances = 0;

    LightmapVolume* lightmapVolume = nullptr;
    LightmapElementId lightmapElementId = LightmapElementId(~0u);

    RenderableAttributeSet attributes;

    InstanceData instanceData;

    EntityShaderData bufferData {};

    uint8 enableAutoInstancing : 1 = false;

    uint8 currentLodIndex = 0;
};

struct EnvProbeShaderData
{
    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4f worldPosition; // w == diffuse strength

    Vec2u dimensions;
    uint32 textureIndex = ~0u;
    uint32 typeAndFlags;

    Vec4f shData[9];
};

static_assert(sizeof(EnvProbeShaderData));

struct RenderProxyEnvProbe : IRenderProxy
{
    EnvProbe* envProbe = nullptr;

    Texture* texture = nullptr;           // baked cubemap texture or prefiltered env
    Texture* visibilityTexture = nullptr; // only relevant if envprobe has HAS_VISIBILITY flag set.
    EnvProbeShaderData bufferData {};
};

struct ShadowMapData
{
    Mat4f viewProjMat;
    Mat4f invProjMat;

    Vec4f aabbMin;         // w = offsetUV.x
    Vec4f aabbMax;         // w = offsetUV.y
    Vec4f dimensionsScale; // xy = shadow map dimensions in pixels, zw = shadow map dimensions relative to the atlas dimensions

    uint32 layerIndex;
    float splitDistance;
    float _pad0;
    float _pad1;
};

struct LightShaderData
{
    uint32 lightType;
    uint32 materialIndex;
    uint32 radiusFalloffPacked;
    uint32 flags;

    Vec4f positionIntensity;
    Vec4f color;

    union
    {
        Vec3f areaNormal;
        Vec3f spotLightDir;
    };

    Vec2f areaSize; // also angles for spot lights
    Vec2f _pad0;
    Vec4f _pad1;
    Vec4f _pad2;
    Vec4f _pad3;
};

static_assert(sizeof(LightShaderData) % 64 == 0);

struct RenderProxyLight : IRenderProxy
{
    Light* light = nullptr;
    Material* lightMaterial = nullptr; // for textured area lights
    Texture* bakedShadowMap = nullptr;
    uint32 numCascades = 0;
    LightShaderData bufferData {};
};

struct LightmapVolumeShaderData
{
    Vec4f aabbMax;
    Vec4f aabbMin;

    uint32 textureIndex;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;

    Vec4f _pad3;
};

static_assert(sizeof(LightmapVolumeShaderData) % 64 == 0);

struct RenderProxyLightmapVolume : IRenderProxy
{
    LightmapVolume* lightmapVolume = nullptr;
    FixedArray<Texture*, MaxAtlasesPerLightmapVolume> atlasIrradianceTextures {};
    FixedArray<Texture*, MaxAtlasesPerLightmapVolume> atlasBentNormalTextures {};
    uint32 numAtlases = 0;

    Mat4f transformMatrix;
    BoundingBox worldAabb;

    LightmapVolumeShaderData bufferData {};
};

struct RenderProxyParticleVolume : IRenderProxy
{
    ParticleVolume* particleVolume = nullptr;

    Texture* particleTexture = nullptr;
    Mesh* particleMesh = nullptr;

    BoundingBox worldAabb; // for culling/debug

    struct {
        Mat4f transformMatrix;

        float startSize = 0.0f;
        float randomness = 0.0f;
        float avgLifespan = 0.0f;
        uint32 maxParticles = 0;
    } bufferData;
};

struct FogVolumeShaderData
{
    Mat4f transformMatrix;
    Vec4f aabbMin;
    Vec4f aabbMax;
    Vec4u lightIndices[4];

    uint32 numBoundLights;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;

    Vec4f _pad3;
};

struct RenderProxyFogVolume : IRenderProxy
{
    FogVolume* fogVolume = nullptr;
    Texture* volumeTexture = nullptr;
    Texture* noiseTexture = nullptr;
    BoundingBox worldAabb;
    FogVolumeShaderData bufferData {};
};

struct MaterialShaderData
{
    Vec4f albedo;

    // 4 vec4s of 0.0..1.0 values stuffed into uint32s
    Vec4u packedParams;

    Vec4u textureIndices[4];

    // =====
    uint32 textureUsage;
    float parallaxHeight;
    Vec2f uvScale;

    // =====
    Vec4f _pad0;
};

static_assert(sizeof(MaterialShaderData) % 64 == 0);

struct RenderProxyMaterial : IRenderProxy
{
    RenderProxyMaterial()
    {
        // set all to invalid (~0u) by default
        Memory::Fill(boundTextureIndices.Data(), 0xFFu, boundTextureIndices.ByteSize());
    }

    Material* material = nullptr;
    MaterialAttributes attributes;

    FixedArray<uint32, MaxBoundTextures> boundTextureIndices;
    Array<Texture*> boundTextures;

    MaterialShaderData bufferData {};
};

struct SkeletonShaderData
{
    Mat4f bones[MaxBonesPerSkeleton];
};

struct RenderProxySkeleton : IRenderProxy
{
    RenderProxySkeleton()
    {
        for (uint32 i = 0; i < MaxBonesPerSkeleton; ++i)
        {
            bufferData.bones[i] = Mat4f::Identity();
        }
    }

    Skeleton* skeleton = nullptr;
    SkeletonShaderData bufferData {};
};

struct CameraShaderData
{
    Mat4f viewMat;
    Mat4f projMat;

    Mat4f viewProjMat;

    Mat4f inverseViewMat;
    Mat4f inverseProjMat;

    Mat4f prevViewProjMat;

    Vec4u dimensions;
    Vec4f cameraPosition;
    Vec4f cameraDirection;
    Vec4f jitter;

    float cameraNear;
    float cameraFar;
    float cameraFov;
    float _pad0;

    Vec4f _pad1;
    Vec4f _pad2;
    Vec4f _pad3;
};

static_assert(sizeof(CameraShaderData) % 64 == 0);

struct RenderProxyCamera : IRenderProxy
{
    Camera* camera = nullptr;
    CameraShaderData bufferData {};
    Frustum viewFrustum;
};

struct SpriteShaderData
{
    Vec4f positionSize;
    Vec4f color;

    uint32 spriteType;
    float opacity;
    uint32 textureIndex;
    uint32 alwaysFaceCamera;

    Vec4f _pad0;
};

struct RenderProxySprite : IRenderProxy
{
    class Sprite* sprite = nullptr;
    Texture* texture = nullptr;
    class FontAtlas* fontAtlas = nullptr;
    String text;
    Color textColor;
    float textSize;
    BoundingBox textAabb;
    SpriteShaderData bufferData {};
};

struct ProbeRayData
{
    Vec4f directionDepth;
    Vec4f origin;
    Vec4f normal;
    Vec4f color;
};

struct alignas(16) DDGIConstants
{
    Mat4f rotationMatrix;

    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4u probeBorder;
    Vec4u probeCounts;
    Vec4u gridDimensions;
    Vec4u imageDimensions;

    float probeDistance;
    uint32 numRaysPerProbe;
    uint32 numBoundLights;
    uint32 counter;
};

} // namespace Hyperion
