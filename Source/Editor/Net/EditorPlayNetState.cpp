/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Net/EditorPlayNetState.hpp>

#include <Framework/EngineGlobals.hpp>

#include <EditorPlayNetState.generated.inl>

namespace Hyperion {

EditorPlayNetState::EditorPlayNetState()
    : m_playNetMode(EditorPlayNetMode::Standalone),
      m_playNetHost("127.0.0.1"),
      m_playNetPort(NetGlobals::GetGameServerPort()),
      m_playNetAutoLaunchServer(true),
      m_playNetCachePort(8081),
      m_activeNetMode(EditorPlayNetMode::Standalone),
      m_activeAutoLaunchServer(false),
      status(EditorPlayNetStatus::None)
{

}

} // namespace Hyperion
