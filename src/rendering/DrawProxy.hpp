#ifndef HYPERION_V2_DRAW_PROXY_H
#define HYPERION_V2_DRAW_PROXY_H

#include "Base.hpp"
#include "RenderBucket.hpp"
#include "Buffers.hpp"

#include <math/BoundingBox.hpp>
#include <math/Vector4.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <memory>

namespace hyperion::v2 {
    
using renderer::IndirectDrawCommand;
using renderer::IndirectBuffer;
using renderer::StorageBuffer;
using renderer::StagingBuffer;
using renderer::Frame;
using renderer::Result;
using renderer::Extent2D;
using renderer::ShaderVec2;
using renderer::ShaderVec4;

class Mesh;
class Material;
class Engine;
class Entity;
class Camera;
class EnvProbe;

struct alignas(16) ObjectInstance
{
    UInt32             entity_id;
    UInt32             draw_command_index;
    UInt32             batch_index;
    UInt32             num_indices;
    ShaderVec4<Float>  aabb_max;
    ShaderVec4<Float>  aabb_min;

    UInt32             packed_data;
};

template <class T>
struct DrawProxy {};

template <>
struct DrawProxy<STUB_CLASS(Entity)>
{
    // rendering objects (sent from game thread to
    // rendering thread when updates are enqueued)
    // engine will hold onto RenderResources like Mesh and Material
    // for at least {max_frames_in_flight} frames, so we will have
    // rendered the DrawProxy before the ptr is invalidated.
    Mesh *mesh = nullptr;
    Material *material = nullptr;

    IDBase entity_id,
           scene_id,
           mesh_id,
           material_id,
           skeleton_id;

    BoundingBox bounding_box;

    // object instance in GPU
    UInt32 draw_command_index = 0;

    Bucket bucket = Bucket::BUCKET_OPAQUE;
};

using EntityDrawProxy = DrawProxy<STUB_CLASS(Entity)>;

template <>
struct DrawProxy<STUB_CLASS(Camera)>
{
    Matrix4  view;
    Matrix4  projection;
    Vector3  position;
    Vector3  direction;
    Extent2D dimensions;
    Float    clip_near;
    Float    clip_far;
    Float    fov;
};

using CameraDrawProxy = DrawProxy<STUB_CLASS(Camera)>;

template <>
struct DrawProxy<STUB_CLASS(EnvProbe)>
{
    IDBase id;
    BoundingBox aabb;
    Vector3 world_position;
    UInt texture_index;
    EnvProbeFlags flags;
};

using EnvProbeDrawProxy = DrawProxy<STUB_CLASS(EnvProbe)>;

template <class T>
class HasDrawProxy
{
public:
    HasDrawProxy()
        : m_draw_proxy{}
    {
    }

    HasDrawProxy(const DrawProxy<T> &draw_proxy)
        : m_draw_proxy(draw_proxy)
    {
    }

    HasDrawProxy(DrawProxy<T> &&draw_proxy)
        : m_draw_proxy(std::move(draw_proxy))
    {
    }

    template <class ...Args>
    HasDrawProxy(Args &&... args)
        : m_draw_proxy(std::forward<Args>(args)...)
    {
    }

    HasDrawProxy(const HasDrawProxy &other)
        : m_draw_proxy(other.m_draw_proxy)
    {
    }

    HasDrawProxy &operator=(const HasDrawProxy &other)
    {
        m_draw_proxy = other.m_draw_proxy;
        return *this;
    }

    HasDrawProxy(HasDrawProxy &&other) noexcept
        : m_draw_proxy(std::move(other.m_draw_proxy))
    {
    }

    HasDrawProxy &operator=(HasDrawProxy &&other) noexcept
    {
        m_draw_proxy = std::move(other.m_draw_proxy);
        return *this;
    }

    ~HasDrawProxy() = default;

    /*! \brief Get the DrawProxy for this object. Only call from render thread. */
    const DrawProxy<T> &GetDrawProxy() const { return m_draw_proxy; }

protected:
    /*! Enqueue an update /for/ the render thread to update the draw proxy.
        Cannot set it directly in a thread-safe manner if not on the render thread,
        so use this.
    */
    template <class EngineImpl>
    void EnqueueDrawProxyUpdate(EngineImpl *engine, DrawProxy<T> &&draw_proxy)
    {
        engine->render_scheduler.Enqueue([this, _drawable = std::move(draw_proxy)](...) mutable {
            // just update draw_proxy object on render thread
            HasDrawProxy::m_draw_proxy = _drawable;

            HYPERION_RETURN_OK;
        });
    }

    // Only touch from render thread.
    // Update this when updates are enqueued, so just update
    // the shader data state to dirty to refresh this.
    DrawProxy<T> m_draw_proxy;
};

} // namespace hyperion::v2

#endif