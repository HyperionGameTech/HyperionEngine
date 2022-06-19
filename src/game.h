#ifndef HYPERION_V2_GAME_H
#define HYPERION_V2_GAME_H

#include "game_counter.h"

namespace hyperion {

class SystemWindow;

namespace renderer {

class Frame;

} // namespace renderer

} // namespace hyperion

namespace hyperion::v2 {

using renderer::Frame;

class Engine;

class Game {
    friend class GameThread;

public:
    Game();
    virtual ~Game();

    virtual void Init(Engine *engine, SystemWindow *window);
    virtual void Teardown(Engine *engine);

    virtual void OnPostInit(Engine *engine);
    virtual void OnFrameBegin(Engine *engine, Frame *frame) = 0;
    virtual void OnFrameEnd(Engine *engine, Frame *frame) = 0;
    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) = 0;

private:
    bool m_is_init;
};

} // namespace hyperion::v2

#endif