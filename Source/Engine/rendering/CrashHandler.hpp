/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/RenderMemory.hpp>

namespace Hyperion {

class CrashHandler
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_rhiPool);

    CrashHandler();
    ~CrashHandler();

    void Initialize();
    void Shutdown();

    void Dump();

private:
    bool m_isInitialized;
};

} // namespace Hyperion
