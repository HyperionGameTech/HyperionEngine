/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DRAW_PROXY_HPP
#define HYPERION_DRAW_PROXY_HPP

#include <core/Base.hpp>
#include <core/ID.hpp>

#include <rendering/RenderBucket.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Extent.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Color.hpp>
#include <core/math/Frustum.hpp>

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