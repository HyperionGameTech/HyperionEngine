/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Name/Name.hpp>

#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

class CrashHandler
{
public:
    static void Initialize();
    static void Shutdown();

    static void Dump();

    /// Register a shader's name to a hash in case of a big ol' dump involving the shader
    /// (so we can map it to the culprit)
    static void RegisterShaderBinary(uint64 binaryHash, Name name);

private:
    static bool s_isInitialized;
};

} // namespace Hyperion
