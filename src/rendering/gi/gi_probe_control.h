#ifndef GI_PROBE_CONTROL_H
#define GI_PROBE_CONTROL_H

#include <memory>
#include "../../control.h"
#include "../../math/bounding_box.h"

namespace hyperion {
class GIMapper;
class GIProbeControl : public EntityControl {
public:
    GIProbeControl(const BoundingBox &bounds);
    virtual ~GIProbeControl() = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt);

protected:
    std::shared_ptr<Entity> m_gi_mapper_node;
    std::shared_ptr<GIMapper> m_gi_mapper;
};
} // namespace hyperion

#endif