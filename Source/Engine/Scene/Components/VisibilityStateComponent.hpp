/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Scene/VisibilityState.hpp>
#include <Scene/SceneOctree.hpp>

namespace Hyperion {

HYP_ENUM()
enum class VisibilityStateFlags : uint32
{
    NONE = 0x0,
    ALWAYS_VISIBLE = 0x1,
    INVALIDATED = 0x2
};

HYP_MAKE_ENUM_FLAGS(VisibilityStateFlags)

HYP_STRUCT(Component, Size = 32, Serialize = false, Editor = false)
struct VisibilityStateComponent
{
    HYP_STRUCT_BODY(VisibilityStateComponent);

    HYP_FIELD()
    EnumFlags<VisibilityStateFlags> flags = VisibilityStateFlags::NONE;

    HYP_FIELD()
    OctantId octantId = OctantId::Invalid();

    HYP_FIELD()
    VisibilityState* visibilityState = nullptr;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode();
    }
};

} // namespace Hyperion
