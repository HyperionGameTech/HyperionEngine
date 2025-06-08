/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_OBJECT_LIBRARY_HPP
#define HYPERION_FBOM_OBJECT_LIBRARY_HPP

#include <core/utilities/UUID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/serialization/fbom/FBOMObject.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion {

namespace compression {
class Archive;
} // namespace compression

using compression::Archive;

enum class FBOMObjectLibraryFlags : uint8
{
    NONE = 0x0,
    LOCATION_INLINE = 0x1,
    LOCATION_EXTERNAL = 0x2,
    LOCATION_MASK = LOCATION_INLINE | LOCATION_EXTERNAL
};

HYP_MAKE_ENUM_FLAGS(FBOMObjectLibraryFlags)

namespace serialization {

struct FBOMObjectLibrary
{
    UUID uuid;
    Array<FBOMObject> object_data;

    bool TryGet(uint32 index, FBOMObject& out) const
    {
        if (index >= object_data.Size())
        {
            return false;
        }

        out = object_data[index];

        return true;
    }

    void Put(FBOMObject& object)
    {
        const uint32 next_index = uint32(object_data.Size());

        FBOMExternalObjectInfo* external_object_info = object.GetExternalObjectInfo();
        AssertThrow(external_object_info != nullptr);
        AssertThrow(!external_object_info->IsLinked());

        external_object_info->index = next_index;
        external_object_info->library_id = uuid;

        object_data.Resize(next_index + 1);

        object_data[next_index] = object;
    }

    void Put(FBOMObject&& object)
    {
        const uint32 next_index = uint32(object_data.Size());

        FBOMExternalObjectInfo* external_object_info = object.GetExternalObjectInfo();
        AssertThrow(external_object_info != nullptr);
        AssertThrow(!external_object_info->IsLinked());

        external_object_info->index = next_index;
        external_object_info->library_id = uuid;

        object_data.Resize(next_index + 1);

        object_data[next_index] = std::move(object);
    }

    // uint32 Put(FBOMData &&data)
    // {
    //     const uint32 next_index = uint32(object_data.Size());
    //     object_data.Resize(next_index + 1);

    //     object_data[next_index] = std::move(data);

    //     return next_index;
    // }

    SizeType CalculateTotalSize() const
    {
        // SizeType size = 0;

        // for (const FBOMData &data : object_data) {
        //     size += data.TotalSize();
        // }

        // return size;

        return object_data.Size();
    }
};

} // namespace serialization
} // namespace hyperion

#endif
