/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/HashMap.hpp>

#include <rendering/RendererBase.hpp>
#include <rendering/RenderableAttributes.hpp>

namespace Hyperion {

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

    void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

    int RunCleanupCycle(int maxIter = 10) override;

protected:
    Handle<PassData> CreateViewPassData(View* view, PassDataExt& ext) override;

private:
    struct VolumeState
    {
        GpuBufferRef particleBuffer; // SSBO of ParticleShaderData
        GpuBufferRef indirectBuffer; // struct IndirectDrawCommand
        FixedArray<GpuBufferRef, NumFramesInFlight> uniformBuffers; // per-frame uniform buffer for this volume
        Handle<Texture> noiseMap;    // 128x128

        RenderableAttributeSet renderableAttributes;

        SizeType maxParticles = 0;
        bool hasPhysics = false;

        // last frame this volume was used for rendering
        uint32 fc = uint32(-1);

        ~VolumeState();
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

} // namespace Hyperion
