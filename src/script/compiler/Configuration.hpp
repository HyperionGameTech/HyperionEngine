#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <string>
#include <map>
#include <fstream>

#include <Types.hpp>

#define ACE_ENABLE_LAZY_DECLARATIONS 0
#define ACE_ANY_ONLY_FUNCTION_PARAMATERS 0
#define ACE_ALLOW_IDENTIFIERS_OTHER_MODULES 0
#define ACE_ENABLE_CONFIG_FILE 0
#define ACE_ENABLE_BUILTIN_CONSTRUCTOR_OVERRIDE 0 // new String() => load_str [%0, u32(0), ""]
#define ACE_ENABLE_VARIABLE_INLINING 0
#define ACE_NEWLINES_TERMINATE_STATEMENTS 0

namespace hyperion::compiler {

struct Config
{
    static const SizeType max_data_members;
    static const char *global_module_name;
    /** Optimize by removing unused variables */
    static bool cull_unused_objects;
};

} // namespace hyperion::compiler

#endif
