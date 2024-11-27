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
    AtomicVar<int16>    m_ref_count;
    AtomicVar<int16>    m_update_counter;
    bool                m_is_initialized;
};

struct RenderResourcesHandle
{
    RenderResourcesHandle()
        : render_resources(nullptr)
    {
    }

    RenderResourcesHandle(RenderResourcesBase &render_resources)
        : render_resources(&render_resources)
    {
        render_resources.Claim();
    }

    RenderResourcesHandle(const RenderResourcesHandle &other)
        : render_resources(other.render_resources)
    {
        if (render_resources != nullptr) {
            render_resources->Claim();
        }
    }

    RenderResourcesHandle &operator=(const RenderResourcesHandle &other)
    {
        if (this == &other || render_resources == other.render_resources) {
            return *this;
        }

        if (render_resources != nullptr) {
            render_resources->Unclaim();
        }

        render_resources = other.render_resources;

        if (render_resources != nullptr) {
            render_resources->Claim();
        }

        return *this;
    }

    RenderResourcesHandle(RenderResourcesHandle &&other) noexcept
        : render_resources(other.render_resources)
    {
        other.render_resources = nullptr;
    }

    RenderResourcesHandle &operator=(RenderResourcesHandle &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (render_resources != nullptr) {
            render_resources->Unclaim();
        }

        render_resources = other.render_resources;
        other.render_resources = nullptr;

        return *this;
    }

    ~RenderResourcesHandle()
    {
        if (render_resources != nullptr) {
            render_resources->Unclaim();
        }
    }

    void Reset()
    {
        if (render_resources != nullptr) {
            render_resources->Unclaim();
        }

        render_resources = nullptr;
    }

    RenderResourcesBase *render_resources;
};

} // namespace hyperion

#endif