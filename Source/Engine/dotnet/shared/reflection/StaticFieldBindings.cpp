/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/reflection/StaticField.hpp>

#include <Core/name/Name.hpp>

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

} // extern "C"
