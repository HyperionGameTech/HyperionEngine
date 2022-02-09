#ifndef ENVMAP_PROBE_H
#define ENVMAP_PROBE_H

#include "../probe.h"
#include "../../framebuffer_cube.h"
#include "../../../math/matrix4.h"
#include "../../../math/vector3.h"

#include <memory>
#include <array>

namespace hyperion {

class Shader;
class ComputeShader;
class Cubemap;

class EnvMapProbe : public Probe {
public:
    EnvMapProbe(const Vector3 &origin, const BoundingBox &bounds, int width, int height, float near, float far);
    EnvMapProbe(const EnvMapProbe &other) = delete;
    EnvMapProbe &operator=(const EnvMapProbe &other) = delete;
    virtual ~EnvMapProbe();

    inline std::shared_ptr<Cubemap> GetColorTexture() const
    {
        return std::static_pointer_cast<Cubemap>(m_fbo->GetAttachment(Framebuffer::FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_COLOR));
    }

    inline std::shared_ptr<Cubemap> GetDepthTexture() const
    {
        return std::static_pointer_cast<Cubemap>(m_fbo->GetAttachment(Framebuffer::FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_DEPTH));
    }

    inline int GetWidth() const { return m_width; }
    inline int GetHeight() const { return m_height; }
    inline float GetNear() const { return m_near; }
    inline float GetFar() const { return m_far; }

    virtual void Bind(Shader *shader) override;
    virtual void Update(double dt) override;
    virtual void Render(Renderer *renderer, Camera *cam) override;

    std::shared_ptr<Texture2D> m_sh_texture; // tmp

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

    void RenderCubemap(Renderer *, Camera *);
    void RenderSphericalHarmonics();

    int m_width;
    int m_height;
    float m_near;
    float m_far;
    std::shared_ptr<ComputeShader> m_spherical_harmonics_shader;
    Framebuffer *m_fbo;
    double m_render_tick;
    int m_render_index;
    bool m_is_first_run;
};
} // namespace hyperion

#endif
