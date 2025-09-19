/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/HashMap.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/serialization/fbom/FBOMObjectLibrary.hpp>

namespace hyperion {
namespace serialization {

class FBOMLoadContext
{
public:
    HashMap<Uuid, FBOMObjectLibrary> objectLibraries;
};

} // namespace serialization
} // namespace hyperion
