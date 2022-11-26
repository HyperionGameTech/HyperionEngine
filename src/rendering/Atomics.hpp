#ifndef HYPERION_V2_ATOMICS_H
#define HYPERION_V2_ATOMICS_H

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion::v2 {

using renderer::Fence;
using renderer::CommandBuffer;
using renderer::GPUBuffer;
using renderer::AtomicCounterBuffer;
using renderer::StagingBuffer;

class Engine;

class AtomicCounter
{
public:
    using CountType = UInt32;

    AtomicCounter();
    AtomicCounter(const AtomicCounter &other) = delete;
    AtomicCounter &operator=(const AtomicCounter &other) = delete;
    ~AtomicCounter();

    AtomicCounterBuffer *GetBuffer() const { return m_buffer.get(); }

    void Create();
    void Destroy();

    void Reset(CountType value = 0);
    CountType Read() const;

private:
    std::unique_ptr<AtomicCounterBuffer> m_buffer;
    std::unique_ptr<StagingBuffer> m_staging_buffer;
};

} // namespace hyperion::v2

#endif