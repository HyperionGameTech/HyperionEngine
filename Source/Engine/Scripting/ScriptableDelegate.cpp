/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Method.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <DotNET/ManagedClass.hpp>

namespace Hyperion {
namespace functional {

ENGINE_API void LogScriptableDelegateError(const char* message, dotnet::ManagedObject* objectPtr)
{
    HYP_LOG(Core, Error, "ScriptableDelegate: {}", message);
}

void ScriptableDelegateHelper::InvokeMethod_Internal(BoxedValue* pOutBoxed, const Method* method, const Handle<ObjectBase>& target, Span<BoxedValue> argsBoxed)
{
    HYP_CORE_ASSERT(method != nullptr, "Method cannot be null");

    if (pOutBoxed)
    {
        *pOutBoxed = method->Invoke(argsBoxed);
    }
    else
    {
        (void)method->Invoke(argsBoxed);
    }
}

const Method* ScriptableDelegateHelper::GetClassMethod(const Class* cls, const Name* methodName)
{
    if (!cls || !methodName)
    {
        return nullptr;
    }

    return cls->GetMethod(*methodName);
}

} // namespace functional

} // namespace Hyperion
