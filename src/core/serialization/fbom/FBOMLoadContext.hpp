/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_LOAD_CONTEXT_HPP
#define HYPERION_FBOM_LOAD_CONTEXT_HPP

#include <core/containers/HashMap.hpp>

#include <core/utilities/UUID.hpp>

#include <core/serialization/fbom/FBOMObjectLibrary.hpp>

namespace hyperion {
namespace serialization {

class FBOMLoadContext
{
public:
    HashMap<UUID, FBOMObjectLibrary> object_libraries;
};

} // namespace serialization
} // namespace hyperion

#endif