/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/utilities/EnumFlags.hpp>

#include <Core/containers/FixedArray.hpp>

#include <Core/math/BoundingBox.hpp>
#include <Core/math/Mat4f.hpp>
#include <Core/math/Frustum.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderObject.hpp>

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
class EnvGrid;
class ShadowMap;
class InstancedMeshData;

enum AtlasTextureType : uint32;

HYP_STRUCT()
struct MeshRayTracingData
{
    HYP_STRUCT_BODY(MeshRayTracingData);

    HYP_FIELD(NoScriptBindings)
    GpuBlasRef blas;

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

struct alignas(16) WorldShaderData
{
    Vec4f fogParams;

    float gameTime;
    uint32 frameCounter;
    uint32 _pad0;
    uint32 _pad1;
};

struct alignas(16) EntityShaderData
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

enum class LightmapElementId : uint32;

struct InstanceData
{
    static constexpr uint32 MaxBuffers = 5;

    ByteBuffer buffers[MaxBuffers];

    uint32 bufferStructSizes[MaxBuffers] {};
    uint32 bufferStructAlignments[MaxBuffers] {};
};

/*! \brief Proxy for a renderable Entity with a valid Mesh and Material assigned */
class RenderProxyMesh final : public IRenderProxy
{
public:
    WeakHandle<Entity> entity;

    Mesh* mesh = nullptr;
    Material* material = nullptr;
    Skeleton* skeleton = nullptr;

    uint32 numIndices = 0;

    uint32 numInstances = 0;
    bool enableAutoInstancing = false;

    LightmapVolume* lightmapVolume = nullptr;
    LightmapElementId lightmapElementId = LightmapElementId(~0u);

    RenderableAttributeSet cachedAttributes;

    InstanceData instanceData;

    MeshRayTracingData rayTracingData;

    EntityShaderData bufferData {};
};

struct alignas(16) EnvProbeShaderData
{
    Mat4f faceViewMatrices[6];

    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4f worldPosition;

    uint32 textureIndex = ~0u;
    uint32 flags = 0;
    Vec2u dimensions;

    Vec4f shData[9];
};

class RenderProxyEnvProbe final : public IRenderProxy
{
public:
    WeakHandle<EnvProbe> envProbe;
    Texture* texture = nullptr; // baked cubemap texture or prefiltered env
    EnvProbeShaderData bufferData {};
};

struct alignas(16) EnvGridShaderData
{
    // Nothing for now until we add the new env grid (baked)

    Vec4f dummy;
};

class RenderProxyEnvGrid final : public IRenderProxy
{
public:
    WeakHandle<EnvGrid> envGrid;
    EnvGridShaderData bufferData {};
};

struct alignas(16) ShadowMapData
{
    uint32 layerIndex;
    float splitDistance;
    float _pad0;
    float _pad1;

    Mat4f viewProjMat;
    Mat4f invProjMat;
    Vec4f aabbMin;          // w = offsetUV.x
    Vec4f aabbMax;          // w = offsetUV.y
    Vec4f dimensionsScale;  // xy = shadow map dimensions in pixels, zw = shadow map dimensions relative to the atlas dimensions
};

struct alignas(16) LightShaderData
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
    Vec4f _pad1[3];
};

class RenderProxyLight final : public IRenderProxy
{
public:
    WeakHandle<Light> light;
    Material* lightMaterial = nullptr; // for textured area lights
    Texture* bakedShadowMap = nullptr;
    uint32 numCascades = 0;
    LightShaderData bufferData {};
};

struct alignas(16) LightmapVolumeShaderData
{
    Vec4f aabbMax;
    Vec4f aabbMin;

    uint32 textureIndex;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;
};

class RenderProxyLightmapVolume final : public IRenderProxy
{
public:
    WeakHandle<LightmapVolume> lightmapVolume;
    Array<Texture*> atlasIrradianceTextures;
    Array<Texture*> atlasRadianceTextures;
    uint32 numAtlases = 0;
    LightmapVolumeShaderData bufferData {};
};

struct alignas(16) ParticleVolumeShaderData
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
};

class RenderProxyParticleVolume final : public IRenderProxy
{
public:
    WeakHandle<ParticleVolume> particleVolume;
    Texture* particleTexture = nullptr;
    BoundingBox worldAabb; // for culling/debug
    ParticleVolumeShaderData bufferData {};
};

struct alignas(16) FogVolumeShaderData
{
    Mat4f transformMatrix;
    Vec4f aabbMin;
    Vec4f aabbMax;
    uint32 numBoundLights;
    Vec4u lightIndices[4];
};

class RenderProxyFogVolume final : public IRenderProxy
{
public:
    WeakHandle<FogVolume> fogVolume;
    Texture* volumeTexture = nullptr;
    Texture* noiseTexture = nullptr;
    BoundingBox worldAabb;
    FogVolumeShaderData bufferData {};
};

struct alignas(16) MaterialShaderData
{
    Vec4f albedo;

    // 4 vec4s of 0.0..1.0 values stuffed into uint32s
    Vec4u packedParams;

    Vec2f uvScale;
    float parallaxHeight;

    uint32 textureUsage;

    Vec4u textureIndices[4];

    Vec4f _pad0;
};

class RenderProxyMaterial final : public IRenderProxy
{
public:
    RenderProxyMaterial()
    {
        for (uint32 i = 0; i < MaxBoundTextures; ++i)
        {
            boundTextureIndices[i] = ~0u;
        }
    }

    WeakHandle<Material> material;
    MaterialAttributes attributes;
    MaterialShaderData bufferData {};
    FixedArray<uint32, MaxBoundTextures> boundTextureIndices;
    Array<Handle<Texture>> boundTextures;
};

struct alignas(16) SkeletonShaderData
{
    static constexpr size_t maxBones = 256;

    Mat4f bones[maxBones];
};

class RenderProxySkeleton final : public IRenderProxy
{
public:
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

struct alignas(16) CameraShaderData
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
    float _pad;

    Vec4f _pad1;
    Vec4f _pad2;
    Vec4f _pad3;
};

class RenderProxyCamera final : public IRenderProxy
{
public:
    WeakHandle<Camera> camera;
    CameraShaderData bufferData {};
    Frustum viewFrustum;
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
