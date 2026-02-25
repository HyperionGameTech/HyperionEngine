/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/functional/Proc.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

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

} // namespace Hyperion
