/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/Util.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/BoundingSphere.hpp>

namespace Hyperion {

HYP_ENUM()
enum class LightType : uint32
{
    Directional = 0,
    Point,
    Spot,
    AreaRect,

    Max
};

static constexpr LightType InvalidLightType = Invalid<LightType>;
static constexpr uint32 NumLightTypes = uint32(LightType::Max);

// clang-format off

HYP_ENUM()
enum class LightFlags : uint32
{
    None = 0x0,                                     //!< @editor=false

    ShadowCaster = 0x1,                             //!< @title="Render shadows"

    CacheStaticShadowMaps = 0x10,                   //!< @title="Cache shadow maps for static objects"
    BakeStaticShadows = 0x20,                       //!< @editor=false

    OnlyDrawStaticShadowMaps = 0x40,                //!< @title="Only render shadows for static objects"

    Default = ShadowCaster | CacheStaticShadowMaps  //!< @editor=false
};

// clang-format on

HYP_MAKE_ENUM_FLAGS(LightFlags);

struct LightCSMState
{
    /// Per-cascade last committed FC value
    FixedArray<uint32, MaxShadowMapCascades> lastCommittedFrame {};
    FixedArray<HashCode, MaxShadowMapCascades> lastComittedEntryListHashes {};

    /// The light space basis every cascade's bounds are currently fit in
    Mat4f committedViewMatrix = Mat4f::Identity();

    Vec3f lastCommittedLightDir;
    BoundingSphere lastCommittedWorldBounds;

    uint32 nextUpdateCascade = 0;

    bool basisInitialized = false;
};

} // namespace Hyperion