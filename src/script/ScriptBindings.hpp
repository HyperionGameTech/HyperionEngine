#ifndef SCRIPT_BINDINGS_HPP
#define SCRIPT_BINDINGS_HPP

#include <script/ScriptApi.hpp>

namespace hyperion {

namespace scriptapi2 {
class Context;
} // namespace scriptapi2

class ScriptBindings
{
public:
    static APIInstance::ClassBindings class_bindings;

    static void DeclareAll(APIInstance &api_instance);
};

} // namespace hyperion

#endif
