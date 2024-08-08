/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Base.hpp>

namespace hyperion {

// BasicObjectBase::BasicObjectBase(const IClassInfo *class_info)
// {
//     if (const HypClass *hyp_class = class_info->GetClass()) {
//         if (dotnet::Class *managed_class = hyp_class->GetManagedClass()) {
//             m_managed_object = managed_class->NewObject();
//             // @TODO: Make new constructor for this to reduce number of invocations needed when 
//             // initializing an object.
//             m_managed_object->InvokeMethodByName<void, void *, void *>("InitializeHypObject", this, const_cast<HypClass *>(hyp_class));
//         }
//     }
// }

BasicObjectBase::BasicObjectBase(BasicObjectBase &&other) noexcept
    // : m_managed_object(std::move(other.m_managed_object))
{
}

BasicObjectBase::~BasicObjectBase()
{
    // m_managed_object.Reset();
}

} // namespace hyperion