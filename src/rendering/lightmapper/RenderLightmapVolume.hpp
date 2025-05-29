/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_LIGHTMAP_VOLUME_HPP
#define HYPERION_RENDERING_LIGHTMAP_VOLUME_HPP

#include <rendering/RenderResource.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class LightmapVolume;

class RenderLightmapVolume final : public RenderResourceBase
{
public:
    RenderLightmapVolume(LightmapVolume* lightmap_volume);
    virtual ~RenderLightmapVolume() override;

    HYP_FORCE_INLINE LightmapVolume* GetLightmapVolume() const
    {
        return m_lightmap_volume;
    }

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

private:
    LightmapVolume* m_lightmap_volume;
};

} // namespace hyperion

#endif
