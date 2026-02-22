/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/utilities/Uuid.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class LightmapVolume;

enum class LightmapElementId : uint32;

HYP_STRUCT(Component, NoScriptBindings)
struct HYP_API LightmapElementComponent
{
    HYP_STRUCT_BODY(LightmapElementComponent);

    HYP_FIELD()
    LightmapElementId lightmapElementId;

    HYP_FIELD(Transient)
    WeakHandle<LightmapVolume> lightmapVolume;

    LightmapElementComponent();
};

} // namespace Hyperion
