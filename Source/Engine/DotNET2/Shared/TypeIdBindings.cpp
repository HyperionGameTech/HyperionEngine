/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Debug/Debug.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT void TypeId_ForManagedType(const char* pTypeName, TypeId* pOutTypeId)
    {
        Assert(pOutTypeId != nullptr);
        *pOutTypeId = TypeId::ForManagedType(pTypeName);
    }
} // extern "C"
