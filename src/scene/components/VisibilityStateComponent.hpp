/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <scene/VisibilityState.hpp>
#include <scene/SceneOctree.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

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

} // namespace hyperion
