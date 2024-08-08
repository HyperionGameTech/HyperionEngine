/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Base.hpp>

namespace hyperion {

BasicObjectBase::BasicObjectBase(BasicObjectBase &&other) noexcept
    // : m_managed_object(std::move(other.m_managed_object))
{
}

BasicObjectBase::~BasicObjectBase()
{
    // m_managed_object.Reset();
}

} // namespace hyperion