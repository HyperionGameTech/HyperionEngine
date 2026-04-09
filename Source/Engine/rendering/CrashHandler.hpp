/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <rendering/RenderResult.hpp>

namespace Hyperion {

class HYP_API CrashHandler
{
public:
    CrashHandler();
    ~CrashHandler();

    void Initialize();
    void Dump();

private:
    bool m_isInitialized;
};

} // namespace Hyperion
