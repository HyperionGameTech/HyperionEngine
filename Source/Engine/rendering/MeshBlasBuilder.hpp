/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Mesh;
class Material;

class HYP_API MeshBlasBuilder
{
public:
    static GpuBlasRef Build(Mesh* mesh, Material* material = nullptr);
};

} // namespace Hyperion
