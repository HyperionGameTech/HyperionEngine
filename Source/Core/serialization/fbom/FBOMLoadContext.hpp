/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/containers/HashMap.hpp>

#include <Core/memory/allocator/ArenaAllocator.hpp>

#include <Core/utilities/Uuid.hpp>

#include <Core/serialization/fbom/FBOMObjectLibrary.hpp>

namespace Hyperion {
namespace serialization {

/// \todo : Use GlobalContext interface so we can use ArenaAllocator while loading
class FBOMLoadContext
{
public:
    HashMap<UUID, FBOMObjectLibrary> objectLibraries;
};

} // namespace serialization
} // namespace Hyperion
