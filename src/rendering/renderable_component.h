#ifndef HYPERION_V2_RENDERABLE_COMPONENT_H
#define HYPERION_V2_RENDERABLE_COMPONENT_H

#include "buffers.h"
#include "shader_globals.h"
#include <core/lib/atomic_semaphore.h>
#include <threads.h>
#include <types.h>

#include <atomic>
#include <type_traits>

namespace hyperion::v2 {

template <class Derived, class RenderDataStruct>
class RenderableComponent {
    struct SetBufferDataFunctor {
        using Function = std::add_pointer_t<void(Derived *self, ShaderGlobals *shader_globals, const RenderDataStruct &data)>;

        Derived         *self{nullptr};
        Function         fn{nullptr};
        
        void operator()(ShaderGlobals *shader_globals, const RenderDataStruct &data) const
        {
            fn(self, shader_globals, data);
        }
    };

public:
    RenderableComponent();
    ~RenderableComponent() = default;

    void RenderUpdate(ShaderGlobals *shader_globals);
    bool IsRenderDataChanged() const { return m_state.changed; }

protected:
    void SetRenderData(RenderDataStruct &&data);

    void OnRenderUpdate(
        typename SetBufferDataFunctor::Function function
    );

private:
    struct {
        RenderDataStruct data{};
        std::atomic_bool changed{false};
    } m_state;
    
    AtomicSemaphore<int> m_semaphore; // so we do not update while changing

    SetBufferDataFunctor m_functor;
};

template <class Derived, class RenderDataStruct>
RenderableComponent<Derived, RenderDataStruct>::RenderableComponent()
    : m_state{}
{
}

template <class Derived, class RenderDataStruct>
void RenderableComponent<Derived, RenderDataStruct>::OnRenderUpdate(
    typename SetBufferDataFunctor::Function function
)
{
    m_functor = SetBufferDataFunctor{static_cast<Derived *>(this), function};
}

template <class Derived, class RenderDataStruct>
void RenderableComponent<Derived, RenderDataStruct>::SetRenderData(RenderDataStruct &&data)
{
    //Threads::AssertOnThread(~THREAD_RENDER); // cannot be on render thread or we will get deadlocks!

    //if (!Threads::IsOnThread(THREAD_RENDER)) {
    //    m_semaphore.WaitUntilValue(0);
    //}

    m_state.data    = std::forward<RenderDataStruct>(data);
    m_state.changed = true;
}

template <class Derived, class RenderDataStruct>
void RenderableComponent<Derived, RenderDataStruct>::RenderUpdate(ShaderGlobals *shader_globals)
{
    Threads::AssertOnThread(THREAD_RENDER);

    //m_semaphore.Inc();

    m_functor(shader_globals, m_state.data);
    m_state.changed = false;

    //m_semaphore.Dec();
}

} // namespace hyperion::v2

#endif