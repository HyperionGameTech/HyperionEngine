/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/Method.hpp>
#include <core/reflection/ClassRegistry.hpp>
#include <core/reflection/HypObject.hpp>
#include <core/Name.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/ManagedObject.hpp>

using namespace hyperion;

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

    HYP_EXPORT bool Method_Invoke(const Method* method, HypData* args, uint32 numArgs, HypData* outResult)
    {
        if (!method || !outResult)
        {
            return false;
        }

        if (numArgs != 0 && !args)
        {
            return false;
        }

        Span<HypData> argsView(args, numArgs);

        *outResult = method->Invoke(argsView);

        return true;
    }

} // extern "C"
