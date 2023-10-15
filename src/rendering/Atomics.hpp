#ifndef HYPERION_V2_ATOMICS_H
#define HYPERION_V2_ATOMICS_H

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Fence;
using renderer::CommandBuffer;

class Engine;

class AtomicCounter
{
public:
    using CountType = UInt32;

    AtomicCounter();
    AtomicCounter(const AtomicCounter &other) = delete;
    AtomicCounter &operator=(const AtomicCounter &other) = delete;
    ~AtomicCounter();

    const GPUBufferRef &GetBuffer() const
        { return m_buffer; }

    void Create();
    void Destroy();

    void Reset(CountType value = 0);
    CountType Read() const;

private:
    GPUBufferRef    m_buffer;
};

} // namespace hyperion::v2

#endif