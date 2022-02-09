#ifndef ENVMAP_PROBE_CAMERA_H
#define ENVMAP_PROBE_CAMERA_H

#include "../probe_camera.h"

#include <memory>

namespace hyperion {
class EnvMapProbeCamera : public ProbeCamera {
    friend class EnvMapProbe;
public:
    EnvMapProbeCamera(const ProbeRegion &region, int width, int height, float near, float far);
    EnvMapProbeCamera(const EnvMapProbeCamera &other) = delete;
    EnvMapProbeCamera &operator=(const EnvMapProbeCamera &other) = delete;
    ~EnvMapProbeCamera();

    virtual const Texture *GetTexture() const override;

    virtual void Update(double dt) override;
    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;
    inline void SetTexture(const Texture *texture) { m_texture = texture; }

    const Texture *m_texture;
};
} // namespace hyperion

#endif
