/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SKY_COMPONENT_HPP
#define HYPERION_ECS_SKY_COMPONENT_HPP

#include <core/Handle.hpp>

#include <HashCode.hpp>

namespace hyperion {

class SkydomeRenderer;

HYP_STRUCT(Component, Label="Sky Component", Description="Controls the rendering of a dynamic skydome.", Editor=true)
struct SkyComponent
{
    HYP_FIELD()
    RC<SkydomeRenderer> render_component;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode();
    }
};

} // namespace hyperion

#endif