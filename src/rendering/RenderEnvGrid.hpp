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
class RenderView;
class RenderScene;

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

class RenderEnvGrid final : public RenderResourceBase
{
public:
    RenderEnvGrid(EnvGrid* env_grid);
    virtual ~RenderEnvGrid() override;

    HYP_FORCE_INLINE EnvGrid* GetEnvGrid() const
    {
        return m_env_grid;
    }

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    HYP_FORCE_INLINE const FramebufferRef& GetFramebuffer() const
    {
        return m_framebuffer;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const EnvGridShaderData& GetBufferData() const
    {
        return m_buffer_data;
    }

    void SetBufferData(const EnvGridShaderData& buffer_data);

    void SetAABB(const BoundingBox& aabb);

    void SetProbeIndices(Array<uint32>&& indices);

    HYP_FORCE_INLINE const TResourceHandle<RenderCamera>& GetCameraRenderResourceHandle() const
    {
        return m_render_camera;
    }

    void SetCameraResourceHandle(TResourceHandle<RenderCamera>&& render_camera);

    HYP_FORCE_INLINE const TResourceHandle<RenderScene>& GetSceneRenderResourceHandle() const
    {
        return m_render_scene;
    }

    void SetSceneResourceHandle(TResourceHandle<RenderScene>&& render_scene);

    HYP_FORCE_INLINE const TResourceHandle<RenderView>& GetViewRenderResourceHandle() const
    {
        return m_render_view;
    }

    void SetViewResourceHandle(TResourceHandle<RenderView>&& render_view);

    void Render(FrameBase* frame, const RenderSetup& render_setup);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const override;

private:
    void CreateShader();
    void CreateFramebuffer();

    void CreateVoxelGridData();
    void CreateSphericalHarmonicsData();
    void CreateLightFieldData();

    void UpdateBufferData();

    void RenderProbe(FrameBase* frame, const RenderSetup& render_setup, uint32 probe_index);

    void ComputeEnvProbeIrradiance_SphericalHarmonics(FrameBase* frame, const RenderSetup& render_setup, const Handle<EnvProbe>& probe);
    void ComputeEnvProbeIrradiance_LightField(FrameBase* frame, const RenderSetup& render_setup, const Handle<EnvProbe>& probe);

    void OffsetVoxelGrid(FrameBase* frame, Vec3i offset);
    void VoxelizeProbe(FrameBase* frame, uint32 probe_index);

    EnvGrid* m_env_grid;

    EnvGridShaderData m_buffer_data;

    TResourceHandle<RenderCamera> m_render_camera;
    TResourceHandle<RenderScene> m_render_scene;
    TResourceHandle<RenderView> m_render_view;

    ShaderRef m_shader;
    FramebufferRef m_framebuffer;

    uint32 m_current_probe_index;

    ComputePipelineRef m_clear_sh;
    ComputePipelineRef m_compute_sh;
    ComputePipelineRef m_reduce_sh;
    ComputePipelineRef m_finalize_sh;

    Array<DescriptorTableRef> m_compute_sh_descriptor_tables;
    Array<GPUBufferRef> m_sh_tiles_buffers;

    ComputePipelineRef m_clear_voxels;
    ComputePipelineRef m_voxelize_probe;
    ComputePipelineRef m_offset_voxel_grid;
    ComputePipelineRef m_generate_voxel_grid_mipmaps;

    Handle<Texture> m_voxel_grid_texture;

    Array<ImageViewRef> m_voxel_grid_mips;
    Array<DescriptorTableRef> m_generate_voxel_grid_mipmaps_descriptor_tables;

    Handle<Texture> m_irradiance_texture;
    Handle<Texture> m_depth_texture;
    Array<GPUBufferRef> m_uniform_buffers;

    ComputePipelineRef m_compute_irradiance;
    ComputePipelineRef m_compute_filtered_depth;
    ComputePipelineRef m_copy_border_texels;

    Queue<uint32> m_next_render_indices;

    HashCode m_octant_hash_code;
};

} // namespace hyperion

#endif
