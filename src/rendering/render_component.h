#ifndef HYPERION_V2_RENDER_COMPONENT_H
#define HYPERION_V2_RENDER_COMPONENT_H

#include "base.h"
#include "buffers.h"
#include "shader_globals.h"
#include <game_counter.h>
#include <core/lib/atomic_semaphore.h>
#include <rendering/backend/renderer_frame.h>
#include <threads.h>
#include <types.h>

#include <atomic>
#include <type_traits>

namespace hyperion::v2 {

class Engine;

class RenderComponent : public EngineComponentBase<STUB_CLASS(RenderComponent)> {
public:
    RenderComponent() = default;
    RenderComponent(const RenderComponent &other) = delete;
    RenderComponent &operator=(const RenderComponent &other) = delete;
    virtual ~RenderComponent() = default;
    
    virtual void Update(Engine *engine, GameCounter::TickUnit delta) {}
    virtual void Render(Engine *engine, Frame *frame) = 0;
};

} // namespace hyperion::v2

#endif