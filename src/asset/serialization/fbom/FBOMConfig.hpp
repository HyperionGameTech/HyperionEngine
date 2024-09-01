/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_CONFIG_HPP
#define HYPERION_FBOM_CONFIG_HPP

#include <core/containers/FlatMap.hpp>

#include <core/utilities/UUID.hpp>

#include <asset/serialization/fbom/FBOMObjectLibrary.hpp>

namespace hyperion {
namespace fbom {

struct FBOMConfig
{
    bool                                continue_on_external_load_error = false;
    String                              base_path;
    FlatMap<UUID, FBOMObjectLibrary>    external_data_cache;
};

} // namespace fbom
} // namespace hyperion

#endif
