#ifndef PROBE_CAMERA_H
#define PROBE_CAMERA_H

#include "probe_region.h"
#include "../renderable.h"

#include <memory>

namespace hyperion {
class ProbeCamera : public Renderable {
public:
    ProbeCamera(const fbom::FBOMType &loadable_type, const ProbeRegion &region);
    ProbeCamera(const ProbeCamera &other) = delete;
    ProbeCamera &operator=(const ProbeCamera &other) = delete;
    ~ProbeCamera();

    inline ProbeRegion &GetRegion() { return m_region; }
    inline const ProbeRegion &GetRegion() const { return m_region; }
    inline Camera *GetCamera() { return m_camera; }
    inline const Camera *GetCamera() const { return m_camera; }

    virtual const Texture *GetTexture() const = 0;

    virtual void Update(double dt) = 0;
    virtual void Render(Renderer *renderer, Camera *cam) = 0;

protected:
    virtual std::shared_ptr<Renderable> CloneImpl() = 0;

    ProbeRegion m_region;
    Camera *m_camera;
};
} // namespace hyperion

#endif
