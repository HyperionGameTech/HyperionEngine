/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DRAW_PROXY_HPP
#define HYPERION_DRAW_PROXY_HPP

#include <core/Base.hpp>
#include <core/ID.hpp>

#include <rendering/RenderBucket.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <math/BoundingBox.hpp>
#include <math/Extent.hpp>
#include <math/Vector4.hpp>
#include <math/Color.hpp>
#include <math/Frustum.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion {

class Mesh;
class Material;
class Engine;
class Entity;
class Camera;
class Skeleton;
class Scene;
class EnvProbe;
class Light;

} // namespace hyperion

#endif