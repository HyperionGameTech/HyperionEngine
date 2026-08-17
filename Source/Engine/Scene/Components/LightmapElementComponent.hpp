/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Uuid.hpp>
#include <Core/Utilities/Pair.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/HashCode.hpp>

#include <Core/Constants.hpp>

#include <Asset/AssetPath.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

namespace Hyperion {

class LightmapVolume;

enum class LightmapElementId : uint32;
enum class LightmapVolumeId : uint32;

/*! \brief Used for Entities which have baked light via LightmapVolumes or probes to manage their state  */
HYP_STRUCT(Component)
struct ENGINE_API LightmapElementComponent
{
    HYP_STRUCT_BODY(LightmapElementComponent);

    HYP_FIELD()
    LightmapElementId lightmapElementId;

    HYP_FIELD(Transient)
    WeakHandle<LightmapVolume> lightmapVolume;

    HYP_FIELD()
    FixedArray<LightmapVolumeId, MaxLightmapVolumeAssignments> lightmapVolumeAssignments;
    
    HYP_FIELD()
    FixedArray<float, MaxLightmapVolumeAssignments> lightmapVolumeAssignmentWeights;

    LightmapElementComponent();

    LightmapVolumeId GetTopAssignment() const
    {
        return lightmapVolumeAssignments[0];
    }

    HYP_METHOD(NoScriptBindings)
    uint32 NumLightmapVolumeAssignments() const
    {
        for (uint32 i = 0; i < MaxLightmapVolumeAssignments; i++)
        {
            if (lightmapVolumeAssignments[i] == Invalid<LightmapVolumeId>)
            {
                return i;
            }
        }

        return 0;
    }
};

} // namespace Hyperion
