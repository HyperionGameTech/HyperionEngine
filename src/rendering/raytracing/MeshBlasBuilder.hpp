/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Mesh;
class Material;

class HYP_API MeshBlasBuilder
{
public:
    static GpuBlasRef Build(Mesh* mesh, Material* material = nullptr);
};

} // namespace hyperion
