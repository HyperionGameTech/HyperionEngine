/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/Method.hpp>
#include <Core/reflection/ClassRegistry.hpp>
#include <Core/reflection/Object.hpp>
#include <Core/name/Name.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <DotNET/ManagedObject.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void Method_GetName(const Method* method, Name* outName)
    {
        if (!method || !outName)
        {
            return;
        }

        *outName = method->GetName();
    }

    HYP_EXPORT void Method_GetReturnTypeId(const Method* method, TypeId* outReturnTypeId)
    {
        if (!method || !outReturnTypeId)
        {
            return;
        }

        *outReturnTypeId = method->GetTypeId();
    }

    HYP_EXPORT uint32 Method_GetParameters(const Method* method, const MethodParameter** outParams)
    {
        if (!method || !outParams)
        {
            return 0;
        }

        if (method->GetParameters().Empty())
        {
            return 0;
        }

        *outParams = method->GetParameters().Begin();

        return (uint32)method->GetParameters().Size();
    }

    HYP_EXPORT ubyte Method_GetFlags(const Method* method)
    {
        if (!method)
        {
            return ubyte(MethodFlags::NONE);
        }

        return ubyte(method->GetFlags());
    }

    HYP_EXPORT uint32 Method_GetAttributes(const Method* method, const ClassAttribute** outAttributes)
    {
        if (!method)
        {
            return 0;
        }

        const ClassAttributeSet& attributes = method->GetAttributes();

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

    HYP_EXPORT const ClassAttribute* Method_GetAttribute(const Method* method, const Name* name)
    {
        if (!method || !name)
        {
            return nullptr;
        }

        auto it = method->GetAttributes().Find(StringHash(*name));

        if (it == method->GetAttributes().End())
        {
            return nullptr;
        }

        return &*it;
    }

    HYP_EXPORT bool Method_Invoke(const Method* method, BoxedValue* args, uint32 numArgs, BoxedValue* outResult)
    {
        if (!method || !outResult)
        {
            return false;
        }

        if (numArgs != 0 && !args)
        {
            return false;
        }

        Span<BoxedValue> argsView(args, numArgs);

        *outResult = method->Invoke(argsView);

        return true;
    }

} // extern "C"
