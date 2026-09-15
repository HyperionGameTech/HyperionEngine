/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/Volume.hpp>

namespace Hyperion {

struct RenderProxyEffectVolume;

/*! \brief Base for volumes that drive a rendering effect (clouds, ...). Views collect every subclass through one tracker;
 *  render passes pick out the subclass they handle with GetElements<T>(). */
HYP_CLASS(Abstract)
class ENGINE_API EffectVolume : public VolumeBase
{
    HYP_OBJECT_BODY(EffectVolume);

public:
    EffectVolume() = default;

    explicit EffectVolume(const BoundingBox& localBounds)
        : VolumeBase(localBounds)
    {
    }

    virtual ~EffectVolume() override = default;

    /*! \brief Fills in the bounds. Subclasses write their effect parameters into proxy->bufferData.params */
    virtual void UpdateRenderProxy(RenderProxyEffectVolume* proxy);
};

} // namespace Hyperion
