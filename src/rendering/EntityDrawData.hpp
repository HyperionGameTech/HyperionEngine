#ifndef HYPERION_V2_ENTITY_DRAW_DATA_HPP
#define HYPERION_V2_ENTITY_DRAW_DATA_HPP

#include <core/ID.hpp>
#include <math/Transform.hpp>

namespace hyperion::v2 {

class Entity;
class Mesh;
class Material;
class Skeleton;

struct EntityDrawData
{
    ID<Entity>      entity_id;
    ID<Mesh>        mesh_id;
    ID<Material>    material_id;
    ID<Skeleton>    skeleton_id;
    Transform       transform;
};

} // namespace hyperion::v2

#endif