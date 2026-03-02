/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    virtual ~SingleTimeCommands() = default;

    void Push(Proc<void(RenderQueue&)>&& fn)
    {
        m_functions.PushBack(std::move(fn));
    }

    virtual RendererResult Execute() = 0;

protected:
    Array<Proc<void(RenderQueue&)>, RenderAllocator> m_functions;
};

} // namespace Hyperion
