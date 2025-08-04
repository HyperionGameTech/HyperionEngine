/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>

#include <core/utilities/UserData.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/Bitset.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/math/Transform.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Matrix4.hpp>
#include <core/math/Frustum.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/MeshInstanceData.hpp>
#include <rendering/RenderObject.hpp>

#include <util/ResourceTracker.hpp>

namespace hyperion {

class Entity;
class Mesh;
class Light;
class Camera;
class View;
class Texture;
class LightmapVolume;
class Material;
class Skeleton;
class EnvProbe;
class EnvGrid;
class ShadowMap;

HYP_STRUCT()
struct MeshRaytracingData
{
    HYP_FIELD()
    FixedArray<BLASRef, g_framesInFlight> bottomLevelAccelerationStructures;

    ~MeshRaytracingData()
    {
        SafeRelease(std::move(bottomLevelAccelerationStructures));
    }
};

class IRenderProxy
{
protected:
    ~IRenderProxy() = default;
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
    Matrix4 modelMatrix;
    Matrix4 previousModelMatrix;

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

/*! \brief Proxy for a renderable Entity with a valid Mesh and Material assigned */
class RenderProxyMesh final : public IRenderProxy
{
public:
    WeakHandle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<Material> material;
    Handle<Skeleton> skeleton;
    MeshInstanceData instanceData;
    MeshRaytracingData raytracingData;
    EntityShaderData bufferData {};

    HYP_FORCE_INLINE bool operator==(const RenderProxyMesh& other) const
    {
        return entity == other.entity
            && mesh == other.mesh
            && material == other.material
            && skeleton == other.skeleton
            && instanceData == other.instanceData
            && Memory::MemCmp(&bufferData, &other.bufferData, sizeof(EntityShaderData)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const RenderProxyMesh& other) const
    {
        return entity != other.entity
            || mesh != other.mesh
            || material != other.material
            || skeleton != other.skeleton
            || instanceData != other.instanceData
            || Memory::MemCmp(&bufferData, &other.bufferData, sizeof(EntityShaderData)) != 0;
    }
};

struct EnvProbeSphericalHarmonics
{
    Vec4f values[9];
};

struct EnvProbeShaderData
{
    Matrix4 faceViewMatrices[6];

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

    EnvProbeSphericalHarmonics sh;
};

class RenderProxyEnvProbe final : public IRenderProxy
{
public:
    WeakHandle<EnvProbe> envProbe;
    EnvProbeShaderData bufferData {};
};

struct EnvGridShaderData
{
    uint32 probeIndices[g_maxBoundAmbientProbes];

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
    ObjId<EnvProbe> envProbes[g_maxBoundAmbientProbes];
};

struct alignas(16) LightShaderData
{
    uint32 lightType;
    uint32 colorPacked;
    uint32 radiusFalloffPacked;
    uint32 flags;

    Vec2f areaSize; // also angles for spot lights

    Vec4f positionIntensity;
    Vec4f normal;

    // Shadow map data
    Matrix4 shadowMatrix;
    Vec4f aabbMin;
    Vec4f aabbMax;
    Vec4f dimensionsScale; // xy = shadow map dimensions in pixels, zw = shadow map dimensions relative to the atlas dimensions
    Vec2f offsetUv;        // offset in the atlas texture array
    uint32 layerIndex;     // index of the atlas in the shadow map texture array, or cubemap index for point lights

    uint32 materialIndex;
};

class RenderProxyLight final : public IRenderProxy
{
public:
    WeakHandle<Light> light;
    WeakHandle<Material> lightMaterial;  // for textured area lights
    Array<WeakHandle<View>> shadowViews; // optional, for lights casting shadow
    LightShaderData bufferData {};
    ShadowMap* shadowMap = nullptr;
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
    LightmapVolumeShaderData bufferData {};
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
        for (uint32 i = 0; i < g_maxBoundTextures; ++i)
        {
            boundTextureIndices[i] = ~0u;
        }
    }

    WeakHandle<Material> material;
    MaterialShaderData bufferData {};
    FixedArray<uint32, g_maxBoundTextures> boundTextureIndices;
    Array<Handle<Texture>> boundTextures;
};

struct SkeletonShaderData
{
    static constexpr SizeType maxBones = 256;

    Matrix4 bones[maxBones];
};

class RenderProxySkeleton final : public IRenderProxy
{
public:
    RenderProxySkeleton()
    {
        for (SizeType i = 0; i < SkeletonShaderData::maxBones; ++i)
        {
            bufferData.bones[i] = Matrix4::Identity();
        }
    }

    WeakHandle<Skeleton> skeleton;
    SkeletonShaderData bufferData {};
};

struct CameraShaderData
{
    Matrix4 view;
    Matrix4 projection;
    Matrix4 previousView;

    Vec4u dimensions;
    Vec4f cameraPosition;
    Vec4f cameraDirection;
    Vec4f jitter;

    float cameraNear;
    float cameraFar;
    float cameraFov;
    uint32 id;

    Vec4f _pad1;
    Vec4f _pad2;
    Vec4f _pad3;

    Matrix4 _pad4;
    Matrix4 _pad5;
    Matrix4 _pad6;
};

class RenderProxyCamera final : public IRenderProxy
{
public:
    WeakHandle<Camera> camera;
    CameraShaderData bufferData {};
    Frustum viewFrustum;
};

} // namespace hyperion
