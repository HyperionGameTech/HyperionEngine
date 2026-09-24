/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Uuid.hpp>
#include <Core/Utilities/Pair.hpp>

#include <Core/HashCode.hpp>

#include <Core/Constants.hpp>

#include <Asset/AssetPath.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

namespace Hyperion {

class LightmapVolume;

enum class LightmapElementId : uint32;
enum class LightmapVolumeId : uint32;

/*! \brief Used for Entities which have baked light via LightmapVolumes or probes to manage their state */
HYP_STRUCT(Component, Editor = false)
struct ENGINE_API LightmapElementComponent
{
    HYP_STRUCT_BODY(LightmapElementComponent);

    HYP_FIELD()
    LightmapElementId lightmapElementId;

    HYP_FIELD()
    LightmapVolumeId lightmapVolumeId;

    ///fraction of the entity's bounds inside the owning volume
    HYP_FIELD()
    float lightmapVolumeWeight = 0.0f;

    HYP_FIELD()
    uint64 meshLightmapUVHash = 0;

    HYP_FIELD(Transient)
    WeakHandle<LightmapVolume> lightmapVolume;

    LightmapElementComponent();
};

} // namespace Hyperion
