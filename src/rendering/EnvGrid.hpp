#ifndef HYPERION_V2_ENV_GRID_HPP
#define HYPERION_V2_ENV_GRID_HPP

#include <core/Base.hpp>
#include <core/lib/AtomicVar.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/Buffers.hpp>
#include <core/Containers.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

class Entity;

using EnvGridFlags = UInt32;

enum EnvGridFlagBits : EnvGridFlags
{
    ENV_GRID_FLAGS_NONE                     = 0x0,
    ENV_GRID_FLAGS_RESET_REQUESTED          = 0x1,
    ENV_GRID_FLAGS_NEEDS_VOXEL_GRID_OFFSET  = 0x2
};

struct RenderCommand_UpdateEnvProbeAABBsInGrid;

struct EnvProbeCollection
{
    SizeType num_probes = 0;
    FixedArray<UInt, max_bound_ambient_probes * 2> indirect_indices = { 0 };
    FixedArray<Handle<EnvProbe>, max_bound_ambient_probes> probes = { };

    // Must be called on init, before render thread starts using probes too
    // returns the index
    UInt AddProbe(Handle<EnvProbe> probe)
    {
        AssertThrow(num_probes < max_bound_ambient_probes);

        const UInt index = num_probes++;

        probes[index] = std::move(probe);
        indirect_indices[index] = index;
        indirect_indices[max_bound_ambient_probes + index] = index;

        return index;
    }

    void AddProbe(UInt index, Handle<EnvProbe> probe)
    {
        AssertThrow(index < max_bound_ambient_probes);

        num_probes = MathUtil::Max(num_probes, index + 1);

        probes[index] = std::move(probe);
        indirect_indices[index] = index;
        indirect_indices[max_bound_ambient_probes + index] = index;
    }

    void SetProbeIndexOnGameThread(UInt index, UInt new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[index] = new_index;
    }

    UInt GetEnvProbeIndexOnGameThread(UInt index) const
        { return indirect_indices[index]; }

    Handle<EnvProbe> &GetEnvProbeDirect(UInt index)
        { return probes[index]; }

    const Handle<EnvProbe> &GetEnvProbeDirect(UInt index) const
        { return probes[index]; }

    Handle<EnvProbe> &GetEnvProbeOnGameThread(UInt index)
        { return probes[indirect_indices[index]]; }

    const Handle<EnvProbe> &GetEnvProbeOnGameThread(UInt index) const
        { return probes[indirect_indices[index]]; }

    void SetProbeIndexOnRenderThread(UInt index, UInt new_index)
    {
        AssertThrow(index < max_bound_ambient_probes);
        AssertThrow(new_index < max_bound_ambient_probes);

        indirect_indices[max_bound_ambient_probes + index] = new_index;
    }

    UInt GetEnvProbeIndexOnRenderThread(UInt index) const
        { return indirect_indices[max_bound_ambient_probes + index]; }
    
    Handle<EnvProbe> &GetEnvProbeOnRenderThread(UInt index)
        { return probes[indirect_indices[max_bound_ambient_probes + index]]; }
    
    const Handle<EnvProbe> &GetEnvProbeOnRenderThread(UInt index) const
        { return probes[indirect_indices[max_bound_ambient_probes + index]]; }
};

struct LightFieldStorageImage
{
    ImageRef image;
    ImageViewRef image_view;
};

class EnvGrid : public RenderComponent<EnvGrid>
{
public:
    friend struct RenderCommand_UpdateEnvProbeAABBsInGrid;

    EnvGrid(Name name, EnvGridType type, const BoundingBox &aabb, const Extent3D &density);
    EnvGrid(const EnvGrid &other) = delete;
    EnvGrid &operator=(const EnvGrid &other) = delete;
    virtual ~EnvGrid();

    HYP_FORCE_INLINE EnvGridType GetEnvGridType() const
        { return m_type; }

    void SetCameraData(const Vec3f &camera_position);

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    Vec3f SizeOfProbe() const
        { return m_aabb.GetExtent() / Vec3f(m_density); }

    EnvProbeType GetEnvProbeType() const
    {
        switch (GetEnvGridType()) {
        case ENV_GRID_TYPE_SH:
            return ENV_PROBE_TYPE_AMBIENT;
        case ENV_GRID_TYPE_LIGHT_FIELD:
            return ENV_PROBE_TYPE_LIGHT_FIELD;
        default:
            return ENV_PROBE_TYPE_INVALID;
        }
    }

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    void CreateShader();
    void CreateFramebuffer();

    void CreateVoxelGridData();

    void CreateSHData();

    void CreateLightFieldData();
    void ComputeLightFieldData(
        Frame *frame,
        UInt32 probe_index
    );

    void RenderEnvProbe(
        Frame *frame,
        UInt32 probe_index
    );

    void ComputeSH(
        Frame *frame,
        const Image *image,
        const ImageView *image_view,
        UInt32 probe_index
    );

    void OffsetVoxelGrid(
        Frame *frame,
        Vec3i offset
    );

    void VoxelizeProbe(
        Frame *frame,
        UInt32 probe_index
    );

    EnvGridType m_type;

    BoundingBox m_aabb;
    BoundingBox m_voxel_grid_aabb;
    Vec3f m_offset;
    Extent3D m_density;
    
    Handle<Camera> m_camera;
    RenderList m_render_list;

    Handle<Shader> m_ambient_shader;
    Handle<Framebuffer> m_framebuffer;
    
    // Array<Handle<EnvProbe>> m_ambient_probes;
    // Array<const EnvProbeDrawProxy *> m_env_probe_draw_proxies;

    EnvProbeCollection m_env_probe_collection;

    EnvGridShaderData m_shader_data;
    UInt m_current_probe_index;

    AtomicVar<EnvGridFlags> m_flags;

    Handle<ComputePipeline> m_compute_sh;
    Handle<ComputePipeline> m_clear_sh;
    Handle<ComputePipeline> m_finalize_sh;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_compute_sh_descriptor_sets;
    GPUBufferRef m_sh_tiles_buffer;

    Handle<ComputePipeline> m_pack_light_field_probe;
    Handle<ComputePipeline> m_copy_light_field_border_texels_irradiance;
    Handle<ComputePipeline> m_copy_light_field_border_texels_depth;
    Handle<ComputePipeline> m_clear_voxels;
    Handle<ComputePipeline> m_voxelize_probe;
    Handle<ComputePipeline> m_offset_voxel_grid;
    Handle<ComputePipeline> m_generate_voxel_grid_mipmaps;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_light_field_probe_descriptor_sets;

    Handle<Texture> m_light_field_color_texture;
    Handle<Texture> m_light_field_normals_texture;
    Handle<Texture> m_light_field_depth_texture;
    Handle<Texture> m_light_field_lowres_depth_texture;
    Handle<Texture> m_light_field_irradiance_texture;
    Handle<Texture> m_light_field_filtered_distance_texture;
    Handle<Texture> m_voxel_grid_texture;

    Array<ImageViewRef> m_voxel_grid_mips;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_voxelize_probe_descriptor_sets;
    Array<DescriptorSetRef> m_generate_voxel_grid_mipmaps_descriptor_sets;

    Queue<UInt> m_next_render_indices;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP