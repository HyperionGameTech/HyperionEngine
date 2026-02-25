/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/ResourceTracker.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/memory/pool/Pool.hpp>

namespace Hyperion {

HYP_API const TypeInfo& Class_GetTypeInfo(const Class& cls)
{
    return *cls.GetTypeInfo();
}

} // namespace Hyperion
