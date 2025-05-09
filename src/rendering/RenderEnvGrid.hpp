/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_ENV_GRID_HPP
#define HYPERION_RENDER_ENV_GRID_HPP

#include <core/Handle.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class EnvGrid;
class ViewRenderResource;
class SceneRenderResource;

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


class EnvGridRenderResource final : public RenderResourceBase
{
public:
    EnvGridRenderResource(EnvGrid *env_grid);
    virtual ~EnvGridRenderResource() override;

    HYP_FORCE_INLINE EnvGrid *GetEnvGrid() const
        { return m_env_grid; }

    HYP_FORCE_INLINE const ShaderRef &GetShader() const
        { return m_shader; }

    HYP_FORCE_INLINE const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const EnvGridShaderData &GetBufferData() const
        { return m_buffer_data; }

    void SetBufferData(const EnvGridShaderData &buffer_data);

    void SetAABB(const BoundingBox &aabb);

    void SetProbeIndices(Array<uint32> &&indices);

    HYP_FORCE_INLINE const TResourceHandle<CameraRenderResource> &GetCameraRenderResourceHandle() const
        { return m_camera_render_resource_handle; }

    void SetCameraResourceHandle(TResourceHandle<CameraRenderResource> &&camera_render_resource_handle);

    HYP_FORCE_INLINE const TResourceHandle<SceneRenderResource> &GetSceneRenderResourceHandle() const
        { return m_scene_render_resource_handle; }

    void SetSceneResourceHandle(TResourceHandle<SceneRenderResource> &&scene_render_resource_handle);

    HYP_FORCE_INLINE const TResourceHandle<ViewRenderResource> &GetViewRenderResourceHandle() const
        { return m_view_render_resource_handle; }

    void SetViewResourceHandle(TResourceHandle<ViewRenderResource> &&view_render_resource_handle);
    
    void Render(FrameBase *frame);
    
protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;
    
    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;
    
private:
    void CreateShader();
    void CreateFramebuffer();

    void CreateVoxelGridData();
    void CreateSphericalHarmonicsData();
    void CreateLightFieldData();

    void UpdateBufferData();

    void RenderEnvProbe(
        FrameBase *frame,
        uint32 probe_index
    );

    void ComputeEnvProbeIrradiance_SphericalHarmonics(
        FrameBase *frame,
        const Handle<EnvProbe> &probe
    );

    void ComputeEnvProbeIrradiance_LightField(
        FrameBase *frame,
        const Handle<EnvProbe> &probe
    );

    void OffsetVoxelGrid(
        FrameBase *frame,
        Vec3i offset
    );

    void VoxelizeProbe(
        FrameBase *frame,
        uint32 probe_index
    );

    EnvGrid                                     *m_env_grid;

    EnvGridShaderData                           m_buffer_data;

    TResourceHandle<CameraRenderResource>       m_camera_render_resource_handle;
    TResourceHandle<SceneRenderResource>        m_scene_render_resource_handle;
    TResourceHandle<ViewRenderResource>         m_view_render_resource_handle;

    ShaderRef                                   m_shader;
    FramebufferRef                              m_framebuffer;

    uint32                                      m_current_probe_index;

    ComputePipelineRef                          m_clear_sh;
    ComputePipelineRef                          m_compute_sh;
    ComputePipelineRef                          m_reduce_sh;
    ComputePipelineRef                          m_finalize_sh;

    Array<DescriptorTableRef>                   m_compute_sh_descriptor_tables;
    Array<GPUBufferRef>                         m_sh_tiles_buffers;

    ComputePipelineRef                          m_clear_voxels;
    ComputePipelineRef                          m_voxelize_probe;
    ComputePipelineRef                          m_offset_voxel_grid;
    ComputePipelineRef                          m_generate_voxel_grid_mipmaps;
    
    Handle<Texture>                             m_voxel_grid_texture;

    Array<ImageViewRef>                         m_voxel_grid_mips;
    Array<DescriptorTableRef>                   m_generate_voxel_grid_mipmaps_descriptor_tables;

    Handle<Texture>                             m_irradiance_texture;
    Handle<Texture>                             m_depth_texture;
    Array<GPUBufferRef>                         m_uniform_buffers;

    ComputePipelineRef                          m_compute_irradiance;
    ComputePipelineRef                          m_compute_filtered_depth;
    ComputePipelineRef                          m_copy_border_texels;

    Queue<uint32>                               m_next_render_indices;

    HashCode                                    m_octant_hash_code;
};

} // namespace hyperion

#endif
