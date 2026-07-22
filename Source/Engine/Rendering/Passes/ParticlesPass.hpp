/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Map.hpp>

#include <Rendering/Pass.hpp>
#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/RenderHelpers.hpp>

namespace Hyperion {

class Texture;
class Mesh;
class ParticleVolume;
struct RenderProxyParticleVolume;

class ParticlesPass : public PassBase
{
public:
    ParticlesPass();
    ~ParticlesPass() override;

    void Initialize() override;
    void Shutdown() override;

    void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

    void OnFrameEnd(uint32 prevFrameIndex) override;

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
        bool enableCollision = false;

        // last frame this volume was used for rendering
        uint32 lastFrame = UINT32_MAX;

        ~VolumeState();
    };

    VolumeState& EnsureVolumeState(RenderProxyParticleVolume* proxy, CommandRecorder& cr);

    Map<ParticleVolume*, VolumeState, RenderAllocator> m_volumeStates;

    uint32 m_counter = 0u;
};

} // namespace Hyperion
