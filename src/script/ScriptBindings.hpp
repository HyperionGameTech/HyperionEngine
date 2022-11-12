#ifndef SCRIPT_BINDINGS_HPP
#define SCRIPT_BINDINGS_HPP

#include <script/ScriptApi.hpp>

namespace hyperion {

class ScriptBindings
{
public:
    static APIInstance::ClassBindings class_bindings;

    static HYP_SCRIPT_FUNCTION(NodeGetName);
    static HYP_SCRIPT_FUNCTION(NodeGetLocalTranslation);

    static HYP_SCRIPT_FUNCTION(Vector3ToString);

    static HYP_SCRIPT_FUNCTION(ArraySize);
    static HYP_SCRIPT_FUNCTION(ArrayPush);
    static HYP_SCRIPT_FUNCTION(ArrayPop);


    static HYP_SCRIPT_FUNCTION(Puts);
    static HYP_SCRIPT_FUNCTION(ToString);
    static HYP_SCRIPT_FUNCTION(Format);
    static HYP_SCRIPT_FUNCTION(Print);
    static HYP_SCRIPT_FUNCTION(Malloc);
    static HYP_SCRIPT_FUNCTION(Free);

    static void DeclareAll(APIInstance &api_instance);
    static void RegisterBindings(APIInstance &api_instance);

};

} // namespace hyperion

#endif
