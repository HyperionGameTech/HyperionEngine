#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region ConsoleCommandBase Reflection Data

HYP_BEGIN_CLASS(ConsoleCommandBase, 215, 1, NAME("ObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(Execute)), &ConsoleCommandBase::Execute, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } })
HYP_END_CLASS

#pragma endregion ConsoleCommandBase Reflection Data

#pragma region ConsoleCommandBase Scriptable Methods

Result ConsoleCommandBase::Execute(const CommandLineArguments & args)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Execute");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<Result>(method_ptr, args);
        }
    }

    return Execute_Impl(args);
}
#pragma endregion ConsoleCommandBase Scriptable Methods
} // namespace hyperion

