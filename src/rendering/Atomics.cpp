#include "Atomics.hpp"
#include "../Engine.hpp"
#include <Types.hpp>

namespace hyperion::v2 {

using Context = renderer::StagingBufferPool::Context;

AtomicCounter::AtomicCounter() = default;

AtomicCounter::~AtomicCounter()
{
    AssertThrowMsg(m_buffer == nullptr, "buffer should have been destroyed before destructor call");
}

void AtomicCounter::Create()
{
    AssertThrow(m_buffer == nullptr);

    m_buffer = std::make_unique<AtomicCounterBuffer>();
    HYPERION_ASSERT_RESULT(m_buffer->Create(g_engine->GetGPUInstance()->GetDevice(), sizeof(UInt32)));
}

void AtomicCounter::Destroy()
{
    AssertThrow(m_buffer != nullptr);

    HYPERION_ASSERT_RESULT(m_buffer->Destroy(g_engine->GetGPUInstance()->GetDevice()));
    m_buffer.reset();
}

void AtomicCounter::Reset(CountType value)
{
    HYPERION_ASSERT_RESULT(g_engine->GetGPUInstance()->GetStagingBufferPool().Use(
        g_engine->GetGPUInstance()->GetDevice(),
        [this, value](Context &context) {
            auto *staging_buffer = context.Acquire(sizeof(value));
            
            staging_buffer->Copy(g_engine->GetGPUInstance()->GetDevice(), sizeof(value), (void *)&value);
   
            auto commands = g_engine->GetGPUInstance()->GetSingleTimeCommands();

            commands.Push([&](const CommandBufferRef &command_buffer) {
                m_buffer->CopyFrom(command_buffer, staging_buffer, sizeof(value));

                HYPERION_RETURN_OK;
            });

            return commands.Execute(g_engine->GetGPUInstance()->GetDevice());
        }
    ));
}

auto AtomicCounter::Read() const -> CountType
{
    auto result = MathUtil::MaxSafeValue<CountType>();

    HYPERION_ASSERT_RESULT(g_engine->GetGPUInstance()->GetStagingBufferPool().Use(
        g_engine->GetGPUInstance()->GetDevice(),
        [this, &result](Context &context) {
            auto *staging_buffer = context.Acquire(sizeof(result));
            
            auto commands = g_engine->GetGPUInstance()->GetSingleTimeCommands();

            commands.Push([&](const CommandBufferRef &command_buffer) {
                staging_buffer->CopyFrom(command_buffer, m_buffer.get(), sizeof(result));

                HYPERION_RETURN_OK;
            });

            HYPERION_BUBBLE_ERRORS(commands.Execute(g_engine->GetGPUInstance()->GetDevice()));
            
            staging_buffer->Read(g_engine->GetGPUInstance()->GetDevice(), sizeof(result), (void *)&result);

            HYPERION_RETURN_OK;
        }
    ));

    return result;
}

} // namespace hyperion::v2