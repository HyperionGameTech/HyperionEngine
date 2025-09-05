#pragma once

#include <string>
#include <map>
#include <fstream>

#include <core/Types.hpp>

#define HYP_SCRIPT_ENABLE_LAZY_DECLARATIONS 0
#define HYP_SCRIPT_ANY_ONLY_FUNCTION_PARAMATERS 0
#define HYP_SCRIPT_ALLOW_IDENTIFIERS_OTHER_MODULES 0
#define HYP_SCRIPT_ENABLE_BUILTIN_CONSTRUCTOR_OVERRIDE 0 // new String() => loadStr [%0, u32(0), ""]
#define HYP_SCRIPT_ENABLE_VARIABLE_INLINING 0
#define HYP_SCRIPT_AUTO_SELF_INSERTION 1

namespace hyperion {

struct Config
{
    static const SizeType maxDataMembers;
    static const char* globalModuleName;
    /** Optimize by removing unused variables */
    static bool cullUnusedObjects;
};

} // namespace hyperion
