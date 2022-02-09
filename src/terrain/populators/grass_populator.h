#ifndef GRASS_POPULATOR_H
#define GRASS_POPULATOR_H

#include "populator.h"

namespace hyperion {

class GrassPopulator : public Populator {
public:
    GrassPopulator(
        Camera *camera,
        unsigned long seed = 12345,
        double probability_factor = 0.55,
        float tolerance = 0.15f,
        float max_distance = 150.0f,
        float spread = 1.5f,
        int num_entities_per_chunk = 4,
        int num_patches = 4
    );
    ~GrassPopulator();

    std::shared_ptr<Entity> CreateEntity(const Vector3 &position) const override;

protected:
    virtual std::shared_ptr<EntityControl> CloneImpl() override;
};

} // namespace hyperion

#endif
