/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Scene/Entity.hpp>

namespace Hyperion {

HYP_CLASS(Abstract)
class ENGINE_API VolumeBase : public Entity
{
    HYP_OBJECT_BODY(VolumeBase);

public:
    VolumeBase() = default;

    explicit VolumeBase(Name name)
        : Entity(name)
    {
    }

    VolumeBase(Name name, const BoundingBox& localBounds)
        : Entity(name)
    {
        m_localBounds = localBounds;
    }

    explicit VolumeBase(const BoundingBox& localBounds)
    {
        m_localBounds = localBounds;
    }

    virtual ~VolumeBase() override = default;
};

} // namespace Hyperion
