/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class SkydomeRenderer;

HYP_STRUCT(Component, Label = "Sky Component", Description = "Controls the rendering of a dynamic skydome.", Editor = true)
struct SkyComponent
{
    HYP_FIELD(NoScriptBindings, Transient)
    Handle<SkydomeRenderer> subsystem;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode();
    }
};

} // namespace hyperion
