/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_ENV_GRID_HPP
#define HYPERION_RENDER_ENV_GRID_HPP

#include <core/Handle.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/Renderer.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class EnvGrid;
class RenderView;
class RenderScene;

class RenderEnvGrid final : public RenderResourceBase
{
public:
    RenderEnvGrid(EnvGrid* env_grid);
    virtual ~RenderEnvGrid() override;

    HYP_FORCE_INLINE EnvGrid* GetEnvGrid() const
    {
        return m_env_grid;
    }

    void SetProbeIndices(Array<uint32>&& indices);

    void Render(FrameBase* frame, const RenderSetup& render_setup);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    EnvGrid* m_env_grid;
};

struct HYP_API EnvGridPassData : PassData
{
    virtual ~EnvGridPassData() override;

    ShaderRef shader;
    FramebufferRef framebuffer;

    ComputePipelineRef clear_sh;
    ComputePipelineRef compute_sh;
    ComputePipelineRef reduce_sh;
    ComputePipelineRef finalize_sh;

    Array<DescriptorTableRef> compute_sh_descriptor_tables;
    Array<GpuBufferRef> sh_tiles_buffers;

    ComputePipelineRef clear_voxels;
    ComputePipelineRef voxelize_probe;
    ComputePipelineRef offset_voxel_grid;
    ComputePipelineRef generate_voxel_grid_mipmaps;

    Array<ImageViewRef> voxel_grid_mips;
    Array<DescriptorTableRef> generate_voxel_grid_mipmaps_descriptor_tables;

    Array<GpuBufferRef> uniform_buffers;

    ComputePipelineRef compute_irradiance;
    ComputePipelineRef compute_filtered_depth;
    ComputePipelineRef copy_border_texels;

    uint32 current_probe_index;
    Queue<uint32> next_render_indices;
};

struct EnvGridPassDataExt : PassDataExt
{
    EnvGrid* env_grid = nullptr;

    EnvGridPassDataExt()
        : PassDataExt(TypeId::ForType<EnvGridPassDataExt>())
    {
    }

    virtual ~EnvGridPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        EnvGridPassDataExt* clone = new EnvGridPassDataExt;
        clone->env_grid = env_grid;

        return clone;
    }
};

class EnvGridRenderer : public RendererBase
{
public:
    EnvGridRenderer();
    virtual ~EnvGridRenderer() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& render_setup) override final;

protected:
    void RenderProbe(FrameBase* frame, const RenderSetup& render_setup, uint32 probe_index);

    void ComputeEnvProbeIrradiance_SphericalHarmonics(FrameBase* frame, const RenderSetup& render_setup, const Handle<EnvProbe>& probe);
    void ComputeEnvProbeIrradiance_LightField(FrameBase* frame, const RenderSetup& render_setup, const Handle<EnvProbe>& probe);
    void OffsetVoxelGrid(FrameBase* frame, const RenderSetup& render_setup, Vec3i offset);
    void VoxelizeProbe(FrameBase* frame, const RenderSetup& render_setup, uint32 probe_index);

    PassData* CreateViewPassData(View* view, PassDataExt& ext) override;
    void CreateVoxelGridData(EnvGrid* env_grid, EnvGridPassData& pd);
    void CreateSphericalHarmonicsData(EnvGrid* env_grid, EnvGridPassData& pd);
    void CreateLightFieldData(EnvGrid* env_grid, EnvGridPassData& pd);
};

} // namespace hyperion

#endif
