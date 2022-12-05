#ifndef HYPERION_V2_DRAW_PROXY_H
#define HYPERION_V2_DRAW_PROXY_H

#include <core/Base.hpp>
#include <rendering/RenderBucket.hpp>

#include <math/BoundingBox.hpp>
#include <math/Vector4.hpp>
#include <math/Color.hpp>
#include <math/Frustum.hpp>

#include <core/ID.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <memory>
#include <atomic>

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
using renderer::ShaderValue;

class Mesh;
class Material;
class Engine;
class Entity;
class Camera;
class Skeleton;
class Scene;
class EnvProbe;
class Light;

void WaitForRenderUpdatesToComplete();

enum class LightType : UInt32
{
    DIRECTIONAL,
    POINT,
    SPOT
};

template <class T>
struct DrawProxy
{
private:
    DrawProxy(); // break intentionally; needs specialization
};

template <>
struct DrawProxy<STUB_CLASS(Material)>
{
    // TODO
};

using MaterialDrawProxy = DrawProxy<STUB_CLASS(Material)>;

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

    ID<Entity> entity_id;
    ID<Scene> scene_id;
    ID<Mesh> mesh_id;
    ID<Material> material_id;
    ID<Skeleton> skeleton_id;

    BoundingBox bounding_box;

    // object instance in GPU
    UInt32 draw_command_index = 0;

    Bucket bucket = Bucket::BUCKET_OPAQUE;
};

using EntityDrawProxy = DrawProxy<STUB_CLASS(Entity)>;

template <>
struct DrawProxy<STUB_CLASS(Camera)>
{
    Matrix4 view;
    Matrix4 projection;
    Matrix4 previous_view;
    Vector3 position;
    Vector3 direction;
    Vector3 up;
    Extent2D dimensions;
    Float clip_near;
    Float clip_far;
    Float fov;
    Frustum frustum;
};

using CameraDrawProxy = DrawProxy<STUB_CLASS(Camera)>;

template <>
struct DrawProxy<STUB_CLASS(Scene)>
{
    CameraDrawProxy camera;
    UInt32 frame_counter;
};

using SceneDrawProxy = DrawProxy<STUB_CLASS(Scene)>;

enum EnvProbeFlags : UInt32
{
    ENV_PROBE_NONE = 0x0,
    ENV_PROBE_PARALLAX_CORRECTED = 0x1
};

template <>
struct DrawProxy<STUB_CLASS(EnvProbe)>
{
    IDBase id;
    BoundingBox aabb;
    Vector3 world_position;
    UInt32 texture_index;
    EnvProbeFlags flags;
};

using EnvProbeDrawProxy = DrawProxy<STUB_CLASS(EnvProbe)>;

template <>
struct DrawProxy<STUB_CLASS(Light)>
{
    IDBase id;
    LightType type;
    Color color;
    Float radius;
    UInt32 shadow_map_index;
    Vector4 position_intensity;
};

using LightDrawProxy = DrawProxy<STUB_CLASS(Light)>;



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
    bool HasPendingRenderUpdates() const
        { return m_has_render_updates.load(std::memory_order_acquire) != 0; }

    // Only touch from render thread.
    // Update this when updates are enqueued, so just update
    // the shader data state to dirty to refresh this.
    DrawProxy<T> m_draw_proxy;

private:
    std::atomic_uint m_has_render_updates { 0u };
};

} // namespace hyperion::v2

#endif