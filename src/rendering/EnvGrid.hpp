/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENV_GRID_HPP
#define HYPERION_ENV_GRID_HPP

#include <core/Base.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <scene/camera/Camera.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>

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
    ENV_GRID_TYPE_INVALID   = uint32(-1),
    ENV_GRID_TYPE_SH        = 0,
    ENV_GRID_TYPE_MAX
};

struct alignas(256) EnvGridShaderData
{
    uint32  probe_indices[max_bound_ambient_probes];

    Vec4f   center;
    Vec4f   extent;
    Vec4f   aabb_max;
    Vec4f   aabb_min;

    Vec4u   density;
    Vec4u   enabled_indices_mask;

    Vec4f   voxel_grid_aabb_max;
    Vec4f   voxel_grid_aabb_min;
};

static_assert(sizeof(EnvGridShaderData) == 4352);

static constexpr uint32 max_env_grids = (1ull * 1024ull * 1024ull) / sizeof(EnvGridShaderData);

struct EnvProbeCollection
{
    uint                                                    num_probes = 0;
    FixedArray<uint, max_bound_ambient_probes * 2>          indirect_indices = { 0 };
    FixedArray<Handle<EnvProbe>, max_bound_ambient_probes>  probes = { };

    // Must be called on init, before render thread starts using probes too
    // returns the index
    uint AddProbe(Handle<EnvProbe> probe)
    {
        AssertThrow(num_probes < max_bound_ambient_probes);

        const uint index = num_probes++;

        probes[index] = std::move(probe);
        indirect_indices[index] = index;
        indirect_indices[max_bound_ambient_probes + index] = index;

        return index;
    }

    void AddProbe(uint index, Handle<EnvProbe> probe)
    {
        AssertThrow(index < max_bound_ambient_probes);

        num_probes = MathUtil::Max(num_probes, index + 1);

        probes[index] = std::move(probe);
        indirect_indices[index] = index;
        indirect_indices[max_bound_ambient_probes + index] = index;
    }

    void SetProbeIndexOnGameThread(uint index, uint new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[index] = new_index;
    }

    HYP_FORCE_INLINE uint GetEnvProbeIndexOnGameThread(uint index) const
        { return indirect_indices[index]; }

    HYP_FORCE_INLINE const Handle<EnvProbe> &GetEnvProbeDirect(uint index) const
        { return probes[index]; }

    HYP_FORCE_INLINE const Handle<EnvProbe> &GetEnvProbeOnGameThread(uint index) const
        { return probes[indirect_indices[index]]; }

    void SetProbeIndexOnRenderThread(uint index, uint new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[max_bound_ambient_probes + index] = new_index;
    }

    HYP_FORCE_INLINE uint GetEnvProbeIndexOnRenderThread(uint index) const
        { return indirect_indices[max_bound_ambient_probes + index]; }
    
    HYP_FORCE_INLINE const Handle<EnvProbe> &GetEnvProbeOnRenderThread(uint index) const
        { return probes[indirect_indices[max_bound_ambient_probes + index]]; }
};

struct EnvGridOptions
{
    EnvGridType             type = ENV_GRID_TYPE_SH;
    BoundingBox             aabb;
    Vec3u                   density = Vec3u::Zero();
    EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;
};

HYP_CLASS()
class HYP_API EnvGrid : public RenderComponentBase
{
    HYP_OBJECT_BODY(EnvGrid);

public:
    EnvGrid(Name name, EnvGridOptions options);
    EnvGrid(const EnvGrid &other)               = delete;
    EnvGrid &operator=(const EnvGrid &other)    = delete;
    virtual ~EnvGrid();
    
    HYP_FORCE_INLINE EnvGridType GetEnvGridType() const
        { return m_options.type; }
    
    HYP_METHOD(Property="AABB", Editor=true, Label="EnvGrid Area Bounds", Description="The area that will be considered for inclusion in the EnvGrid")
    const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_METHOD(Property="AABB", Editor=true)
    void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; }

    void SetCameraData(const BoundingBox &aabb, const Vec3f &camera_position);

private:
    virtual void Init() override;
    virtual void InitGame() override; // init on game thread
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(Frame *frame) override;

    HYP_FORCE_INLINE Vec3f SizeOfProbe() const
        { return m_aabb.GetExtent() / Vec3f(m_options.density); }
    
    HYP_FORCE_INLINE EnvProbeType GetEnvProbeType() const
    {
        switch (GetEnvGridType()) {
        case ENV_GRID_TYPE_SH:
            return ENV_PROBE_TYPE_AMBIENT;
        default:
            return ENV_PROBE_TYPE_INVALID;
        }
    }

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    void CreateShader();
    void CreateFramebuffer();

    void CreateVoxelGridData();

    void CreateSHData();

    void RenderEnvProbe(
        Frame *frame,
        uint32 probe_index
    );

    void ComputeSH(
        Frame *frame,
        const ImageRef &image,
        const ImageViewRef &image_view,
        uint32 probe_index
    );

    void OffsetVoxelGrid(
        Frame *frame,
        Vec3i offset
    );

    void VoxelizeProbe(
        Frame *frame,
        uint32 probe_index
    );

    const EnvGridOptions        m_options;

    BoundingBox                 m_aabb;
    BoundingBox                 m_voxel_grid_aabb;
    Vec3f                       m_offset;
    
    Handle<Camera>              m_camera;
    RenderCollector             m_render_collector;

    ShaderRef                   m_ambient_shader;
    FramebufferRef              m_framebuffer;

    EnvProbeCollection          m_env_probe_collection;

    EnvGridShaderData           m_shader_data;
    uint                        m_current_probe_index;

    ComputePipelineRef          m_clear_sh;
    ComputePipelineRef          m_compute_sh;
    ComputePipelineRef          m_reduce_sh;
    ComputePipelineRef          m_finalize_sh;

    Array<DescriptorTableRef>   m_compute_sh_descriptor_tables;
    Array<GPUBufferRef>         m_sh_tiles_buffers;

    Handle<Texture>             m_probe_data_texture;

    ComputePipelineRef          m_clear_voxels;
    ComputePipelineRef          m_voxelize_probe;
    ComputePipelineRef          m_offset_voxel_grid;
    ComputePipelineRef          m_generate_voxel_grid_mipmaps;
    
    Handle<Texture>             m_voxel_grid_texture;

    Array<ImageViewRef>         m_voxel_grid_mips;
    Array<DescriptorTableRef>   m_generate_voxel_grid_mipmaps_descriptor_tables;

    Queue<uint>                 m_next_render_indices;
};

} // namespace hyperion

#endif // !HYPERION_ENV_PROBE_HPP