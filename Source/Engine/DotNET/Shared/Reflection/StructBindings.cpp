/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Object);

extern "C"
{

    HYP_EXPORT Struct* Struct_CreateDynamicStruct(
        const TypeId* pTypeId,
        const char* pTypeName,
        uint32 size,
        const void* pDefaultValue)
    {
        Assert(pTypeId != nullptr);
        Assert(pTypeName != nullptr);

        if (size == 0)
        {
            HYP_LOG(Object, Error, "Cannot create Struct with size 0");

            return nullptr;
        }

        // C# dynamic structs are blittable, so the defaults (template or zero-fill construct, memcpy copy/move, no-op destruct) are exact.
        // No managed callbacks means nothing dangles if the native Struct outlives the managed DynamicStruct.
        DynamicStructInstanceFunctions functions {};

        DynamicStructInstance* pStruct = new DynamicStructInstance(
            *pTypeId,
            CreateNameFromDynamicString(pTypeName),
            size,
            uint32(alignof(void*)),
            Span<const ClassAttribute>(),
            ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
            Span<MemberVariant>(),
            functions);

        // the managed default instance (constructor + field initializers) becomes the template for default construction
        pStruct->SetDefaultValue(pDefaultValue);

        return pStruct;
    }

    HYP_EXPORT void Struct_DestroyDynamicStruct(Struct* pStruct)
    {
        Assert(pStruct != nullptr);

        static_cast<DynamicStructInstance*>(pStruct)->Release();
    }

} // extern "C"

} // namespace Hyperion
