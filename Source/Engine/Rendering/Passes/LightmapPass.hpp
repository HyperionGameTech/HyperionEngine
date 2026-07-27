/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class LightmapVolume;
class Texture;
class Mesh;

class LightmapPass final : public FullScreenPass
{
public:
    LightmapPass();
    LightmapPass(const LightmapPass& other) = delete;
    LightmapPass& operator=(const LightmapPass& other) = delete;
    virtual ~LightmapPass() override;

    virtual void Create() override;

protected:
    struct LightmapVolumePassData
    {
        class LightmapVolume* volume = nullptr;
        Array<Texture*, RenderAllocator> atlasIrradianceTextures;
        Array<Texture*, RenderAllocator> atlasRadianceTextures;
        Array<GpuBufferRef, RenderAllocator> uniformBuffers;
    };

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer) override;

    LightmapVolumePassData& GetLightmapVolumePassData(LightmapVolume* lightmapVolume)
    {
        auto it = m_lightmapVolumePassData.FindIf([lightmapVolume](auto& item)
                                                  {
                                                      return item.volume == lightmapVolume;
                                                  });

        if (it != m_lightmapVolumePassData.End())
        {
            return *it;
        }

        it = &m_lightmapVolumePassData.EmplaceBack();
        it->volume = lightmapVolume;

        return *it;
    }

    Array<LightmapVolumePassData, RenderAllocator> m_lightmapVolumePassData;

    Handle<Mesh> m_volumeMesh;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderCheckerboarded() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u newSize) override;
};

} // namespace Hyperion
