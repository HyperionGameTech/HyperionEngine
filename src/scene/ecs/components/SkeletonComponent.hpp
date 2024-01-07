#ifndef HYPERION_V2_ECS_SKELETON_COMPONENT_HPP
#define HYPERION_V2_ECS_SKELETON_COMPONENT_HPP

#include <core/Handle.hpp>
#include <scene/animation/Skeleton.hpp>

namespace hyperion::v2 {

struct SkeletonComponent
{
    Handle<Skeleton> skeleton;
};

} // namespace hyperion::v2

#endif