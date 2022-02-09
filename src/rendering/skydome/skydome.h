#ifndef SKYDOME_H
#define SKYDOME_H

#include "../../entity.h"
#include "../../control.h"
#include "../mesh.h"
#include "../texture.h"
#include "../camera/camera.h"
#include "../../math/vector4.h"
#include "skydome_shader.h"
#include "clouds_shader.h"

#include <memory>

namespace hyperion {
class SkydomeControl : public EntityControl {
public:
    SkydomeControl(Camera *camera);
    virtual ~SkydomeControl() = default;

    void OnAdded();
    void OnRemoved();
    void OnUpdate(double dt);

private:
    virtual std::shared_ptr<EntityControl> CloneImpl() override;

    static const bool clouds_in_dome;

    std::shared_ptr<Entity> dome;
    std::shared_ptr<SkydomeShader> shader;
    std::shared_ptr<Mesh> clouds_quad;
    std::shared_ptr<CloudsShader> clouds_shader;
    Camera *camera;
    double global_time;
    Vector4 sky_color;
};
} // namespace hyperion

#endif
