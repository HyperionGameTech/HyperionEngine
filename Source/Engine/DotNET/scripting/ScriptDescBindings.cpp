/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Scripting/Script.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT ScriptDesc* ScriptDesc_AllocateNativeObject(ScriptDesc* scriptDesc)
    {
        if (!scriptDesc)
        {
            return nullptr;
        }

        return new ScriptDesc(*scriptDesc);
    }

    HYP_EXPORT void ScriptDesc_FreeNativeObject(ScriptDesc* scriptDesc)
    {
        if (!scriptDesc)
        {
            return;
        }

        delete scriptDesc;
    }
} // extern "C"
