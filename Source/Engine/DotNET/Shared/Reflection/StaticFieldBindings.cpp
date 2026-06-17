/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Reflection/StaticField.hpp>

#include <Core/Name/Name.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void StaticField_GetName(const StaticField* pStaticField, Name* pOutName)
    {
        if (!pStaticField || !pOutName)
        {
            return;
        }

        *pOutName = pStaticField->GetName();
    }

    HYP_EXPORT void StaticField_GetTypeId(const StaticField* pStaticField, TypeId* pOutTypeId)
    {
        if (!pStaticField || !pOutTypeId)
        {
            return;
        }

        *pOutTypeId = pStaticField->GetTypeId();
    }

    HYP_EXPORT void StaticField_Get(const StaticField* pStaticField, BoxedValue* pOutBoxed)
    {
        Assert(pStaticField != nullptr);
        Assert(pOutBoxed != nullptr);

        *pOutBoxed = pStaticField->Get();
    }

    HYP_EXPORT void* StaticField_GetDataPointer(const StaticField* pStaticField)
    {
        Assert(pStaticField != nullptr);

        return pStaticField->GetDataPointer();
    }

    HYP_EXPORT const ClassAttribute* StaticField_GetAttribute(const StaticField* pStaticField, const Name* name)
    {
        if (!pStaticField || !name)
        {
            return nullptr;
        }

        auto it = pStaticField->GetAttributes().Find(StringHash(*name));

        if (it == pStaticField->GetAttributes().End())
        {
            return nullptr;
        }

        return &*it;
    }

    HYP_EXPORT uint32 StaticField_GetAttributes(const StaticField* pStaticField, const ClassAttribute** outAttributes)
    {
        if (!pStaticField)
        {
            return 0;
        }

        const ClassAttributeSet& attributes = pStaticField->GetAttributes();

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
