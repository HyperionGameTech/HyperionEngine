/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/MemberVariant.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Object);

// Layout shared with DynamicStructField in Struct.cs
struct ManagedDynamicStructField
{
    const char* name;
    uint32 offset;
    uint32 size;
    const TypeInfo* typeInfo;
};

extern "C"
{

    HYP_EXPORT Struct* Struct_CreateDynamicStruct(
        const TypeId* pTypeId,
        const char* pTypeName,
        uint32 size,
        const void* pDefaultValue,
        const ManagedDynamicStructField* pFields,
        uint32 numFields)
    {
        Assert(pTypeId != nullptr);
        Assert(pTypeName != nullptr);
        Assert(pFields != nullptr || numFields == 0);

        if (size == 0)
        {
            HYP_LOG(Object, Error, "Cannot create Struct with size 0");

            return nullptr;
        }

        // C# dynamic structs are blittable, so the defaults (template or zero-fill construct, memcpy copy/move, no-op destruct) are exact.
        // No managed callbacks means nothing dangles if the native Struct outlives the managed DynamicStruct.
        DynamicStructInstanceFunctions functions {};

        // reflected fields make the struct visible to serialization and the editor
        Array<MemberVariant> members;
        members.Reserve(numFields);

        for (uint32 fieldIndex = 0; fieldIndex < numFields; fieldIndex++)
        {
            const ManagedDynamicStructField& field = pFields[fieldIndex];

            if (!field.name || uint64(field.offset) + uint64(field.size) > uint64(size))
            {
                HYP_LOG(Object, Warning, "Skipping invalid field {} of dynamic Struct {}", fieldIndex, pTypeName);

                continue;
            }

            DynamicStructFieldDesc fieldDesc;
            fieldDesc.name = CreateNameFromDynamicString(field.name);
            fieldDesc.offset = field.offset;
            fieldDesc.size = field.size;
            fieldDesc.typeInfo = field.typeInfo;

            MemberVariant member;

            if (MakeDynamicStructProperty(fieldDesc, int(fieldIndex), member))
            {
                members.PushBack(std::move(member));
            }
        }

        DynamicStructInstance* pStruct = new DynamicStructInstance(
            *pTypeId,
            CreateNameFromDynamicString(pTypeName),
            size,
            uint32(alignof(void*)),
            Span<const ClassAttribute>(),
            ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
            members.ToSpan(),
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
