#ifndef HYPERION_V2_RENDER_COMPONENT_H
#define HYPERION_V2_RENDER_COMPONENT_H

#include "Base.hpp"
#include "Buffers.hpp"
#include "ShaderGlobals.hpp"
#include <GameCounter.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <scene/Entity.hpp>
#include <math/MathUtil.hpp>
#include <Constants.hpp>
#include <Threads.hpp>
#include <Types.hpp>

#include <atomic>
#include <type_traits>

namespace hyperion::renderer {

class Frame;

} // namespace renderer

namespace hyperion::v2 {

class Engine;
class RenderEnvironment;

using renderer::Frame;

enum RenderComponentName : UInt {
    RENDER_COMPONENT_VCT,
    RENDER_COMPONENT_SHADOWS,
    RENDER_COMPONENT_CUBEMAP,
    RENDER_COMPONENT_DDGI
};

class RenderComponentBase {
public:
    using Index = UInt;

    /*! @param render_frame_slicing Number of frames to wait between render calls */
    RenderComponentBase(RenderComponentName name, UInt render_frame_slicing = 0)
        : m_name(name),
          m_render_frame_slicing(MathUtil::NextMultiple(render_frame_slicing, max_frames_in_flight)),
          m_render_frame_slicing_counter(MathUtil::MaxSafeValue<UInt>()),
          m_index(~0u),
          m_parent(nullptr)
    {
    }

    RenderComponentBase(const RenderComponentBase &other) = delete;
    RenderComponentBase &operator=(const RenderComponentBase &other) = delete;
    virtual ~RenderComponentBase() = default;

    RenderComponentName GetName() const         { return m_name; }

    RenderEnvironment *GetParent() const        { return m_parent; }
    void SetParent(RenderEnvironment *parent)   { m_parent = parent; }

    bool IsValidComponent() const               { return m_index != ~0u; }

    Index GetComponentIndex() const             { return m_index; }
    virtual void SetComponentIndex(Index index) { m_index = index; }

    /*! \brief Init the component. Runs in RENDER/MAIN thread. */
    virtual void ComponentInit(Engine *engine) = 0;
    /*! \brief Update data for the component. Runs on GAME thread. */
    virtual void ComponentUpdate(Engine *engine, GameCounter::TickUnit delta) {}
    /*! \brief Perform rendering. Runs in RENDER thread. */
    virtual void ComponentRender(Engine *engine, Frame *frame) = 0;

    /*! \brief Called when an entity is added to the parent scene. Runs in RENDER thread. */
    virtual void OnEntityAdded(Ref<Entity> &entity) = 0;
    /*! \brief Called when an entity is removed from the parent scene. Runs in RENDER thread. */
    virtual void OnEntityRemoved(Ref<Entity> &entity) = 0;
    /*! \brief Called when an entity has meaningful attributes changed. Runs in RENDER thread. */
    virtual void OnEntityRenderableAttributesChanged(Ref<Entity> &entity) = 0;

protected:
    RenderComponentName m_name;
    const UInt          m_render_frame_slicing; // amount of frames to wait between render calls
    UInt                m_render_frame_slicing_counter; // amount of frames to wait between render calls
    Index               m_index;
    RenderEnvironment  *m_parent;
};

template <class Derived>
class RenderComponent : public RenderComponentBase {
public:
    /* Derived class must have static component_name member of type RenderComponentName, to forward to RenderComponentBase. */
    RenderComponent(UInt render_frame_slicing = 0)
        : RenderComponentBase(Derived::component_name, render_frame_slicing),
          m_component_is_render_init(false),
          m_component_is_game_init(false)
    {
    }

    RenderComponent(const RenderComponent &other) = delete;
    RenderComponent &operator=(const RenderComponent &other) = delete;
    virtual ~RenderComponent() override = default;

    bool IsComponentRenderInit() const { return m_component_is_render_init; }
    bool IsComponentGameInit() const   { return m_component_is_game_init; }

    virtual void SetComponentIndex(Index index) override final
    {
        if (index == m_index) {
            return;
        }

        const auto prev_index = m_index;

        RenderComponentBase::SetComponentIndex(index);

        if (m_component_is_render_init) {
            OnComponentIndexChanged(index, prev_index);
        }
    }
    
    virtual void ComponentInit(Engine *engine) override final
    {
        Threads::AssertOnThread(THREAD_RENDER);

        static_cast<Derived *>(this)->Init(engine);

        m_component_is_render_init = true;
    }

    virtual void ComponentUpdate(Engine *engine, GameCounter::TickUnit delta) override final
    {
        Threads::AssertOnThread(THREAD_GAME);

        if (!m_component_is_game_init) {
            static_cast<Derived *>(this)->InitGame(engine);

            m_component_is_game_init = true;
        }

        static_cast<Derived *>(this)->OnUpdate(engine, delta);
    }

    virtual void ComponentRender(Engine *engine, Frame *frame) override final
    {
        Threads::AssertOnThread(THREAD_RENDER);

        if (m_render_frame_slicing_counter >= m_render_frame_slicing) {
            static_cast<Derived *>(this)->OnRender(engine, frame);

            m_render_frame_slicing_counter = 0;
        } else {
            ++m_render_frame_slicing_counter;
        }
    }

    virtual void OnEntityAdded(Ref<Entity> &entity) override {}
    virtual void OnEntityRemoved(Ref<Entity> &entity) override {}
    virtual void OnEntityRenderableAttributesChanged(Ref<Entity> &entity) override {}

protected:
    virtual void OnComponentIndexChanged(Index new_index, Index prev_index) = 0;

private:
    bool m_component_is_render_init,
         m_component_is_game_init;
};

} // namespace hyperion::v2

#endif