/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/Renderer.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace hyperion {

class EnvGrid;

HYP_CLASS(NosScriptBindings)
class HYP_API EnvGridPassData : public PassData
{
    HYP_OBJECT_BODY(EnvGridPassData);

public:
    virtual ~EnvGridPassData() override;

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

struct EnvGridPassDataExt : PassDataExt
{
    EnvGrid* envGrid = nullptr;

    EnvGridPassDataExt()
        : PassDataExt(TypeId::ForType<EnvGridPassDataExt>())
    {
    }

    virtual ~EnvGridPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        EnvGridPassDataExt* clone = new EnvGridPassDataExt;
        clone->envGrid = envGrid;

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

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) override final;

protected:
    void RenderProbe(FrameBase* frame, const RenderSetup& renderSetup, uint32 probeIndex);

    void ComputeEnvProbeIrradiance_SphericalHarmonics(FrameBase* frame, const RenderSetup& renderSetup, const Handle<EnvProbe>& probe);
    void ComputeEnvProbeIrradiance_LightField(FrameBase* frame, const RenderSetup& renderSetup, const Handle<EnvProbe>& probe);
    void OffsetVoxelGrid(FrameBase* frame, const RenderSetup& renderSetup, Vec3i offset);
    void VoxelizeProbe(FrameBase* frame, const RenderSetup& renderSetup, uint32 probeIndex);

    Handle<PassData> CreateViewPassData(View* view, PassDataExt& ext) override;
    void CreateVoxelGridData(EnvGrid* envGrid, EnvGridPassData& pd);
    void CreateSphericalHarmonicsData(EnvGrid* envGrid, EnvGridPassData& pd);
    void CreateLightFieldData(EnvGrid* envGrid, EnvGridPassData& pd);
};

} // namespace hyperion
