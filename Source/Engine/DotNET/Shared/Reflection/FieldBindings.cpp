/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Reflection/Field.hpp>

#include <Core/Name/Name.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void Field_GetName(const Field* field, Name* outName)
    {
        if (!field || !outName)
        {
            return;
        }

        *outName = field->GetName();
    }

    HYP_EXPORT void Field_GetTypeId(const Field* field, TypeId* outTypeId)
    {
        if (!field || !outTypeId)
        {
            return;
        }

        *outTypeId = field->GetTypeId();
    }

    HYP_EXPORT uint32 Field_GetOffset(const Field* field)
    {
        if (!field)
        {
            return 0;
        }

        return field->GetOffset();
    }

    HYP_EXPORT void Field_Get(const Field* field, const BoxedValue* targetData, BoxedValue* outData)
    {
        Assert(field != nullptr);
        Assert(targetData != nullptr);
        Assert(outData != nullptr);

        *outData = field->Get(*targetData);
    }

    HYP_EXPORT const ClassAttribute* Field_GetAttribute(const Field* field, const Name* name)
    {
        if (!field || !name)
        {
            return nullptr;
        }

        auto it = field->GetAttributes().Find(StringHash(*name));

        if (it == field->GetAttributes().End())
        {
            return nullptr;
        }

        return &*it;
    }

    HYP_EXPORT uint32 Field_GetAttributes(const Field* field, const ClassAttribute** outAttributes)
    {
        if (!field)
        {
            return 0;
        }

        const ClassAttributeSet& attributes = field->GetAttributes();

        if (!outAttributes)
        {
            return uint32(attributes.Size());
        }

        uint32 index = 0;

        for (const ClassAttribute& attribute : attributes)
        {
            outAttributes[index++] = &attribute;
        }

        return index;
    }

} // extern "C"
