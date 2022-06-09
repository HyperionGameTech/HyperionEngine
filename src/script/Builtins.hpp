#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include <script/ScriptApi.hpp>

namespace hyperion {

class ScriptFunctions {
public:
    static HYP_SCRIPT_FUNCTION(ArraySize);
    //static HYP_SCRIPT_FUNCTION(ArrayPush);
    //static HYP_SCRIPT_FUNCTION(ArrayPop);

    static void Build(APIInstance &api_instance);
};

} // namespace hyperion

#endif
