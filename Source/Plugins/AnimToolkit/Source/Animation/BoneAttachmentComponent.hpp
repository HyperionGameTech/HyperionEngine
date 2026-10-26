/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Math/Transform.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

/*! \brief Keeps an entity on a socket or bone of the nearest skinned entity in the hierarchy. */
HYP_STRUCT(Component, Label = "Bone Attachment Component", Editor = true, NoScriptBindings)
struct ENGINE_API BoneAttachmentComponent
{
    HYP_STRUCT_BODY(BoneAttachmentComponent);

    /// a socket name, or a bone name when the skeleton has no socket by that name.
    HYP_FIELD(Property = "Socket", Serialize = true, Editor = true, EditorType = "Socket")
    Name socket;

    HYP_FIELD(Property = "Offset", Serialize = true, Editor = true)
    Transform offset;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(socket);
        hc.Add(offset);

        return hc;
    }
};

} // namespace Hyperion
