#ifndef HYPERION_V2_ECS_SCENE_COMPONENT_HPP
#define HYPERION_V2_ECS_SCENE_COMPONENT_HPP

#include <core/Handle.hpp>

namespace hyperion::v2 {

class Scene;

struct SceneComponent
{
    ID<Scene> scene;
};

} // namespace hyperion::v2

#endif