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
    VolumeBase()
    {
        m_nodeFlags |= NodeFlags::ExcludeFromParentBounds;
    }

    explicit VolumeBase(Name name)
        : Entity(name)
    {
        m_nodeFlags |= NodeFlags::ExcludeFromParentBounds;
    }

    VolumeBase(Name name, const BoundingBox& localBounds)
        : Entity(name)
    {
        m_localBounds = localBounds;

        m_nodeFlags |= NodeFlags::ExcludeFromParentBounds;
    }

    explicit VolumeBase(const BoundingBox& localBounds)
    {
        m_localBounds = localBounds;

        m_nodeFlags |= NodeFlags::ExcludeFromParentBounds;
    }

    virtual ~VolumeBase() override = default;

#if HYP_EDITOR
    bool useVolumeEditTool : 1 = true;
#endif // HYP_EDITOR
};

} // namespace Hyperion
