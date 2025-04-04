/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Ptr_Get(TypeID type_id, void *ptr, ValueStorage<HypData> *out_hyp_data)
{
    AssertThrow(out_hyp_data != nullptr);

    out_hyp_data->Construct(AnyRef(type_id, ptr));
}

} // extern "C"