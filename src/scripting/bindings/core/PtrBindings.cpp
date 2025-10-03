/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/utilities/TypeInfoFwd.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Ptr_Get(const TypeInfo* pTypeInfo, void* pObject, ValueStorage<HypData>* outHypData)
    {
        Assert(outHypData != nullptr);

        outHypData->Construct(AnyRef(pTypeInfo, pObject));
    }

} // extern "C"