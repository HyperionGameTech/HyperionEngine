/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/Game.hpp>

namespace Hyperion {

/// Game hooks for a host that can't subclass Game (C, Rust, ...). Each is optional and runs on the sim thread.
struct HypGameCallbacks
{
    void (*onLaunch)(void* userData, Game* game);
    void (*onUpdate)(void* userData, Game* game, float delta);
    void (*onShutdown)(void* userData, Game* game);
};

HYP_CLASS()
class ENGINE_API CallbackGame final : public Game
{
    HYP_OBJECT_BODY(CallbackGame);

public:
    CallbackGame();
    virtual ~CallbackGame() override;

    void SetCallbacks(const HypGameCallbacks& callbacks, void* userData);

protected:
    virtual void OnLaunch() override;
    virtual void OnUpdate(float delta) override;
    virtual void BeforeShutdown() override;

    HypGameCallbacks m_callbacks;
    void* m_userData;
};

} // namespace Hyperion
