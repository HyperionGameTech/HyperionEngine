/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Ptr_Get(TypeId typeId, void* ptr, ValueStorage<HypData>* outHypData)
    {
        Assert(outHypData != nullptr);

        outHypData->Construct(AnyRef(typeId, ptr));
    }

} // extern "C"