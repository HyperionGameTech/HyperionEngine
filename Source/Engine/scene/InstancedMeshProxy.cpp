/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/InstancedMeshProxy.hpp>
#include <scene/Entity.hpp>
#include <scene/EntityTag.hpp>
#include <scene/EntityManager.hpp>

#include <InstancedMeshProxy.generated.inl>

namespace Hyperion {

InstancedMeshProxy::InstancedMeshProxy()
{
}


InstancedMeshProxy::~InstancedMeshProxy()
{
}

void InstancedMeshProxy::OnAttachedToNode(Node* node)
{
    Node::OnAttachedToNode(node);

    // If we are being attached to an entity, we want to make sure the new parent entity has the UpdateInstancedMeshData tag
    if (Entity* entity = ObjCast<Entity>(node))
    {
        entity->AddTag<EntityTag::UpdateInstancedMeshData>();
    }
}

void InstancedMeshProxy::OnDetachedFromNode(Node* node)
{
    Node::OnDetachedFromNode(node);

    if (Entity* entity = ObjCast<Entity>(node))
    {
        entity->AddTag<EntityTag::UpdateInstancedMeshData>();
    }
}

void InstancedMeshProxy::OnTransformUpdated()
{
    Node::OnTransformUpdated();

    if (Entity* entity = ObjCast<Entity>(GetParent()))
    {
        entity->AddTag<EntityTag::UpdateInstancedMeshData>();
    }
}

} // namespace Hyperion
