/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/utilities/Uuid.hpp>

#include <Core/HashCode.hpp>

#include <Asset/AssetPath.hpp>

namespace Hyperion {

class LightmapVolume;

enum class LightmapElementId : uint32;

HYP_STRUCT(Component, NoScriptBindings)
struct ENGINE_API LightmapElementComponent
{
    HYP_STRUCT_BODY(LightmapElementComponent);

    HYP_FIELD()
    LightmapElementId lightmapElementId;

    HYP_FIELD()
    AssetPath lightmapVolumePath;

    HYP_FIELD(Transient)
    WeakHandle<LightmapVolume> lightmapVolume;

    LightmapElementComponent();
};

} // namespace Hyperion
