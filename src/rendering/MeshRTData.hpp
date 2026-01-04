/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/Pimpl.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/raytracing/RenderAccelerationStructure.hpp>

namespace Hyperion {

class Entity;
class Mesh;
class Material;

using BLASRef = GpuBlasRef;

class MeshRTData
{
public:
    MeshRTData();
    
    MeshRTData(const MeshRTData& other) = delete;
    MeshRTData& operator=(const MeshRTData& other) = delete;

    MeshRTData(MeshRTData&& other) noexcept = delete;
    MeshRTData& operator=(MeshRTData&& other) noexcept = delete;

    ~MeshRTData();

    const BLASRef& GetOrCreateBLAS(Entity* entity, Mesh* mesh, Material* material);
    void InvalidateBLAS(Entity* entity);

private:
    Pimpl<class MeshRTDataImpl> m_impl;
};

} // namespace Hyperion
