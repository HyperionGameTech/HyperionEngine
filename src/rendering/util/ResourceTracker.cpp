/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/util/ResourceTracker.hpp>

#include <core/reflection/Class.hpp>

#include <core/threading/Threads.hpp>

#include <core/memory/pool/Pool.hpp>

#include <rendering/RenderMemory.hpp>

namespace hyperion {

HYP_API const TypeInfo& Class_GetTypeInfo(const Class& cls)
{
    return *cls.GetTypeInfo();
}

} // namespace hyperion