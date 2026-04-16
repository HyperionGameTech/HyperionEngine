/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/animation/Skeleton.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/MaterialInstance.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <MeshComponent.generated.inl>

namespace Hyperion {

MeshComponent& MeshComponent::operator=(const MeshComponent& other)
{
    if (this == &other)
    {
        return *this;
    }

    if (mesh)
    {
        EnqueueDeletion(std::move(mesh));
    }

    if (material)
    {
        EnqueueDeletion(std::move(material));
    }

    if (skeleton)
    {
        EnqueueDeletion(std::move(skeleton));
    }

    mesh = other.mesh;
    material = other.material;
    skeleton = other.skeleton;
    enableAutoInstancing = other.enableAutoInstancing;
    numInstances = other.numInstances;
    instanceData = other.instanceData;
    previousModelMatrix = other.previousModelMatrix;

    return *this;
}

MeshComponent& MeshComponent::operator=(MeshComponent&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (mesh)
    {
        EnqueueDeletion(std::move(mesh));
    }

    if (material)
    {
        EnqueueDeletion(std::move(material));
    }

    if (skeleton)
    {
        EnqueueDeletion(std::move(skeleton));
    }

    mesh = std::move(other.mesh);
    material = std::move(other.material);
    skeleton = std::move(other.skeleton);
    enableAutoInstancing = other.enableAutoInstancing;
    numInstances = other.numInstances;
    instanceData = std::move(other.instanceData);
    previousModelMatrix = std::move(other.previousModelMatrix);

    return *this;
}

MeshComponent::~MeshComponent()
{
    EnqueueDeletion(std::move(mesh));
    EnqueueDeletion(std::move(material));
    EnqueueDeletion(std::move(skeleton));
}

} // namespace Hyperion
