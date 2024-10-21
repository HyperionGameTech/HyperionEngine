/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_RESOURCES_HPP
#define HYPERION_RENDER_RESOURCES_HPP

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <Types.hpp>

namespace hyperion {

// Represents the objects an engine object (e.g Material) uses while it is currently being rendered.
// The resources are reference counted internally, so as long as the object is being used for rendering somewhere,
// the resources will remain in memory.

// Claim/Unclaim are only meant to be used from the render thread, as the reference count is not atomic.

class RenderResourcesBase : public EnableRefCountedPtrFromThis<RenderResourcesBase>
{
public:
    RenderResourcesBase();

    RenderResourcesBase(const RenderResourcesBase &other)                   = delete;
    RenderResourcesBase &operator=(const RenderResourcesBase &other)        = delete;

    RenderResourcesBase(RenderResourcesBase &&other) noexcept;
    RenderResourcesBase &operator=(RenderResourcesBase &&other) noexcept    = delete;

    virtual ~RenderResourcesBase()                                          = default;

    void Claim();
    void Unclaim();

protected:
    virtual void Initialize() = 0;
    virtual void Destroy() = 0;
    virtual void Update() = 0;

    void SetNeedsUpdate();

private:
    uint16              m_ref_count;
    AtomicVar<uint16>   m_update_counter;
};

} // namespace hyperion

#endif