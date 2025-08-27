/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypStruct.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/Defines.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Object);

extern "C"
{

    HYP_EXPORT HypStruct* HypStruct_CreateDynamicHypStruct(
        const TypeId* typeId,
        const char* typeName,
        uint32 size,
        DynamicHypStructInstance_CopyFunction copyFunction,
        DynamicHypStructInstance_DestructFunction destructFunction)
    {
        Assert(typeId != nullptr);
        Assert(typeName != nullptr);
        Assert(copyFunction != nullptr);
        Assert(destructFunction != nullptr);

        if (size == 0)
        {
            HYP_LOG(Object, Error, "Cannot create HypStruct with size 0");

            return nullptr;
        }

        return new DynamicHypStructInstance(
            *typeId,
            CreateNameFromDynamicString(typeName),
            size,
            Span<const HypClassAttribute>(),
            HypClassFlags::STRUCT_TYPE | HypClassFlags::DYNAMIC,
            Span<HypMember>(),
            copyFunction,
            destructFunction);
    }

    HYP_EXPORT void HypStruct_DestroyDynamicHypStruct(HypStruct* hypStruct)
    {
        Assert(hypStruct != nullptr);

        delete hypStruct;
    }

} // extern "C"

} // namespace hyperion