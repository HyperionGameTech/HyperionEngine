#ifndef ENUMS_HPP
#define ENUMS_HPP

#include <Types.hpp>

namespace hyperion::compiler {

enum AccessMode : Int
{
    ACCESS_MODE_LOAD = 1,
    ACCESS_MODE_STORE = 2
};

enum IdentifierType : Int
{
    IDENTIFIER_TYPE_UNKNOWN = -1,
    IDENTIFIER_TYPE_NOT_FOUND = 0,
    IDENTIFIER_TYPE_VARIABLE,
    IDENTIFIER_TYPE_MODULE,
    IDENTIFIER_TYPE_TYPE
};

} // namespace hyperion::compiler

#endif
