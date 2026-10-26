/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

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
    forcedLod = other.forcedLod;
    lodBias = other.lodBias;
    instanceData = other.instanceData;
    previousModelMatrix = other.previousModelMatrix;
    userData = other.userData;

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
    forcedLod = other.forcedLod;
    lodBias = other.lodBias;
    instanceData = std::move(other.instanceData);
    previousModelMatrix = std::move(other.previousModelMatrix);
    userData = std::move(other.userData);

    if (skeletonInstance)
    {
        EnqueueDeletion(std::move(skeletonInstance));
    }

    skeletonInstance = std::move(other.skeletonInstance);

    return *this;
}

const Handle<Skeleton>& MeshComponent::GetRenderSkeleton() const
{
    // a copied component can still point at another entity's instance until the AnimationSystem replaces it
    if (skeletonInstance.IsValid() && skeletonInstance->GetInstanceSource() == skeleton.Get())
    {
        return skeletonInstance;
    }

    return skeleton;
}

MeshComponent::~MeshComponent()
{
    if (mesh.IsValid())
        EnqueueDeletion(std::move(mesh));

    if (material.IsValid())
        EnqueueDeletion(std::move(material));

    if (skeleton.IsValid())
        EnqueueDeletion(std::move(skeleton));

    if (skeletonInstance.IsValid())
        EnqueueDeletion(std::move(skeletonInstance));
}

} // namespace Hyperion
