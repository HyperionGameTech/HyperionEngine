/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/Struct.hpp>
#include <core/reflection/Class.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Object);

extern "C"
{

    HYP_EXPORT Struct* Struct_CreateDynamicStruct(
        const TypeId* pTypeId,
        const char* pTypeName,
        uint32 size,
        DynamicStructInstance_CopyFunction copyFunction,
        DynamicStructInstance_DestructFunction destructFunction)
    {
        Assert(pTypeId != nullptr);
        Assert(pTypeName != nullptr);
        Assert(copyFunction != nullptr);
        Assert(destructFunction != nullptr);

        if (size == 0)
        {
            HYP_LOG(Object, Error, "Cannot create Struct with size 0");

            return nullptr;
        }

        return new DynamicStructInstance(
            *pTypeId,
            CreateNameFromDynamicString(pTypeName),
            size,
            Span<const ClassAttribute>(),
            ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
            Span<MemberVariant>(),
            copyFunction,
            destructFunction);
    }

    HYP_EXPORT void Struct_DestroyDynamicStruct(Struct* pStruct)
    {
        Assert(pStruct != nullptr);

        delete pStruct;
    }

} // extern "C"

} // namespace Hyperion
