#pragma once

#include <core/Types.hpp>

namespace Hyperion {

enum AccessMode : int
{
    ACCESS_MODE_LOAD = 1,
    ACCESS_MODE_STORE = 2
};

enum IdentifierType : int
{
    IDENTIFIER_TYPE_UNKNOWN = -1,
    IDENTIFIER_TYPE_NOT_FOUND = 0,
    IDENTIFIER_TYPE_VARIABLE,
    IDENTIFIER_TYPE_MODULE,
    IDENTIFIER_TYPE_TYPE
};

} // namespace Hyperion
