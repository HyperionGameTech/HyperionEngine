/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_COMPONENT_HPP
#define HYPERION_RENDER_COMPONENT_HPP

#include <core/Base.hpp>

#include <core/threading/Threads.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>

#include <scene/Entity.hpp>

#include <math/MathUtil.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <GameCounter.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <atomic>
#include <type_traits>

namespace hyperion {

class RenderEnvironment;

HYP_CLASS(Abstract)
class RenderComponentBase : public EnableRefCountedPtrFromThis<RenderComponentBase>
{
    HYP_OBJECT_BODY(RenderComponentBase);

public:
    using Index = uint;

    friend class RenderEnvironment;

    /*! \param render_frame_slicing Number of frames to wait between render calls */
    RenderComponentBase(Name name, uint render_frame_slicing = 0)
        : m_name(name),
          m_render_frame_slicing(MathUtil::NextMultiple(render_frame_slicing, max_frames_in_flight)),
          m_render_frame_slicing_counter(0),
          m_index(~0u),
          m_parent(nullptr),
          m_is_initialized(0)
    {
    }

    RenderComponentBase(const RenderComponentBase &other)               = delete;
    RenderComponentBase &operator=(const RenderComponentBase &other)    = delete;
    virtual ~RenderComponentBase()                                      = default;

    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    HYP_FORCE_INLINE RenderEnvironment *GetParent() const
        { return m_parent; }

    HYP_FORCE_INLINE bool IsValidComponent() const
        { return m_index != ~0u; }

    HYP_FORCE_INLINE Index GetComponentIndex() const
        { return m_index; }

    void SetComponentIndex(Index index)
    {
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        if (index == m_index) {
            return;
        }

        const auto prev_index = m_index;

        m_index = index;

        if (m_is_initialized.Get(MemoryOrder::ACQUIRE) & ThreadName::THREAD_RENDER) {
            OnComponentIndexChanged(index, prev_index);
        }
    }

    HYP_FORCE_INLINE bool IsInitialized(ThreadName thread_name = ThreadName::THREAD_RENDER) const
        { return m_is_initialized.Get(MemoryOrder::ACQUIRE) & thread_name; }
    
    /*! \brief Init the component. Called on the RENDER thread when the RenderComponent is added to the RenderEnvironment */
    void ComponentInit()
    {
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        AssertThrow(!(m_is_initialized.Get(MemoryOrder::ACQUIRE) & ThreadName::THREAD_RENDER));

        Init();

        m_is_initialized.BitOr(ThreadName::THREAD_RENDER, MemoryOrder::RELEASE);
    }

    /*! \brief Update data for the component. Called from GAME thread. */
    void ComponentUpdate(GameCounter::TickUnit delta)
    {
        Threads::AssertOnThread(ThreadName::THREAD_GAME);

        if (!(m_is_initialized.Get(MemoryOrder::ACQUIRE) & ThreadName::THREAD_GAME)) {
            InitGame();

            m_is_initialized.BitOr(ThreadName::THREAD_GAME, MemoryOrder::RELEASE);
        }

        OnUpdate(delta);
    }

    /*! \brief Perform rendering. Called from RENDER thread. */
    void ComponentRender(Frame *frame)
    {
        Threads::AssertOnThread(ThreadName::THREAD_RENDER);

        AssertThrow(m_is_initialized.Get(MemoryOrder::ACQUIRE) & ThreadName::THREAD_RENDER);

        if (m_render_frame_slicing == 0 || m_render_frame_slicing_counter++ % m_render_frame_slicing == 0) {
            OnRender(frame);
        }
    }

    /*! \brief Called on the RENDER thread when the component is removed. */
    void ComponentRemoved() { OnRemoved(); }

protected:
    virtual void Init() = 0;
    virtual void InitGame() = 0;
    virtual void OnUpdate(GameCounter::TickUnit delta) = 0;
    virtual void OnRender(Frame *frame) = 0;
    virtual void OnRemoved() { }
    virtual void OnComponentIndexChanged(Index new_index, Index prev_index) = 0;

    Name                    m_name;
    const uint              m_render_frame_slicing; // amount of frames to wait between render calls
    uint                    m_render_frame_slicing_counter;
    Index                   m_index;
    RenderEnvironment       *m_parent;

private:
    HYP_FORCE_INLINE void SetParent(RenderEnvironment *parent)
        { m_parent = parent; }

    AtomicVar<ThreadMask>   m_is_initialized;
};

} // namespace hyperion

#endif