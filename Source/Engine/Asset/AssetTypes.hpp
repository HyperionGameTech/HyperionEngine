/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Name/Name.hpp>

#include <Core/FileSystem/FilePath.hpp>

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
    Transient = 0x2         //!< Asset is not saved to disk
};

HYP_MAKE_ENUM_FLAGS(AssetObjectFlags);

#pragma pack(push, 1)

HYP_STRUCT()
struct AssetDesc
{
    HYP_STRUCT_BODY(AssetDesc);

    static constexpr uint32 InvalidIndex = 0;

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    uint32 index = InvalidIndex;

    HYP_FORCE_INLINE bool operator==(const AssetDesc& other) const
    {
        return Memory::Compare(this, &other, sizeof(AssetDesc)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const AssetDesc& other) const
    {
        return Memory::Compare(this, &other, sizeof(AssetDesc)) != 0;
    }
};

#pragma pack(pop)

} // namespace Hyperion
