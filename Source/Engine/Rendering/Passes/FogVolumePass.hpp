/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class GBuffer;
class FogVolume;
class Texture;
class Mesh;

class FogVolumePass final : public FullScreenPass
{
public:
    FogVolumePass(Vec2u extent, GBuffer* gbuffer);

    FogVolumePass(const FogVolumePass& other) = delete;
    FogVolumePass& operator=(const FogVolumePass& other) = delete;

    virtual ~FogVolumePass() override;

    HYP_FORCE_INLINE Texture* GetUpsampledResultTexture() const
    {
        return m_upsamplePasses[NumUpsamplePasses - 1]->GetAttachment(0);
    }

    virtual void Create() override;
    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    // Since we render quarter res, we double then double again:
    static constexpr uint32 NumUpsamplePasses = 2;

    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderCheckerboarded() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u newSize) override;

    struct FogVolumePassData
    {
        class FogVolume* volume = nullptr;
        Texture* volumeTexture = nullptr;
        Texture* noiseTexture = nullptr;
    };

    FogVolumePassData& GetFogVolumePassData(FogVolume* fogVolume)
    {
        auto it = m_fogVolumePassData.FindIf(
            [fogVolume](auto& item)
            {
                return item.volume == fogVolume;
            });

        if (it != m_fogVolumePassData.End())
        {
            return *it;
        }

        it = &m_fogVolumePassData.EmplaceBack();
        it->volume = fogVolume;

        return *it;
    }

    Array<FogVolumePassData, RenderAllocator> m_fogVolumePassData;
    Handle<Mesh> m_volumeMesh;

    UniquePtr<FullScreenPass> m_upsamplePasses[NumUpsamplePasses];
};

} // namespace Hyperion
