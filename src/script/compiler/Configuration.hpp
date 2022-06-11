#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <string>
#include <map>
#include <fstream>

#define ACE_ENABLE_BLOCK_EXPRESSIONS 1
#define ACE_ENABLE_LAZY_DECLARATIONS 0
#define ACE_ANY_ONLY_FUNCTION_PARAMATERS 0
#define ACE_ALLOW_IDENTIFIERS_OTHER_MODULES 0
#define ACE_ENABLE_CONFIG_FILE 0
#define ACE_ENABLE_BUILTIN_CONSTRUCTOR_OVERRIDE 0 // new String() => load_str [%0, u32(0), ""]

namespace hyperion::compiler {

struct Config {
    static const int max_data_members;
    static const std::string global_module_name;
    /** Store strings, functions, etc.... at
        the top of the program, or load them at the point they're needed */
    static bool use_static_objects;
    /** Optimize by removing unused variables */
    static bool cull_unused_objects;
};

} // namespace hyperion::compiler

#endif
