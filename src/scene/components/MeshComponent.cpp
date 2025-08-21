/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <scene/components/MeshComponent.hpp>
#include <scene/animation/Skeleton.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <rendering/util/SafeDeleter.hpp>

namespace hyperion {

MeshComponent& MeshComponent::operator=(const MeshComponent& other)
{
    if (this == &other)
    {
        return *this;
    }

    if (mesh)
    {
        SafeDelete(std::move(mesh));
    }

    if (material)
    {
        SafeDelete(std::move(material));
    }

    if (skeleton)
    {
        SafeDelete(std::move(skeleton));
    }

    mesh = other.mesh;
    material = other.material;
    skeleton = other.skeleton;
    instanceData = other.instanceData;
    previousModelMatrix = other.previousModelMatrix;
    lightmapVolumeUuid = other.lightmapVolumeUuid;
    lightmapElementId = other.lightmapElementId;

    return *this;
}

MeshComponent::MeshComponent(MeshComponent&& other) noexcept
    : mesh(std::move(other.mesh)),
      material(std::move(other.material)),
      skeleton(std::move(other.skeleton)),
      instanceData(std::move(other.instanceData)),
      previousModelMatrix(std::move(other.previousModelMatrix)),
      lightmapVolumeUuid(std::move(other.lightmapVolumeUuid)),
      lightmapElementId(other.lightmapElementId)
{
    other.lightmapElementId = ~0u;
}

MeshComponent& MeshComponent::operator=(MeshComponent&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (mesh)
    {
        SafeDelete(std::move(mesh));
    }

    if (material)
    {
        SafeDelete(std::move(material));
    }

    if (skeleton)
    {
        SafeDelete(std::move(skeleton));
    }

    mesh = std::move(other.mesh);
    material = std::move(other.material);
    skeleton = std::move(other.skeleton);
    instanceData = std::move(other.instanceData);
    previousModelMatrix = std::move(other.previousModelMatrix);
    lightmapVolumeUuid = std::move(other.lightmapVolumeUuid);
    lightmapElementId = other.lightmapElementId;

    other.lightmapElementId = ~0u;

    return *this;
}

MeshComponent::~MeshComponent()
{
    if (mesh)
    {
        SafeDelete(std::move(mesh));
    }

    if (material)
    {
        SafeDelete(std::move(material));
    }

    if (skeleton)
    {
        SafeDelete(std::move(skeleton));
    }
}

} // namespace hyperion