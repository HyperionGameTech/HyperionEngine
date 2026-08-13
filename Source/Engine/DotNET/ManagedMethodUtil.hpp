/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Utilities/Optional.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#ifdef HYP_DOTNET
#include <DotNET/ManagedObject.hpp>
#include <DotNET/ManagedClass.hpp>
#include <DotNET/ManagedMethod.hpp>
#endif // HYP_DOTNET

namespace Hyperion {

// Includers should ensure it is defined:
class ObjectBase;

template <class ReturnType, class... Args>
Optional<ReturnType> TryInvokeManagedOverride(const ObjectBase* obj, const char* methodName, Args&&... args)
{
#ifdef HYP_DOTNET
    if (ScriptObjectResource* managedObjectResource = obj->GetScriptObjectResource(); managedObjectResource && managedObjectResource->GetManagedClass())
    {
        const HashCode hashCode = HashCode::GetHashCode(methodName);

        if (dotnet::ManagedMethod* methodPtr = managedObjectResource->GetManagedClass()->GetMethodByHash(hashCode))
        {
            TResourceGuard<ScriptObjectResource> resourceGuard(*managedObjectResource);
            dotnet::ManagedObject* managedObject = managedObjectResource->GetManagedObject();

            return managedObject->InvokeMethod<ReturnType>(methodPtr, std::forward<Args>(args)...);
        }
    }
#endif // HYP_DOTNET

    return {};
}

template <class... Args>
bool TryInvokeManagedOverrideVoid(const ObjectBase* obj, const char* methodName, Args&&... args)
{
#ifdef HYP_DOTNET
    if (ScriptObjectResource* managedObjectResource = obj->GetScriptObjectResource(); managedObjectResource && managedObjectResource->GetManagedClass())
    {
        const HashCode hashCode = HashCode::GetHashCode(methodName);

        if (dotnet::ManagedMethod* methodPtr = managedObjectResource->GetManagedClass()->GetMethodByHash(hashCode))
        {
            TResourceGuard<ScriptObjectResource> resourceGuard(*managedObjectResource);
            dotnet::ManagedObject* managedObject = managedObjectResource->GetManagedObject();

            managedObject->InvokeMethod<void>(methodPtr, std::forward<Args>(args)...);

            return true;
        }
    }
#endif // HYP_DOTNET

    return false;
}

} // namespace Hyperion
