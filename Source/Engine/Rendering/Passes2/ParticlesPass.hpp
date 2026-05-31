/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Map.hpp>

#include <Rendering/Pass.hpp>
#include <Rendering/RenderableAttributes.hpp>

namespace Hyperion {

class Texture;
class Mesh;
class ParticleVolume;
class RenderProxyParticleVolume;

class ParticlesPass : public PassBase
{
public:
    ParticlesPass();
    ~ParticlesPass() override;

    void Initialize() override;
    void Shutdown() override;

    void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

    int RunCleanupCycle(int maxIter = 10) override;

protected:
    PassData* CreateViewPassData(View* view, PassDataExt& ext) override;

private:
    struct VolumeState
    {
        GpuBufferRef particleBuffer; // RWStructuredBuffer of ParticleShaderData
        GpuBufferRef indirectBuffer; // struct IndirectDrawCommand
        Handle<Texture> noiseMap;    // 128x128

        RenderableAttributeSet renderableAttributes;

        size_t maxParticles = 0;
        bool hasPhysics = false;

        // last frame this volume was used for rendering
        uint32 fc = uint32(-1);

        ~VolumeState();
    };

    VolumeState& EnsureVolumeState(RenderProxyParticleVolume* proxy);

    TMap<ObjId<ParticleVolume>, VolumeState> m_volumeStates;

    uint32 m_counter = 0u;
};

} // namespace Hyperion
