#ifndef GI_PROBE_CONTROL_H
#define GI_PROBE_CONTROL_H

#include "../../../control.h"
#include "../../../math/bounding_box.h"
#include <memory>

namespace hyperion {
class GIMapper;
class GIProbeControl : public EntityControl {
public:
    GIProbeControl(const Vector3 &origin, const BoundingBox &bounds);
    virtual ~GIProbeControl() = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt);

protected:
    virtual std::shared_ptr<EntityControl> CloneImpl() override;

    std::shared_ptr<Entity> m_gi_mapper_node;
    std::shared_ptr<GIMapper> m_gi_mapper;
    Vector3 m_origin;
    BoundingBox m_bounds;
};
} // namespace hyperion

#endif