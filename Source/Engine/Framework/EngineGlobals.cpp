/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Core.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {
namespace EngineGlobals {

#if HYP_EDITOR

ENGINE_API bool IsEditor()
{
    const CommandLineArguments& cliArgs = CoreApi::GetCommandLineArguments();
    
    return cliArgs["Editor"].ToBool();
}

#endif // HYP_EDITOR

} // namespace EngineGlobals
} // namespace Hyperion
