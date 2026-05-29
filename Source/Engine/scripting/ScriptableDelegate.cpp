/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/Method.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <dotnet/ManagedClass.hpp>

namespace Hyperion {

namespace functional {

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

} // namespace functional

} // namespace Hyperion
