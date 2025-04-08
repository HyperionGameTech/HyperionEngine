#include <rendering/RenderResource.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Mutex.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Resource);

#pragma region RenderResourceBase

RenderResourceBase::RenderResourceBase()
    : m_buffer_index(~0u),
      m_buffer_address(nullptr)
{
}

RenderResourceBase::RenderResourceBase(RenderResourceBase &&other) noexcept
    : ResourceBase(static_cast<ResourceBase &&>(other)),
      m_buffer_index(other.m_buffer_index),
      m_buffer_address(other.m_buffer_address)
{
    other.m_buffer_index = ~0u;
    other.m_buffer_address = nullptr;
}

RenderResourceBase::~RenderResourceBase()
{
}

void RenderResourceBase::Initialize()
{
    AssertThrow(m_buffer_index == ~0u);
    AcquireBufferIndex();

    Initialize_Internal();
}

void RenderResourceBase::Destroy()
{
    if (m_buffer_index != ~0u) {
        ReleaseBufferIndex();
    }

    Destroy_Internal();
}

void RenderResourceBase::Update()
{
    Update_Internal();
}

IThread *RenderResourceBase::GetOwnerThread() const
{
    static IThread *const render_thread = Threads::GetThread(g_render_thread);

    return render_thread;
}

bool RenderResourceBase::CanExecuteInline() const
{
    return Threads::IsOnThread(g_render_thread);
}

void RenderResourceBase::FlushScheduledTasks() const
{
    HYPERION_ASSERT_RESULT(renderer::RenderCommands::Flush());
}

void RenderResourceBase::EnqueueOp(Proc<void()> &&proc)
{
    struct RENDER_COMMAND(RenderResourceOperation) : renderer::RenderCommand
    {
        Proc<void()>  proc;

        RENDER_COMMAND(RenderResourceOperation)(Proc<void()> &&proc)
            : proc(std::move(proc))
        {
        }

        virtual ~RENDER_COMMAND(RenderResourceOperation)() override = default;

        virtual RendererResult operator()() override
        {
            proc();

            return { };
        }
    };

    PUSH_RENDER_COMMAND(RenderResourceOperation, std::move(proc));
}

void RenderResourceBase::AcquireBufferIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_buffer_index == ~0u);

    GPUBufferHolderBase *holder = GetGPUBufferHolder();

    if (holder == nullptr) {
        return;
    }

    m_buffer_index = holder->AcquireIndex(&m_buffer_address);
}

void RenderResourceBase::ReleaseBufferIndex()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_buffer_index != ~0u);

    GPUBufferHolderBase *holder = GetGPUBufferHolder();
    AssertThrow(holder != nullptr);

    holder->ReleaseIndex(m_buffer_index);

    m_buffer_index = ~0u;
    m_buffer_address = nullptr;
}

#pragma endregion RenderResourceBase

} // namespace hyperion