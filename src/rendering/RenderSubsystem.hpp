/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_SUBSYSTEM_HPP
#define HYPERION_RENDER_SUBSYSTEM_HPP

#include <core/Base.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/MathUtil.hpp>

#include <scene/Entity.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <GameCounter.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class RenderEnvironment;

HYP_CLASS(Abstract)
class HYP_API RenderSubsystem : public EnableRefCountedPtrFromThis<RenderSubsystem>
{
    HYP_OBJECT_BODY(RenderSubsystem);

public:
    friend class RenderEnvironment;

    /*! \param render_frame_slicing Number of frames to wait between render calls */
    RenderSubsystem(Name name, uint32 render_frame_slicing = 0)
        : m_name(name),
          m_render_frame_slicing(MathUtil::NextMultiple(render_frame_slicing, max_frames_in_flight)),
          m_render_frame_slicing_counter(0),
          m_parent(nullptr),
          m_is_initialized(0)
    {
    }

    RenderSubsystem(const RenderSubsystem& other) = delete;
    RenderSubsystem& operator=(const RenderSubsystem& other) = delete;
    virtual ~RenderSubsystem() = default;

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE bool IsInitialized(const StaticThreadID& thread_id = g_render_thread) const
    {
        return m_is_initialized.Get(MemoryOrder::ACQUIRE) & thread_id;
    }

    /*! \brief Init the component. Called on the RENDER thread when the RenderSubsystem is added to the RenderEnvironment */
    void ComponentInit();
    /*! \brief Update data for the component. Called from GAME thread. */
    void ComponentUpdate(GameCounter::TickUnit delta);
    /*! \brief Perform rendering. Called from RENDER thread. */
    void ComponentRender(FrameBase* frame);

    /*! \brief Called on the RENDER thread when the component is removed. */
    void ComponentRemoved()
    {
        OnRemoved();
    }

    /*! \brief Thread-safe way to remove the RenderSubsystem from the RenderEnvironment, if applicable.
     *  A render command will be issued to remove the RenderSubsystem from the RenderEnvironment. */
    void RemoveFromEnvironment();

protected:
    virtual void Init() { };
    virtual void InitGame() { };
    virtual void OnUpdate(GameCounter::TickUnit delta) { };
    virtual void OnRender(FrameBase* frame) = 0;

    virtual void OnRemoved()
    {
    }

    RenderEnvironment* GetParent() const;

    Name m_name;
    const uint32 m_render_frame_slicing; // amount of frames to wait between render calls
    uint32 m_render_frame_slicing_counter;

private:
    void SetParent(RenderEnvironment* parent);

    RenderEnvironment* m_parent;
    AtomicVar<ThreadMask> m_is_initialized;
};

} // namespace hyperion

#endif