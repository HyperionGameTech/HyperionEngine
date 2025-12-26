/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/ResourceTracker.hpp>

#include <core/threading/Threads.hpp>

#include <core/memory/pool/Pool.hpp>

namespace Hyperion {

HYP_API const TypeInfo& Class_GetTypeInfo(const Class& cls)
{
    return *cls.GetTypeInfo();
}

} // namespace Hyperion
