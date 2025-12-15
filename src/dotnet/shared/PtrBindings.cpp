/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/TypeInfoFwd.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Ptr_Get(const TypeInfo* pTypeInfo, void* pObject, ValueStorage<HypData>* outHypData)
    {
        Assert(outHypData != nullptr);

        outHypData->Construct(AnyRef(pTypeInfo, pObject));
    }

} // extern "C"
