#ifndef CONTROL_H
#define CONTROL_H

#include "asset/fbom/fbom.h"

namespace hyperion {

class Entity;

class EntityControl : public fbom::FBOMLoadable {
    friend class Entity;
public:
    EntityControl(const fbom::FBOMType &loadable_type, const double tps = 30.0);
    EntityControl(const EntityControl &other) = delete;
    EntityControl &operator=(const EntityControl &other) = delete;
    virtual ~EntityControl();

    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;
    virtual void OnFirstRun(double dt);
    virtual void OnUpdate(double dt) = 0;

    virtual std::shared_ptr<Loadable> Clone() override;

protected:
    Entity *parent;

    virtual std::shared_ptr<EntityControl> CloneImpl() = 0;

private:
    const double tps;
    double tick;
    bool m_first_run;
};

} // namespace hyperion

#endif
