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

        Array<DynamicStructFieldDesc> fields;
        fields.Reserve(numFields);

        for (uint32 fieldIndex = 0; fieldIndex < numFields; fieldIndex++)
        {
            const ManagedDynamicStructField& field = pFields[fieldIndex];

            DynamicStructFieldDesc fieldDesc;
            fieldDesc.name = field.name ? CreateNameFromDynamicString(field.name) : Name::Invalid();
            fieldDesc.offset = field.offset;
            fieldDesc.size = field.size;
            fieldDesc.typeInfo = field.typeInfo;
            fields.PushBack(fieldDesc);
        }

        DynamicStructDesc desc;
        desc.typeId = *pTypeId;
        desc.name = CreateNameFromDynamicString(pTypeName);
        desc.size = size;
        desc.alignment = uint32(alignof(void*));
        // the managed default instance (constructor + field initializers) becomes the template for default construction
        desc.defaultValue = pDefaultValue;
        desc.fields = fields.ToSpan();

        return CreateDynamicStruct(desc);
    }

    HYP_EXPORT void Struct_DestroyDynamicStruct(Struct* pStruct)
    {
        Assert(pStruct != nullptr);

        static_cast<DynamicStructInstance*>(pStruct)->Release();
    }

} // extern "C"

} // namespace Hyperion
