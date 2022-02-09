#ifndef ENVMAP_PROBE_CONTROL_H
#define ENVMAP_PROBE_CONTROL_H

#include "../../../control.h"
#include "../../../math/bounding_box.h"
#include <memory>

namespace hyperion {
class EnvMapProbe;
class EnvMapProbeControl : public EntityControl {
public:
    EnvMapProbeControl(const Vector3 &origin, BoundingBox bounds = BoundingBox());
    virtual ~EnvMapProbeControl() = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt);

protected:
    virtual std::shared_ptr<EntityControl> CloneImpl() override;

    std::shared_ptr<Entity> m_env_probe_node;
    std::shared_ptr<EnvMapProbe> m_env_probe;
};
} // namespace hyperion

#endif