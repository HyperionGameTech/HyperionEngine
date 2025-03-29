/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENV_GRID_HPP
#define HYPERION_ENV_GRID_HPP

#include <core/Base.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <scene/camera/Camera.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderEnvProbe.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

namespace hyperion {

class Entity;

enum class EnvGridFlags : uint32
{
    NONE                    = 0x0,
    USE_VOXEL_GRID          = 0x1
};

HYP_MAKE_ENUM_FLAGS(EnvGridFlags);

enum EnvGridType : uint32
{
    ENV_GRID_TYPE_INVALID       = uint32(-1),
    ENV_GRID_TYPE_SH            = 0,
    ENV_GRID_TYPE_VOXEL,
    ENV_GRID_TYPE_LIGHT_FIELD,
    ENV_GRID_TYPE_MAX
};

struct EnvGridShaderData
{
    uint32  probe_indices[max_bound_ambient_probes];

    Vec4f   center;
    Vec4f   extent;
    Vec4f   aabb_max;
    Vec4f   aabb_min;

    Vec4u   density;

    Vec4f   voxel_grid_aabb_max;
    Vec4f   voxel_grid_aabb_min;

    Vec2i   light_field_image_dimensions;
    Vec2i   irradiance_octahedron_size;
};

static constexpr uint32 max_env_grids = (1ull * 1024ull * 1024ull) / sizeof(EnvGridShaderData);

struct EnvProbeCollection
{
    uint32                                                                          num_probes = 0;
    FixedArray<uint32, max_bound_ambient_probes * 2>                                indirect_indices = { 0 };
    FixedArray<Handle<EnvProbe>, max_bound_ambient_probes>                          env_probes = { };
    FixedArray<TResourceHandle<EnvProbeRenderResource>, max_bound_ambient_probes>   env_probe_render_resources = { };

    // Must be called in EnvGrid::Init(), before probes are used from the render thread.
    // returns the index
    uint32 AddProbe(const Handle<EnvProbe> &env_probe);

    // Must be called in EnvGrid::Init(), before probes are used from the render thread.
    void AddProbe(uint32 index, const Handle<EnvProbe> &env_probe);

    HYP_FORCE_INLINE void SetProbeIndexOnGameThread(uint32 index, uint32 new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[index] = new_index;
    }

    HYP_FORCE_INLINE uint32 GetEnvProbeIndexOnGameThread(uint32 index) const
        { return indirect_indices[index]; }

    HYP_FORCE_INLINE const Handle<EnvProbe> &GetEnvProbeDirect(uint32 index) const
        { return env_probes[index]; }

    HYP_FORCE_INLINE const Handle<EnvProbe> &GetEnvProbeOnGameThread(uint32 index) const
        { return env_probes[indirect_indices[index]]; }

    HYP_FORCE_INLINE void SetProbeIndexOnRenderThread(uint32 index, uint32 new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[max_bound_ambient_probes + index] = new_index;
    }

    HYP_FORCE_INLINE uint32 GetEnvProbeIndexOnRenderThread(uint32 index) const
        { return indirect_indices[max_bound_ambient_probes + index]; }
    
    HYP_FORCE_INLINE const Handle<EnvProbe> &GetEnvProbeOnRenderThread(uint32 index) const
        { return env_probes[indirect_indices[max_bound_ambient_probes + index]]; }
};

struct EnvGridOptions
{
    EnvGridType             type = ENV_GRID_TYPE_SH;
    BoundingBox             aabb;
    Vec3u                   density = Vec3u::Zero();
    EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;
};

// @TODO Refactor this class into a base class and divide it into subclasses (SH, LightField, etc.)

HYP_CLASS()
class HYP_API EnvGrid : public RenderSubsystem
{
    HYP_OBJECT_BODY(EnvGrid);

public:
    EnvGrid(Name name, const Handle<Scene> &parent_scene, EnvGridOptions options);
    EnvGrid(const EnvGrid &other)               = delete;
    EnvGrid &operator=(const EnvGrid &other)    = delete;
    virtual ~EnvGrid();
    
