/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MESH_INSTANCE_DATA_HPP
#define HYPERION_MESH_INSTANCE_DATA_HPP

#include <rendering/backend/RenderObject.hpp>

#include <core/containers/Array.hpp>

#include <Types.hpp>

namespace hyperion {

struct MeshInstance
{

};

class HYP_API MeshInstancingData
{
public:
    MeshInstancingData();
    MeshInstancingData(const MeshInstancingData &other)                 = delete;
    MeshInstancingData &operator=(const MeshInstancingData &other)      = delete;
    MeshInstancingData(MeshInstancingData &&other) noexcept             = delete;
    MeshInstancingData &operator=(MeshInstancingData &&other) noexcept  = delete;
    ~MeshInstancingData();

    void SetNumInstances(uint32 num_instances);

    void Create();

private:

    GPUBufferRef    m_instance_buffer;
};

} // namespace hyperion

#endif