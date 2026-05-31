/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Rendering/RenderMemory.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class ScratchImageAllocator final
{
public:
    ScratchImageAllocator();
    ~ScratchImageAllocator();

    ScratchImageAllocator(const ScratchImageAllocator&) = delete;
    ScratchImageAllocator& operator=(const ScratchImageAllocator&) = delete;

    void OnFrameStart();
    void OnFrameEnd();

    Handle<Texture> AcquireScratchImage(TextureType type, TextureFormat format, Vec3u extent);

    void Shutdown();

private:
    Pimpl<struct ScratchImageAllocatorImpl> m_impl;
};

} // namespace Hyperion
