/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/Uuid.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/serialization/fbom/FBOMObject.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

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
    Uuid uuid;
    Array<FBOMObject> objectData;

    bool TryGet(uint32 index, FBOMObject& out) const
    {
        if (index >= objectData.Size())
        {
            return false;
        }

        out = objectData[index];

        return true;
    }

    void Put(FBOMObject& object)
    {
        const uint32 nextIndex = uint32(objectData.Size());

        FBOMExternalObjectInfo* externalObjectInfo = object.GetExternalObjectInfo();
        HYP_CORE_ASSERT(externalObjectInfo != nullptr);
        HYP_CORE_ASSERT(!externalObjectInfo->IsLinked());

        externalObjectInfo->index = nextIndex;
        externalObjectInfo->libraryId = uuid;

        objectData.Resize(nextIndex + 1);

        objectData[nextIndex] = object;
    }

    void Put(FBOMObject&& object)
    {
        const uint32 nextIndex = uint32(objectData.Size());

        FBOMExternalObjectInfo* externalObjectInfo = object.GetExternalObjectInfo();
        HYP_CORE_ASSERT(externalObjectInfo != nullptr);
        HYP_CORE_ASSERT(!externalObjectInfo->IsLinked());

        externalObjectInfo->index = nextIndex;
        externalObjectInfo->libraryId = uuid;

        objectData.Resize(nextIndex + 1);

        objectData[nextIndex] = std::move(object);
    }

    // uint32 Put(FBOMData &&data)
    // {
    //     const uint32 nextIndex = uint32(objectData.Size());
    //     objectData.Resize(nextIndex + 1);

    //     objectData[nextIndex] = std::move(data);

    //     return nextIndex;
    // }

    SizeType CalculateTotalSize() const
    {
        // SizeType size = 0;

        // for (const FBOMData &data : objectData) {
        //     size += data.TotalSize();
        // }

        // return size;

        return objectData.Size();
    }
};

} // namespace serialization
} // namespace hyperion
