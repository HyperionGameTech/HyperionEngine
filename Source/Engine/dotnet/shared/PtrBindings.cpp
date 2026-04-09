/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void Ptr_Get(const TypeInfo* pTypeInfo, void* pObject, ValueStorage<BoxedValue>* outBoxed)
    {
        Assert(outBoxed != nullptr);

        outBoxed->Construct(AnyRef(pTypeInfo, pObject));
    }

} // extern "C"
