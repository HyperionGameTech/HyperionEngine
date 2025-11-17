/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/HashMap.hpp>

#include <rendering/Renderer.hpp>
#include <rendering/RenderableAttributes.hpp>

namespace hyperion {

class Texture;
class Mesh;
class ParticleVolume;
class RenderProxyParticleVolume;

class ParticleVolumeRenderer : public RendererBase
{
public:
    ParticleVolumeRenderer();
    ~ParticleVolumeRenderer() override;

    void Initialize() override;
    void Shutdown() override;

    void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) override;

protected:
    Handle<PassData> CreateViewPassData(View* view, PassDataExt& ext) override;

private:
    struct VolumeState
    {
        GpuBufferRef particleBuffer; // SSBO of ParticleShaderData
        GpuBufferRef indirectBuffer; // struct IndirectDrawCommand
        GpuBufferRef noiseBuffer;    // float 128*128

        ComputePipelineRef updatePipeline;
        GraphicsPipelineCacheHandle graphicsPipelineHandle;
        DescriptorTableRef graphicsDescriptorTable;
        DescriptorTableRef computeDescriptorTable;

        ShaderRef particleShader;
        ShaderRef updateShader;
        RenderableAttributeSet renderableAttributes;

        SizeType maxParticles = 0;
        bool hasPhysics = false;
    };

    struct Staging
    {
        Handle<Mesh> quadMesh;         // shared quad
        GpuBufferRef zeroIndirectArgs; // staging buffer with zeroed indirect args
    };

    VolumeState& EnsureVolumeState(RenderProxyParticleVolume* proxy);
    void EnsureStaging();

    HashMap<ObjId<ParticleVolume>, VolumeState> m_volumeStates;
    Staging m_staging;

    uint32 m_counter = 0u;
};

} // namespace hyperion
