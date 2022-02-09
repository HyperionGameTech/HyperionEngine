#ifndef TREE_POPULATOR_H
#define TREE_POPULATOR_H

#include "populator.h"

namespace hyperion {

class TreePopulator : public Populator {
public:
    TreePopulator(
        Camera *camera,
        unsigned long seed = 555,
        double probability_factor = 0.4,
        float tolerance = 0.15f,
        float max_distance = 200.0f,
        float spread = 4.5f,
        int num_entities_per_chunk = 3,
        int num_patches = 3
    );
    ~TreePopulator();

    std::shared_ptr<Entity> CreateEntity(const Vector3 &position) const override;

protected:
    virtual std::shared_ptr<EntityControl> CloneImpl() override;
};

} // namespace hyperion

#endif
