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
class ProbeVolume;
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

class IRenderProxy
{
public:
    bool forceRebind = false;
};

class NullProxy final : public IRenderProxy
{
private:
    NullProxy() = default;
};

struct WorldShaderData
{
    Vec4f fogParams;

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
    WeakHandle<Entity> entity;

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

    float shData[3 * 9];

    bool enableAutoInstancing = false;
};

struct EnvProbeShaderData
{
    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4f worldPosition;

    Vec2u dimensions;
    uint32 textureIndex = ~0u;
    uint32 typeAndFlags;

    Vec4f shData[9];
};

static_assert(sizeof(EnvProbeShaderData));

struct RenderProxyEnvProbe : IRenderProxy
{
    WeakHandle<EnvProbe> envProbe;
    Texture* texture = nullptr;           // baked cubemap texture or prefiltered env
    Texture* visibilityTexture = nullptr; // only relevant if envprobe has HAS_VISIBILITY flag set.
    EnvProbeShaderData bufferData {};
};

struct ProbeVolumeShaderData
{
    // Nothing for now until we add the new env grid (baked)

    Vec4f dummy;
};

struct RenderProxyProbeVolume : IRenderProxy
{
    WeakHandle<ProbeVolume> probeVolume;
    ProbeVolumeShaderData bufferData {};
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
    WeakHandle<Light> light;
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
    WeakHandle<LightmapVolume> lightmapVolume;
    Array<Texture*> atlasIrradianceTextures;
    Array<Texture*> atlasRadianceTextures;
    uint32 numAtlases = 0;
    LightmapVolumeShaderData bufferData {};
};

struct ParticleVolumeShaderData
{
    Vec4f originStartSize; // xyz = origin, w = start size

    float spawnRadius = 0.0f;
    float randomness = 0.0f;
    float avgLifespan = 0.0f;
    uint32 maxParticles = 0;

    float maxParticlesSqrt = 0.0f;
    float _pad0 = 0.0f;
    float _pad1 = 0.0f;
    float _pad2 = 0.0f;

    Vec4f _pad3;
};

struct RenderProxyParticleVolume : IRenderProxy
{
    WeakHandle<ParticleVolume> particleVolume;

    Texture* particleTexture = nullptr;
    Mesh* particleMesh = nullptr;

    BoundingBox worldAabb; // for culling/debug
    ParticleVolumeShaderData bufferData {};
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
    WeakHandle<FogVolume> fogVolume;
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

    uint32 textureUsage;

    float parallaxHeight;

    Vec2f uvScale;

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

    WeakHandle<Material> material;
    MaterialAttributes attributes;

    FixedArray<uint32, MaxBoundTextures> boundTextureIndices;
    Array<Texture*> boundTextures;

    MaterialShaderData bufferData {};
};

struct SkeletonShaderData
{
    static constexpr size_t maxBones = 256;

    Mat4f bones[maxBones];
};

struct RenderProxySkeleton : IRenderProxy
{
    RenderProxySkeleton()
    {
        for (size_t i = 0; i < SkeletonShaderData::maxBones; ++i)
        {
            bufferData.bones[i] = Mat4f::Identity();
        }
    }

    WeakHandle<Skeleton> skeleton;
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
    WeakHandle<Camera> camera;
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
    WeakHandle<class Sprite> sprite;
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
