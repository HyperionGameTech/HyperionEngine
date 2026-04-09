/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/reflection/ClassAttribute.hpp>

#include <Core/name/Name.hpp>

#include <dotnet/ManagedObject.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT int8 ClassAttribute_GetName(ClassAttribute* pAttr, Name* pOutName)
    {
        Assert(pAttr != nullptr);
        Assert(pOutName != nullptr);

        *pOutName = pAttr->GetName();

        return true;
    }

    HYP_EXPORT int8 ClassAttribute_IsString(ClassAttribute* pAttr)
    {
        Assert(pAttr != nullptr);

        return pAttr->GetValue().IsString();
    }

    HYP_EXPORT const char* ClassAttribute_GetString(ClassAttribute* pAttr)
    {
        Assert(pAttr != nullptr);

        return pAttr->GetValue().GetString().Data();
    }

    HYP_EXPORT int8 ClassAttribute_IsBool(ClassAttribute* pAttr)
    {
        Assert(pAttr != nullptr);

        return pAttr->GetValue().IsBool();
    }

    HYP_EXPORT int8 ClassAttribute_GetBool(ClassAttribute* pAttr)
    {
        Assert(pAttr != nullptr);

        return pAttr->GetValue().GetBool();
    }

    HYP_EXPORT int8 ClassAttribute_IsInt(ClassAttribute* pAttr)
    {
        Assert(pAttr != nullptr);

        return pAttr->GetValue().IsInt();
    }

    HYP_EXPORT int8 ClassAttribute_GetInt(ClassAttribute* pAttr)
    {
        Assert(pAttr != nullptr);

        return pAttr->GetValue().GetInt();
    }

} // extern "C"
