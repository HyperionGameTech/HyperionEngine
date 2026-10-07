/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/CallbackGame.hpp>

#include <CallbackGame.generated.inl>

namespace Hyperion {

CallbackGame::CallbackGame()
    : Game(),
      m_callbacks {},
      m_userData(nullptr)
{
}

CallbackGame::~CallbackGame() = default;

void CallbackGame::SetCallbacks(const HypGameCallbacks& callbacks, void* userData)
{
    m_callbacks = callbacks;
    m_userData = userData;
}

void CallbackGame::OnLaunch()
{
    Game::OnLaunch();

    if (m_callbacks.onLaunch)
    {
        m_callbacks.onLaunch(m_userData, this);
    }
}

void CallbackGame::OnUpdate(float delta)
{
    Game::OnUpdate(delta);

    if (m_callbacks.onUpdate)
    {
        m_callbacks.onUpdate(m_userData, this, delta);
    }
}

void CallbackGame::BeforeShutdown()
{
    if (m_callbacks.onShutdown)
    {
        m_callbacks.onShutdown(m_userData, this);
    }

    Game::BeforeShutdown();
}

} // namespace Hyperion
