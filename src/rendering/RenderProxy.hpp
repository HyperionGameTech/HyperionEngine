/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/ObjId.hpp>
#include <core/reflection/ObjectFwd.hpp>

#include <core/utilities/UserData.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/Bitset.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/math/Transform.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Mat4f.hpp>
#include <core/math/Frustum.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/MeshInstanceData.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/util/ResourceTracker.hpp>

namespace hyperion {

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

enum LightmapTextureType : uint32;

HYP_STRUCT()
struct MeshRaytracingData
{
    HYP_STRUCT_BODY(MeshRaytracingData);

    HYP_FIELD(NoScriptBindings)
    GpuBlasRef blas;

    ~MeshRaytracingData();
};

class IRenderProxy
{
protected:
    virtual ~IRenderProxy() = default; // @TODO: Get rid of virtual dtor

public:
    int version = 0;
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

    Vec4f _pad0;
    Vec4f _pad1;
    Vec3f worldAabbMax;
    Vec3f worldAabbMin;

    uint32 entityIndex = ~0u;
    uint32 lightmapVolumeIndex = ~0u;
    uint32 materialIndex = ~0u;
    uint32 skeletonIndex = ~0u;

    uint32 bucket;
    uint32 flags;
    uint32 _pad3;
    uint32 _pad4;

    struct alignas(16) EntityUserData
    {
        Vec4u userData0;
        Vec4u userData1;
    } userData;
};

static_assert(sizeof(EntityShaderData) == 256);

enum class LightmapElementId : uint32;

/*! \brief Proxy for a renderable Entity with a valid Mesh and Material assigned */
class RenderProxyMesh final : public IRenderProxy
{
public:
    WeakHandle<Entity> entity;

    Mesh* mesh = nullptr;
    Material* material = nullptr;
    Skeleton* skeleton = nullptr;

    uint32 numIndices = 0;

    LightmapVolume* lightmapVolume = nullptr;
    LightmapElementId lightmapElementId = LightmapElementId(~0u);

    RenderableAttributeSet cachedAttributes;

    MeshInstanceData instanceData;

    MeshRaytracingData raytracingData;

    EntityShaderData bufferData {};

    HYP_FORCE_INLINE bool operator==(const RenderProxyMesh& other) const
    {
        return entity == other.entity
            && mesh == other.mesh
            && material == other.material
            && skeleton == other.skeleton
            && numIndices == other.numIndices
            && lightmapVolume == other.lightmapVolume
            && lightmapElementId == other.lightmapElementId
            && cachedAttributes == other.cachedAttributes
            && instanceData == other.instanceData
            && Memory::MemCmp(&bufferData, &other.bufferData, sizeof(EntityShaderData)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const RenderProxyMesh& other) const
    {
        return entity != other.entity
            || mesh != other.mesh
            || material != other.material
            || numIndices != other.numIndices
            || skeleton != other.skeleton
            || lightmapVolume != other.lightmapVolume
            || lightmapElementId != other.lightmapElementId
            || cachedAttributes != other.cachedAttributes
            || instanceData != other.instanceData
            || Memory::MemCmp(&bufferData, &other.bufferData, sizeof(EntityShaderData)) != 0;
    }
};

struct EnvProbeShaderData
{
    Mat4f faceViewMatrices[6];

    Vec4f aabbMax;
    Vec4f aabbMin;
    Vec4f worldPosition;

    uint32 textureIndex = ~0u;
    uint32 flags = 0;
    float cameraNear = 0.01f;
    float cameraFar = 100.0f;

    Vec2u dimensions;
    uint64 visibilityBits = 0;
    Vec4i positionInGrid;

    Vec4f shData[9];
};

class RenderProxyEnvProbe final : public IRenderProxy
{
public:
    WeakHandle<EnvProbe> envProbe;
    Texture* texture = nullptr; // baked cubemap texture or prefiltered env
    EnvProbeShaderData bufferData {};
};

struct EnvGridShaderData
{
    uint32 probeIndices[MaxBoundAmbientProbes];

    Vec4f center;
    Vec4f extent;
    Vec4f aabbMax;
    Vec4f aabbMin;

    Vec4u density;

    Vec4f voxelGridAabbMax;
    Vec4f voxelGridAabbMin;

    Vec2i lightFieldImageDimensions;
    Vec2i irradianceOctahedronSize;
};

class RenderProxyEnvGrid final : public IRenderProxy
{
public:
    WeakHandle<EnvGrid> envGrid;
    EnvGridShaderData bufferData {};
    EnvProbe* envProbes[MaxBoundAmbientProbes];
};

struct alignas(16) LightShaderData
{
    uint32 lightType;
    uint32 materialIndex;
    uint32 radiusFalloffPacked;
    uint32 flags;

    Vec2f areaSize; // also angles for spot lights

    Vec4f positionIntensity;
    Vec4f color;

    Vec4f normal; // area light normal / spot light direction

    // Shadow map data
    Mat4f shadowMatrix;
    Vec4f aabbMin;
    Vec4f aabbMax;
    Vec4f dimensionsScale; // xy = shadow map dimensions in pixels, zw = shadow map dimensions relative to the atlas dimensions
    Vec2f offsetUv;        // offset in the atlas texture array
    uint32 layerIndex;     // index of the atlas in the shadow map texture array, or cubemap index for point lights

    Vec4f _pad0;
    Vec4f _pad1;
    Vec4f _pad2;
};

class RenderProxyLight final : public IRenderProxy
{
public:
    WeakHandle<Light> light;
    Material* lightMaterial = nullptr; // for textured area lights
    ShadowMap* shadowMap = nullptr;
    Array<View*, InlineAllocator<2>> shadowViews; // optional, for lights casting shadow
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
};

class RenderProxyParticleVolume final : public IRenderProxy
{
public:
    WeakHandle<ParticleVolume> particleVolume;
    Texture* particleTexture = nullptr;
    BoundingBox worldAabb; // for culling/debug
    ParticleVolumeShaderData bufferData {};
};

struct FogVolumeShaderData
{
};

class RenderProxyFogVolume final : public IRenderProxy
{
public:
    WeakHandle<FogVolume> fogVolume;
    Texture* volumeTexture = nullptr;
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
};

static_assert(sizeof(MaterialShaderData) == 112);

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
    MaterialShaderData bufferData {};
    FixedArray<uint32, MaxBoundTextures> boundTextureIndices;
    Array<Handle<Texture>> boundTextures;
};

struct SkeletonShaderData
{
    static constexpr SizeType maxBones = 256;

    Mat4f bones[maxBones];
};

class RenderProxySkeleton final : public IRenderProxy
{
public:
    RenderProxySkeleton()
    {
        for (SizeType i = 0; i < SkeletonShaderData::maxBones; ++i)
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
    Mat4f prevViewMat;

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

} // namespace hyperion
