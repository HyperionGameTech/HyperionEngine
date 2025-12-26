/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/math/Mat4f.hpp>

#include <rendering/RendererBase.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace Hyperion {

class EnvGrid;
class LegacyEnvGrid;

HYP_CLASS(NoScriptBindings)
class HYP_API EnvGridRendererPassData : public PassData
{
    HYP_OBJECT_BODY(EnvGridRendererPassData);

public:
    virtual ~EnvGridRendererPassData() override;

    ShaderRef shader;
    FramebufferRef framebuffer;

    ComputePipelineRef clearSh;
    ComputePipelineRef computeSh;
    ComputePipelineRef reduceSh;
    ComputePipelineRef finalizeSh;

    Array<DescriptorTableRef> computeShDescriptorTables;
    Array<GpuBufferRef> shTilesBuffers;

    ComputePipelineRef clearVoxels;
    ComputePipelineRef voxelizeProbe;
    ComputePipelineRef offsetVoxelGrid;
    ComputePipelineRef generateVoxelGridMipmaps;

    Array<GpuImageViewRef> voxelGridMips;
    Array<DescriptorTableRef> generateVoxelGridMipmapsDescriptorTables;

    Array<GpuBufferRef> uniformBuffers;

    ComputePipelineRef computeIrradiance;
    ComputePipelineRef computeFilteredDepth;
    ComputePipelineRef copyBorderTexels;

    uint32 currentProbeIndex;
    Queue<uint32> nextRenderIndices;
};

struct EnvGridRendererPassDataExt : PassDataExt
{
    LegacyEnvGrid* envGrid = nullptr;

    EnvGridRendererPassDataExt()
        : PassDataExt(TypeId::ForType<EnvGridRendererPassDataExt>())
    {
    }

    virtual ~EnvGridRendererPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        EnvGridRendererPassDataExt* clone = new EnvGridRendererPassDataExt;
        *clone = *this;

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

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override final;

protected:
    void RenderProbe(Frame* frame, const RenderSetup& renderSetup, uint32 probeIndex);

    void ComputeEnvProbeIrradiance_SphericalHarmonics(Frame* frame, const RenderSetup& renderSetup, EnvProbe* probe);
    void ComputeEnvProbeIrradiance_LightField(Frame* frame, const RenderSetup& renderSetup, EnvProbe* probe);
    void OffsetVoxelGrid(Frame* frame, const RenderSetup& renderSetup, Vec3i offset);
    void VoxelizeProbe(Frame* frame, const RenderSetup& renderSetup, uint32 probeIndex);

    Handle<PassData> CreateViewPassData(View* view, PassDataExt& ext) override;
    void CreateVoxelGridData(LegacyEnvGrid* envGrid, EnvGridRendererPassData& pd);
    void CreateSphericalHarmonicsData(LegacyEnvGrid* envGrid, EnvGridRendererPassData& pd);
    void CreateLightFieldData(LegacyEnvGrid* envGrid, EnvGridRendererPassData& pd);
};

} // namespace Hyperion
