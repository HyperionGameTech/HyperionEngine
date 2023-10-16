#ifndef HYPERION_V2_RENDER_COMPONENT_H
#define HYPERION_V2_RENDER_COMPONENT_H

#include <core/Base.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include <scene/Entity.hpp>
#include <math/MathUtil.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <GameCounter.hpp>
#include <Constants.hpp>
#include <Threads.hpp>
#include <Types.hpp>

#include <atomic>
#include <type_traits>

namespace hyperion::v2 {

class RenderEnvironment;

using renderer::Frame;

enum RenderComponentName : UInt
{
    RENDER_COMPONENT_INVALID = UInt(-1),

    RENDER_COMPONENT_VCT = 0,
    RENDER_COMPONENT_SHADOWS,
    RENDER_COMPONENT_CUBEMAP,
    RENDER_COMPONENT_SVO,
    RENDER_COMPONENT_UI,
    RENDER_COMPONENT_ENV_GRID,
    RENDER_COMPONENT_POINT_SHADOW,

    RENDER_COMPONENT_SLOT6,
    RENDER_COMPONENT_SLOT7,
    RENDER_COMPONENT_SLOT8,
    RENDER_COMPONENT_SLOT9,
    RENDER_COMPONENT_SLOT10,

    RENDER_COMPONENT_MAX
};

class RenderComponentBase
{
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

    RenderComponentName GetName() const { return m_name; }

    RenderEnvironment *GetParent() const { return m_parent; }
    void SetParent(RenderEnvironment *parent) { m_parent = parent; }

    bool IsValidComponent() const { return m_index != ~0u; }

    Index GetComponentIndex() const { return m_index; }
    virtual void SetComponentIndex(Index index) { m_index = index; }

    /*! \brief Init the component. Runs in RENDER/MAIN thread. */
    virtual void ComponentInit() = 0;
    /*! \brief Update data for the component. Runs on GAME thread. */
    virtual void ComponentUpdate(GameCounter::TickUnit delta) { }
    /*! \brief Perform rendering. Runs in RENDER thread. */
    virtual void ComponentRender(Frame *frame) = 0;
    /*! \brief Called when the component is removed. */
    virtual void ComponentRemoved() = 0;

protected:
    RenderComponentName m_name;
    const UInt m_render_frame_slicing; // amount of frames to wait between render calls
    UInt m_render_frame_slicing_counter; // amount of frames to wait between render calls
    Index m_index;
    RenderEnvironment *m_parent;
};

template <class Derived>
class RenderComponent : public RenderComponentBase
{
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
    
    virtual void ComponentInit() override final
    {
        // Threads::AssertOnThread(THREAD_RENDER);
        Threads::AssertOnThread(THREAD_GAME);

        static_cast<Derived *>(this)->Init();

        m_component_is_render_init = true;
    }

    virtual void ComponentUpdate(GameCounter::TickUnit delta) override final
    {
        Threads::AssertOnThread(THREAD_GAME);

        if (!m_component_is_game_init) {
            static_cast<Derived *>(this)->InitGame();

            m_component_is_game_init = true;
        }

        static_cast<Derived *>(this)->OnUpdate(delta);
    }

    virtual void ComponentRender(Frame *frame) override final
    {
        Threads::AssertOnThread(THREAD_RENDER);

        if (m_render_frame_slicing_counter >= m_render_frame_slicing) {
            static_cast<Derived *>(this)->OnRender(frame);

            m_render_frame_slicing_counter = 0;
        } else {
            ++m_render_frame_slicing_counter;
        }
    }

    virtual void ComponentRemoved() override { }

protected:
    virtual void OnComponentIndexChanged(Index new_index, Index prev_index) = 0;

private:
    bool m_component_is_render_init,
         m_component_is_game_init;
};

} // namespace hyperion::v2

#endif