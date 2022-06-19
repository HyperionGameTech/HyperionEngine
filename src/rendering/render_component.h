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

class RenderComponentBase {
public:
    using Index = UInt;

    RenderComponentBase()
        : m_index(~0u)
    {
    }

    RenderComponentBase(const RenderComponentBase &other) = delete;
    RenderComponentBase &operator=(const RenderComponentBase &other) = delete;
    virtual ~RenderComponentBase() = default;

    bool IsValidComponent() const               { return m_index != ~0u; }

    Index GetComponentIndex() const             { return m_index; }
    virtual void SetComponentIndex(Index index) { m_index = index; }

    virtual void ComponentInit(Engine *engine) = 0;
    virtual void ComponentUpdate(Engine *engine, GameCounter::TickUnit delta) {}
    virtual void ComponentRender(Engine *engine, Frame *frame) = 0;

protected:
    Index m_index;
};

template <class Derived>
class RenderComponent : public RenderComponentBase {
public:
    RenderComponent()
        : RenderComponentBase(),
          m_component_is_init(false)
    {
    }

    RenderComponent(const RenderComponent &other) = delete;
    RenderComponent &operator=(const RenderComponent &other) = delete;
    virtual ~RenderComponent() override = default;

    virtual void SetComponentIndex(Index index) override final
    {
        if (index == m_index) {
            return;
        }

        const auto prev_index = m_index;

        RenderComponentBase::SetComponentIndex(index);

        if (m_component_is_init) {
            OnComponentIndexChanged(index, prev_index);
        }
    }
    
    virtual void ComponentInit(Engine *engine) override final
    {
        //Threads::AssertOnThread(THREAD_RENDER);

        static_cast<Derived *>(this)->Init(engine);

        m_component_is_init = true;
    }

    virtual void ComponentUpdate(Engine *engine, GameCounter::TickUnit delta) override final
    {
        Threads::AssertOnThread(THREAD_GAME);

        static_cast<Derived *>(this)->OnUpdate(engine, delta);
    }

    virtual void ComponentRender(Engine *engine, Frame *frame) override final
    {
        Threads::AssertOnThread(THREAD_RENDER);

        static_cast<Derived *>(this)->OnRender(engine, frame);
    }

protected:
    virtual void OnComponentIndexChanged(Index new_index, Index prev_index) = 0;

private:
    bool m_component_is_init;
};

} // namespace hyperion::v2

#endif