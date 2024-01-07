#ifndef HYPERION_V2_ENTITY_DRAW_DATA_HPP
#define HYPERION_V2_ENTITY_DRAW_DATA_HPP

#include <core/ID.hpp>
#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>
#include <math/Matrix4.hpp>

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
    Matrix4         model_matrix;
    Matrix4         previous_model_matrix;
    BoundingBox     aabb;
    UInt32          bucket;
};

} // namespace hyperion::v2

#endif