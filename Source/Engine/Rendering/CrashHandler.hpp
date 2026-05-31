/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

class CrashHandler
{
public:
    static void Initialize();
    static void Shutdown();

    static void Dump();

private:
    static bool s_isInitialized;
};

} // namespace Hyperion
