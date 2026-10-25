/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/ABI/ABIMarshal.hpp>

#include <DotNET/ManagedObject.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT int8 NativeBindings_GetManagedObject(
        const Class* cls,
        void* objectPtr,
        dotnet::ObjectReference* outObjectReference)
    {
        if (!cls || !objectPtr || !outObjectReference)
        {
            return 0;
        }

        return cls->GetManagedObject(objectPtr, *outObjectReference) ? 1 : 0;
    }

    HYP_EXPORT void NativeBindings_ReleaseObject(void* objectPtr)
    {
        if (objectPtr)
        {
            reinterpret_cast<const ObjectBase*>(objectPtr)->GetObjectHeader_Internal()->DecRefStrong();
        }
    }

    HYP_EXPORT void NativeBindings_FreeString(void* data)
    {
        ABI::Free(data);
    }
}
