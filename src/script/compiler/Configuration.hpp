#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <string>
#include <map>
#include <fstream>

#include <Types.hpp>

#define HYP_SCRIPT_ENABLE_LAZY_DECLARATIONS 0
#define HYP_SCRIPT_ANY_ONLY_FUNCTION_PARAMATERS 0
#define HYP_SCRIPT_ALLOW_IDENTIFIERS_OTHER_MODULES 0
#define HYP_SCRIPT_ENABLE_BUILTIN_CONSTRUCTOR_OVERRIDE 0 // new String() => load_str [%0, u32(0), ""]
#define HYP_SCRIPT_ENABLE_VARIABLE_INLINING 1
#define HYP_SCRIPT_AUTO_SELF_INSERTION 1
#define HYP_SCRIPT_CALLABLE_CLASS_CONSTRUCTORS 1

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
