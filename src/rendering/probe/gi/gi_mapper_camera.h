#ifndef GI_MAPPER_CAMERA_H
#define GI_MAPPER_CAMERA_H

#include "../probe_camera.h"
#include "../probe_region.h"
#include "../../../math/bounding_box.h"
#include "../../texture_3D.h"
#include "../../renderable.h"

#include <memory>

namespace hyperion {
class Shader;
class ComputeShader;

class GIMapperCamera : public ProbeCamera {
public:
    GIMapperCamera(const ProbeRegion &region);
    virtual ~GIMapperCamera();

    virtual const Texture *GetTexture() const override;

    void Begin();
    void End();

    virtual void Update(double dt) override;

    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

    std::shared_ptr<Texture3D> m_texture;
    std::shared_ptr<ComputeShader> m_clear_shader;
    std::shared_ptr<ComputeShader> m_mipmap_shader;
};
} // namespace hyperion

#endif