#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include <script/ScriptApi.hpp>

namespace hyperion {

class ScriptFunctions {
public:
    static HYP_SCRIPT_FUNCTION(Vector3Add);
    static HYP_SCRIPT_FUNCTION(Vector3Add2);
    static HYP_SCRIPT_FUNCTION(Vector3Init);

    static HYP_SCRIPT_FUNCTION(ArraySize);
    static HYP_SCRIPT_FUNCTION(ArrayPush);
    static HYP_SCRIPT_FUNCTION(ArrayPop);


    static HYP_SCRIPT_FUNCTION(Puts);
    static HYP_SCRIPT_FUNCTION(ToString);
    static HYP_SCRIPT_FUNCTION(Format);
    static HYP_SCRIPT_FUNCTION(Print);
    static HYP_SCRIPT_FUNCTION(Malloc);
    static HYP_SCRIPT_FUNCTION(Free);

    static void Build(APIInstance &api_instance);
};

} // namespace hyperion

#endif
