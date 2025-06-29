/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_PROXY_HPP
#define HYPERION_RENDER_PROXY_HPP

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

#include <rendering/RenderableAttributes.hpp>
#include <rendering/MeshInstanceData.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <util/ResourceTracker.hpp>

namespace hyperion {

class Entity;
class Mesh;
class Material;
class Skeleton;
class EnvProbe;
class EnvGrid;

HYP_STRUCT()
struct MeshRaytracingData
{
    HYP_FIELD()
    FixedArray<BLASRef, max_frames_in_flight> bottom_level_acceleration_structures;
};

class IRenderProxy
{
public:
    virtual ~IRenderProxy() = default;

    virtual void SafeRelease()
    {
    }

#ifdef HYP_DEBUG_MODE
    uint16 state = 0x1;
#endif
};

/*! \brief Proxy for a renderable Entity with a valid Mesh and Material assigned */
class RenderProxy : public IRenderProxy
{
public:
    WeakHandle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<Material> material;
    Handle<Skeleton> skeleton;
    Matrix4 model_matrix;
    Matrix4 previous_model_matrix;
    BoundingBox aabb;
    UserData<32, 16> user_data;
    MeshInstanceData instance_data;
    int version = 0;

    ~RenderProxy() override = default;

    HYP_API virtual void SafeRelease() override;

    void IncRefs() const;
    void DecRefs() const;

    bool operator==(const RenderProxy& other) const
    {
        // Check version first for faster comparison
        return version == other.version
            && entity == other.entity
            && mesh == other.mesh
            && material == other.material
            && skeleton == other.skeleton
            && model_matrix == other.model_matrix
            && previous_model_matrix == other.previous_model_matrix
            && aabb == other.aabb
            && user_data == other.user_data
            && instance_data == other.instance_data;
    }

    bool operator!=(const RenderProxy& other) const
    {
        // Check version first for faster comparison
        return version != other.version
            || entity != other.entity
            || mesh != other.mesh
            || material != other.material
            || skeleton != other.skeleton
            || model_matrix != other.model_matrix
            || previous_model_matrix != other.previous_model_matrix
            || aabb != other.aabb
            || user_data != other.user_data
            || instance_data != other.instance_data;
    }
};

struct EnvProbeSphericalHarmonics
{
    Vec4f values[9];
};

struct EnvProbeShaderData
{
    Matrix4 face_view_matrices[6];

    Vec4f aabb_max;
    Vec4f aabb_min;
    Vec4f world_position;

    uint32 texture_index;
    uint32 flags;
    float camera_near;
    float camera_far;

    Vec2u dimensions;
    uint64 visibility_bits;
    Vec4i position_in_grid;

    EnvProbeSphericalHarmonics sh;
};

class RenderProxyEnvProbe : public IRenderProxy
{
public:
    ~RenderProxyEnvProbe() override = default;

    WeakHandle<EnvProbe> env_probe;
    EnvProbeShaderData buffer_data {};
};

struct EnvGridShaderData
{
    uint32 probe_indices[max_bound_ambient_probes];

    Vec4f center;
    Vec4f extent;
    Vec4f aabb_max;
    Vec4f aabb_min;

    Vec4u density;

    Vec4f voxel_grid_aabb_max;
    Vec4f voxel_grid_aabb_min;

    Vec2i light_field_image_dimensions;
    Vec2i irradiance_octahedron_size;
};

class RenderProxyEnvGrid : public IRenderProxy
{
public:
    ~RenderProxyEnvGrid() override = default;

    WeakHandle<EnvGrid> env_grid;
    EnvGridShaderData buffer_data {};
};

struct LightShaderData
{
    uint32 light_id;
    uint32 light_type;
    uint32 color_packed;
    float radius;
    // 16

    float falloff;
    uint32 shadow_map_index;
    Vec2f area_size;
    // 32

    Vec4f position_intensity;
    Vec4f normal;
    // 64

    Vec2f spot_angles;
    uint32 material_index;
    uint32 _pad2;

    Vec4f aabb_min;
    Vec4f aabb_max;

    Vec4u pad5;
};

static_assert(sizeof(LightShaderData) == 128);

class RenderProxyLight : public IRenderProxy
{
public:
    ~RenderProxyLight() override = default;

    WeakHandle<Light> light;
    LightShaderData buffer_data {};
};

struct LightmapVolumeShaderData
{
    Vec4f aabb_max;
    Vec4f aabb_min;

    uint32 texture_index;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;
};

class RenderProxyLightmapVolume : public IRenderProxy
{
public:
    ~RenderProxyLightmapVolume() override = default;

    WeakHandle<LightmapVolume> lightmap_volume;
    LightmapVolumeShaderData buffer_data {};
};

} // namespace hyperion

#endif
