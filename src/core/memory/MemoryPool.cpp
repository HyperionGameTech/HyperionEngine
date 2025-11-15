/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/memory/MemoryPool.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {
namespace memory {

#pragma region MemoryPoolBase

MemoryPoolBase::MemoryPoolBase()
{
}

MemoryPoolBase::~MemoryPoolBase()
{
}

#pragma endregion MemoryPoolBase

} // namespace memory
} // namespace hyperion