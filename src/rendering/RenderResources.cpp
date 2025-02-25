#include <rendering/RenderResources.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(RenderResources, Rendering);

#pragma region RenderResourcesBase

RenderResourcesBase::RenderResourcesBase()
    : m_buffer_index(~0u),
      m_buffer_address(nullptr)
{
}

RenderResourcesBase::RenderResourcesBase(RenderResourcesBase &&other) noexcept
    : ResourceBase(static_cast<ResourceBase &&>(other)),
      m_buffer_index(other.m_buffer_index),
      m_buffer_address(other.m_buffer_address)
{
    other.m_buffer_index = ~0u;
    other.m_buffer_address = nullptr;
}

RenderResourcesBase::~RenderResourcesBase()
{
}

IThread *RenderResourcesBase::GetOwnerThread() const
{
    IThread *render_thread = Threads::GetThread(g_render_thread);
    AssertThrow(render_thread != nullptr);

    return render_thread;
}

void RenderResourcesBase::Initialize()
{
    AssertThrow(m_buffer_index == ~0u);
    AcquireBufferIndex();

    Initialize_Internal();
}

void RenderResourcesBase::Destroy()
{
    if (m_buffer_index != ~0u) {
        ReleaseBufferIndex();
    }

    Destroy_Internal();
}

void RenderResourcesBase::Update()
{
    Update_Internal();
}

bool RenderResourcesBase::CanExecuteInline() const
{
    return Threads::IsOnThread(g_render_thread);
}

void RenderResourcesBase::FlushScheduledTasks()
{
    HYPERION_ASSERT_RESULT(renderer::RenderCommands::Flush());
}

void RenderResourcesBase::EnqueueOp(Proc<void> &&proc)
{
    struct RENDER_COMMAND(RenderResourceOperation) : renderer::RenderCommand
    {
        Proc<void>  proc;

        RENDER_COMMAND(RenderResourceOperation)(Proc<void> &&proc)
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

void RenderResourcesBase::AcquireBufferIndex()
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

void RenderResourcesBase::ReleaseBufferIndex()
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

#pragma endregion RenderResourcesBase

} // namespace hyperion