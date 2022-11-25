#ifndef HYPERION_V2_ENV_PROBE_HPP
#define HYPERION_V2_ENV_PROBE_HPP

#include <core/Base.hpp>
#include <core/lib/Optional.hpp>
#include <rendering/Texture.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <math/BoundingBox.hpp>

namespace hyperion::v2 {

struct RenderCommand_UpdateEnvProbeRenderData;

class EnvProbe
    : public EngineComponentBase<STUB_CLASS(EnvProbe)>,
      public HasDrawProxy<STUB_CLASS(EnvProbe)>,
      public RenderResource
{
    friend struct RenderCommand_UpdateEnvProbeRenderData;

public:
    EnvProbe(Handle<Texture> &&texture);
    EnvProbe(Handle<Texture> &&texture, const BoundingBox &aabb);
    EnvProbe(const EnvProbe &other) = delete;
    EnvProbe &operator=(const EnvProbe &other) = delete;
    ~EnvProbe();

    bool IsParallaxCorrected() const
        { return m_aabb.Any(); }

    void SetIsParallaxCorrected(bool is_parallax_corrected)
    {
        if (is_parallax_corrected) {
            if (!m_aabb.Any()) {
                m_aabb = BoundingBox::empty;
                SetNeedsUpdate();
            }
        } else {
            m_aabb.Unset();
            SetNeedsUpdate();
        }
    }

    const BoundingBox &GetAABB() const
        { return m_aabb.Any() ? m_aabb.Get() : BoundingBox::empty; }

    void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; SetNeedsUpdate(); }

    const Vector3 &GetWorldPosition() const
        { return m_world_position; }

    void SetWorldPosition(const Vector3 &world_position)
        { m_world_position = world_position; SetNeedsUpdate(); }

    void Init(Engine *engine);
    void EnqueueBind() const;
    void EnqueueUnbind() const;
    void Update();

private:
    void SetNeedsUpdate() { m_needs_update = true; }

    Handle<Texture> m_texture;
    Optional<BoundingBox> m_aabb;
    Vector3 m_world_position;
    bool m_needs_update;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
