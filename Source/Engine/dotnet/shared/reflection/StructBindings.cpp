/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/reflection/Struct.hpp>
#include <Core/reflection/Class.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/logging/Logger.hpp>

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Object);

extern "C"
{

    HYP_EXPORT Struct* Struct_CreateDynamicStruct(
        const TypeId* pTypeId,
        const char* pTypeName,
        uint32 size,
        decltype(DynamicStructInstanceFunctions::copy) copyFunction,
        decltype(DynamicStructInstanceFunctions::destruct) destructFunction)
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

        DynamicStructInstanceFunctions functions {};
        functions.construct = nullptr; // not needed when initializing from C#.
        functions.copy = copyFunction;
        functions.destruct = destructFunction;

        return new DynamicStructInstance(
            *pTypeId,
            CreateNameFromDynamicString(pTypeName),
            size,
            Span<const ClassAttribute>(),
            ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
            Span<MemberVariant>(),
            functions);
    }

    HYP_EXPORT void Struct_DestroyDynamicStruct(Struct* pStruct)
    {
        Assert(pStruct != nullptr);

        static_cast<DynamicStructInstance*>(pStruct)->Release();
    }

} // extern "C"

} // namespace Hyperion
