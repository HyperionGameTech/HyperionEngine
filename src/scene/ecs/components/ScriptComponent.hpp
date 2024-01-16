#ifndef HYPERION_V2_ECS_SCRIPT_COMPONENT_HPP
#define HYPERION_V2_ECS_SCRIPT_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/lib/FixedArray.hpp>
#include <script/Script.hpp>
#include <HashCode.hpp>

namespace hyperion::v2 {

using ScriptComponentFlags = UInt32;

enum ScriptComponentFlagBits : ScriptComponentFlags
{
    SCRIPT_COMPONENT_FLAG_NONE  = 0x0,
    SCRIPT_COMPONENT_FLAG_INIT  = 0x1,
    SCRIPT_COMPONENT_FLAG_VALID = 0x2
};

enum ScriptComponentMethods
{
    SCRIPT_METHOD_INVALID,
    SCRIPT_METHOD_ON_ADDED,
    SCRIPT_METHOD_ON_REMOVED,
    SCRIPT_METHOD_ON_TICK,
    SCRIPT_METHOD_MAX
};

struct ScriptComponent
{
    Handle<Script>                                          script;
    String                                                  target_name;

    Script::ObjectHandle                                    target_object;
    FixedArray<Script::FunctionHandle, SCRIPT_METHOD_MAX>   script_methods;

    ScriptComponentFlags                                    flags = SCRIPT_COMPONENT_FLAG_NONE;
};

} // namespace hyperion::v2

#endif