    HYP_FORCE_INLINE EnvGridType GetEnvGridType() const
        { return m_options.type; }

    HYP_FORCE_INLINE const EnvProbeCollection &GetEnvProbeCollection() const
        { return m_env_probe_collection; }
    
    HYP_METHOD(Property="AABB", Editor=true, Label="EnvGrid Area Bounds", Description="The area that will be considered for inclusion in the EnvGrid")
    const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_METHOD(Property="AABB", Editor=true)
    void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; }

    HYP_METHOD(Property="Camera")
    HYP_FORCE_INLINE const Handle<Camera> &GetCamera() const
        { return m_camera; }

    HYP_FORCE_INLINE const ShaderRef &GetShader() const
        { return m_ambient_shader; }

    HYP_FORCE_INLINE RenderCollector &GetRenderCollector()
        { return m_render_collector; }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollector() const
        { return m_render_collector; }

    void SetCameraData(const BoundingBox &aabb, const Vec3f &camera_position);

private:
    virtual void Init() override;
    virtual void InitGame() override; // init on game thread
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(Frame *frame) override;

    HYP_FORCE_INLINE Vec3f SizeOfProbe() const
        { return m_aabb.GetExtent() / Vec3f(m_options.density); }

    virtual void OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index prev_index) override;

    void CreateShader();
    void CreateFramebuffer();

    void CreateVoxelGridData();
    void CreateSphericalHarmonicsData();
    void CreateLightFieldData();

    void RenderEnvProbe(
        Frame *frame,
        uint32 probe_index
    );

    void ComputeEnvProbeIrradiance_SphericalHarmonics(
        Frame *frame,
        const Handle<EnvProbe> &probe
    );

    void ComputeEnvProbeIrradiance_LightField(
        Frame *frame,
        const Handle<EnvProbe> &probe
    );

    void OffsetVoxelGrid(
        Frame *frame,
        Vec3i offset
    );

    void VoxelizeProbe(
        Frame *frame,
        uint32 probe_index
    );

    Handle<Scene>               m_parent_scene;

    const EnvGridOptions        m_options;

    BoundingBox                 m_aabb;
    BoundingBox                 m_voxel_grid_aabb;
    Vec3f                       m_offset;
    
    Handle<Camera>              m_camera;
    ResourceHandle              m_camera_resource_handle;

    RenderCollector             m_render_collector;

    ShaderRef                   m_ambient_shader;
    FramebufferRef              m_framebuffer;

    EnvProbeCollection          m_env_probe_collection;

    EnvGridShaderData           m_shader_data;
    uint32                      m_current_probe_index;

    ComputePipelineRef          m_clear_sh;
    ComputePipelineRef          m_compute_sh;
    ComputePipelineRef          m_reduce_sh;
    ComputePipelineRef          m_finalize_sh;

    Array<DescriptorTableRef>   m_compute_sh_descriptor_tables;
    Array<GPUBufferRef>         m_sh_tiles_buffers;

    ComputePipelineRef          m_clear_voxels;
    ComputePipelineRef          m_voxelize_probe;
    ComputePipelineRef          m_offset_voxel_grid;
    ComputePipelineRef          m_generate_voxel_grid_mipmaps;
    
    Handle<Texture>             m_voxel_grid_texture;

    Array<ImageViewRef>         m_voxel_grid_mips;
    Array<DescriptorTableRef>   m_generate_voxel_grid_mipmaps_descriptor_tables;

    Handle<Texture>             m_irradiance_texture;
    Handle<Texture>             m_depth_texture;
    Array<GPUBufferRef>         m_uniform_buffers;

    ComputePipelineRef          m_compute_irradiance;
    ComputePipelineRef          m_compute_filtered_depth;
    ComputePipelineRef          m_copy_border_texels;

    Queue<uint32>               m_next_render_indices;

    HashCode                    m_octant_hash_code;
};

} // namespace hyperion

#endif // !HYPERION_ENV_PROBE_HPP