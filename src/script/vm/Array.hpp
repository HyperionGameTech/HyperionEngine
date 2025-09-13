#pragma once

#include <script/vm/Value.hpp>
#include <core/containers/Array.hpp>

#include <sstream>

namespace hyperion {

using Script_Array = Array<Script_Value>;

void GetRepresentation(
    const Script_Array& value,
    std::stringstream& ss,
    bool addTypeName = true,
    int depth = 3);

} // namespace hyperion
