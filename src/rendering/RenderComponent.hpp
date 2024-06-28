/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_COMPONENT_HPP
#define HYPERION_RENDER_COMPONENT_HPP

#include <core/Base.hpp>
#include <core/threading/Threads.hpp>

#include <scene/Entity.hpp>

#include <math/MathUtil.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <GameCounter.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <atomic>
#include <type_traits>

namespace hyperion {

class RenderEnvironment;

class RenderComponentBase
{
public:
    using Index = uint;

    /*! \param render_frame_slicing Number of frames to wait between render calls */
    RenderComponentBase(Name name, uint render_frame_slicing = 0)
        : m_name(name),
          m_render_frame_slicing(MathUtil::NextMultiple(render_frame_slicing, max_frames_in_flight)),
          m_render_frame_slicing_counter(0),
          m_index(~0u),
          m_parent(nullptr)
    {
    }

    RenderComponentBase(const RenderComponentBase &other)               = delete;
    RenderComponentBase &operator=(const RenderComponentBase &other)    = delete;
    virtual ~RenderComponentBase()                                      = default;

    Name GetName() const
        { return m_name; }

    RenderEnvironment *GetParent() const
        { return m_parent; }

    void SetParent(RenderEnvironment *parent)
        { m_parent = parent; }

    bool IsValidComponent() const
        { return m_index != ~0u; }

    Index GetComponentIndex() const
        { return m_index; }

    virtual void SetComponentIndex(Index index)
        { m_index = index; }

    /*! \brief Init the component. Runs in RENDER/MAIN thread. */
    virtual void ComponentInit() = 0;
    /*! \brief Update data for the component. Runs on GAME thread. */
    virtual void ComponentUpdate(GameCounter::TickUnit delta) { }
    /*! \brief Perform rendering. Runs in RENDER thread. */
    virtual void ComponentRender(Frame *frame) = 0;
    /*! \brief Called when the component is removed. */
    virtual void ComponentRemoved() = 0;

protected:
    Name                m_name;
    const uint          m_render_frame_slicing; // amount of frames to wait between render calls
    uint                m_render_frame_slicing_counter;
    Index               m_index;
    RenderEnvironment   *m_parent;
};

template <class Derived>
class RenderComponent : public RenderComponentBase
{
public:
    RenderComponent(Name name, uint render_frame_slicing = 0)
        : RenderComponentBase(name, render_frame_slicing),
          m_component_is_render_init(false),
          m_component_is_game_init(false)
    {
    }

    RenderComponent(const RenderComponent &other)               = delete;
    RenderComponent &operator=(const RenderComponent &other)    = delete;
    virtual ~RenderComponent() override                         = default;

    bool IsComponentRenderInit() const
        { return m_component_is_render_init; }

    bool IsComponentGameInit() const
        { return m_component_is_game_init; }

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
    
    virtual void ComponentInit() override final
    {
        static_cast<Derived *>(this)->Init();

        m_component_is_render_init = true;
    }

    virtual void ComponentUpdate(GameCounter::TickUnit delta) override final
    {
        Threads::AssertOnThread(ThreadName::THREAD_GAME);

        if (!m_component_is_game_init) {
            static_cast<Derived *>(this)->InitGame();

            m_component_is_game_init = true;
        }

        static_cast<Derived *>(this)->OnUpdate(delta);
    }

    virtual void ComponentRender(Frame *frame) override final
    {
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        if (m_render_frame_slicing == 0 || m_render_frame_slicing_counter++ % m_render_frame_slicing == 0) {
            static_cast<Derived *>(this)->OnRender(frame);
        }
    }

    virtual void ComponentRemoved() override { }

protected:
    virtual void OnComponentIndexChanged(Index new_index, Index prev_index) = 0;

    bool m_component_is_render_init,
        m_component_is_game_init;
};

} // namespace hyperion

#endif