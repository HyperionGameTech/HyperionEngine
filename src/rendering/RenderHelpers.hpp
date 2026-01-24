/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/functional/Proc.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>

#include <core/Types.hpp>

namespace Hyperion {

uint32 RetrieveResourceBinding(const ObjectBase* resource);

template <class AllocatorType>
class TRenderQueue;

using RenderQueue = TRenderQueue<RenderAllocator>;

namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod);

} // namespace helpers

class SingleTimeCommands
{
public:
    HYP_API virtual ~SingleTimeCommands() = default;

    void Push(Proc<void(RenderQueue& renderQueue)>&& fn)
    {
        m_functions.PushBack(std::move(fn));
    }

    virtual RendererResult Execute() = 0;

protected:
    Array<Proc<void(RenderQueue& renderQueue)>> m_functions;
};

template <class T>
struct ShaderDataOffset
{
    static_assert(IsPodTypeV<T>, "T must be POD to use with ShaderDataOffset");

    static constexpr uint32 InvalidIndex = ~0u;

    explicit ShaderDataOffset(uint32 index)
        : index(index)
    {
    }

    explicit ShaderDataOffset(const ObjectBase* resource, uint32 indexIfNull = InvalidIndex)
        : index(indexIfNull)
    {
        if (uint32 idx = RetrieveResourceBinding(resource); idx != ~0u)
        {
            index = idx;
        }
    }

    HYP_FORCE_INLINE operator uint32() const
    {
        AssertDebug(index != InvalidIndex);

        return uint32(sizeof(T) * index);
    }

    uint32 index;
};

} // namespace Hyperion
