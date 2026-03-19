/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/utilities/EnumFlags.hpp>

namespace Hyperion {

HYP_ENUM()
enum class AssetPackageFlags : uint32
{
    None = 0x0,
    Transient = 0x1,    //!< Not saved to disk
    Hidden = 0x2,       //!< Hide in content browser
    SaveOnChanged = 0x4 //!< If set, will save whenever asset is added/removed
};

HYP_MAKE_ENUM_FLAGS(AssetPackageFlags);

HYP_ENUM()
enum class AddAssetConflictMode : uint8
{
    FailOnConflict = 0,
    GenerateNewName,
    ReplaceExisting,

    Default //!< per-package type default
};

HYP_ENUM()
enum class AssetObjectFlags : uint8
{
    None = 0x0,
    Persistent = 0x1,       //!< Asset is persistently loaded in memory
    Transient = 0x2,        //!< Asset is not saved to disk
    TransientByProxy = 0x4  //!< Same as above, but is transient due to parent package being transient (will change if asset is moved to a non-transient package)
};

HYP_MAKE_ENUM_FLAGS(AssetObjectFlags);

} // namespace Hyperion
