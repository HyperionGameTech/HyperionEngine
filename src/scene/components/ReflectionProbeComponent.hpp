/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>

#include <core/reflection/Handle.hpp>
#include <core/reflection/HypObjectMacros.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/Camera.hpp>

#include <core/math/Mat4f.hpp>
#include <core/math/Extent.hpp>
#include <core/math/Vector3.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class EnvProbe;
class ReflectionProbeRenderer;

HYP_STRUCT(Component, Size = 24, Label = "Reflection Probe Component", Description = "Handles cubemap reflection calculations for a single EnvProbe source", Editor = true)
struct ReflectionProbeComponent
{
    HYP_STRUCT_BODY(ReflectionProbeComponent);

    HYP_FIELD(Property = "Dimensions", Editor, Label = "Dimensions")
    Vec2u dimensions = Vec2u { 256, 256 };

    HYP_FIELD(Property = "EnvProbe", Editor, Label = "EnvProbe")
    Handle<EnvProbe> envProbe;

    HYP_FIELD(Property = "ReflectionProbeRenderer", NoScriptBindings, Transient, Editor = false)
    RC<ReflectionProbeRenderer> reflectionProbeRenderer;
};

} // namespace hyperion
