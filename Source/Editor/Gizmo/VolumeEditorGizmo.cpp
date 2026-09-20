/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

///Must be before
#include <Scene/Entity.hpp>

#include <Editor/Gizmo/VolumeEditorGizmo.hpp>

#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorAction.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Material.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Ray.hpp>
#include <Core/Logging/Logger.hpp>
#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Debug/Debug.hpp>

#include <System/AppContext.hpp>

#include <VolumeEditorGizmo.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

enum VolumeEditorFace : int
{
    VEF_PosX, // +X face (max.x side)
    VEF_NegX, // -X face (min.x side)
    VEF_PosY, // +Y face (max.y side)
    VEF_NegY, // -Y face (min.y side)
    VEF_PosZ, // +Z face (max.z side)
    VEF_NegZ, // -Z face (min.z side)

    VEF_Max
};

static constexpr Vec3f GetFaceNormal(int faceIndex)
{
    constexpr Vec3f FaceNormals[VEF_Max] = {
        Vec3f(1.0f, 0.0f, 0.0f),
        Vec3f(-1.0f, 0.0f, 0.0f),
        Vec3f(0.0f, 1.0f, 0.0f),
        Vec3f(0.0f, -1.0f, 0.0f),
        Vec3f(0.0f, 0.0f, 1.0f),
        Vec3f(0.0f, 0.0f, -1.0f)
    };

    return FaceNormals[faceIndex];
}

VolumeEditorGizmo::VolumeEditorGizmo()
    : EditorGizmoBase(),
      m_currentBounds(BoundingBox::Zero())
{
}

void VolumeEditorGizmo::UpdateFaceGeometry(const BoundingBox& localBounds, const Vec3f& worldTranslation)
{
    if (!m_node.IsValid())
    {
        return;
    }

    const Vec3f center = localBounds.GetCenter() - worldTranslation;
    const Vec3f extent = localBounds.GetExtent();
    const Vec3f halfExtent = extent * 0.5f;

    for (int i = 0; i < VEF_Max; i++)
    {
        Node* faceNode = m_node->FindChildByName(StringHash(HYP_FORMAT("VolumeFace_{}", i)));

        if (!faceNode)
        {
            continue;
        }

        const Vec3f normal = GetFaceNormal(i);
        Vec3f faceCenter = center + normal * halfExtent[i / 2];
        Vec3f faceScale;

        switch (i)
        {
        case VEF_PosX: // fallthrough
        case VEF_NegX:
            // Face in YZ plane
            faceScale = Vec3f(halfExtent.z, halfExtent.y, 1.0f);
            break;
        case VEF_PosY: // fallthrough
        case VEF_NegY:
            // Face in XZ plane
            faceScale = Vec3f(halfExtent.x, halfExtent.z, 1.0f);
            break;
        case VEF_PosZ: // fallthrough
        case VEF_NegZ:
            // Face in XY plane
            faceScale = Vec3f(halfExtent.x, halfExtent.y, 1.0f);
            break;
        }

        faceNode->UnlockTransform();
        faceNode->SetLocalTranslation(faceCenter);
        faceNode->SetLocalScale(faceScale);
    }

    m_node->UnlockTransform();
    m_node->SetWorldTranslation(worldTranslation);
}

Handle<Node> VolumeEditorGizmo::Load_Internal() const
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { GetEditorAssetRegistry() } };

    if (Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, "VolumeEditGizmo"_sh); prefab.IsValid())
    {
        Handle<Node> rootNode = prefab->GetRoot();

        if (rootNode.IsValid())
        {
            for (const Handle<Node>& child : rootNode->GetChildren())
            {
                if (Entity* entity = DynamicCast<Entity>(child.Get()))
                {
                    if (VisibilityStateComponent* visibilityState = entity->TryGetComponent<VisibilityStateComponent>())
                    {
                        visibilityState->flags |= VisibilityStateFlags::ALWAYS_VISIBLE;
                    }
                    else
                    {
                        entity->AddComponent<VisibilityStateComponent>(VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE });
                    }
                }
            }
        }

        return rootNode;
    }

    return Handle<Node>::Null();
}

void VolumeEditorGizmo::SetFocusedNode(const Handle<Node>& focusedNode)
{
    EditorGizmoBase::SetFocusedNode(focusedNode);

    if (!focusedNode.IsValid() || !m_node.IsValid())
    {
        return;
    }

    m_currentBounds = focusedNode->GetWorldBounds();

    if (!m_currentBounds.IsValid() || !m_currentBounds.IsFinite() || m_currentBounds.IsZero())
    {
        // unbounded volumes (e.g. sky probe) cannot be reshaped
        m_focusedNode.Reset();

        return;
    }

    UpdateFaceGeometry(m_currentBounds, focusedNode->GetWorldTranslation());
}

void VolumeEditorGizmo::OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint)
{
    EditorGizmoBase::OnDragStart(camera, mouseEvent, node, hitpoint);

    m_dragData.Unset();

    Entity* entity = DynamicCast<Entity>(node);
    if (!entity)
    {
        return;
    }

    const NodeTag& faceTag = node->GetTag("VolumeFaceIndex"_sh);

    if (!faceTag)
    {
        return;
    }

    const int faceIndex = faceTag.data.TryGet<int>(-1);

    if (faceIndex < 0 || faceIndex >= VEF_Max)
    {
        return;
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return;
    }

    const Vec3f faceNormal = GetFaceNormal(faceIndex);

    Vec3f planeNormal;

    if (faceIndex / 2 == 1) // y axis
    {
        planeNormal = faceNormal.Cross(camera->GetSideVector()).Normalize();
    }
    else
    {
        planeNormal = faceNormal.Cross(camera->GetUpVector()).Normalize();
    }

    if (planeNormal.LengthSquared() < MathUtil::epsilonF)
    {
        planeNormal = -camera->GetDirection();
    }

    DragData dragData {};
    dragData.faceIndex = faceIndex;
    dragData.faceNormal = faceNormal;
    dragData.planePoint = hitpoint;
    dragData.planeNormal = planeNormal;
    dragData.hitOffset = (hitpoint - focusedNode->GetWorldTranslation()).Dot(faceNormal);
    dragData.originalBounds = m_currentBounds;

    m_dragData = dragData;
}

void VolumeEditorGizmo::OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    EditorGizmoBase::OnDragEnd(camera, mouseEvent);

    // @TODO we should show a "Commit" ui button, and when clicked, that will actually set the

    if (Handle<EditorProject> project = GetCurrentProject(); project.IsValid())
    {
        if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
        {
            if (m_dragData)
            {
                const BoundingBox finalBounds = m_currentBounds;
                const BoundingBox originalBounds = m_dragData->originalBounds;

                // EnvProbes and LightmapVolumes should always be centered on their AABB -
                // recenter the node's world transform to the new bounds' center whenever the shape is edited.
                const bool shouldRecenterTransform = focusedNode->IsA<EnvProbe>() || focusedNode->IsA<LightmapVolume>();

                project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
                    "Edit Volume Shape",
                    [manipulationMode = GetManipulationMode(), focusedNode, finalBounds, originalBounds, shouldRecenterTransform]() -> EditorActionFunctions
                    {
                        return {
                            [focusedNode, finalBounds, manipulationMode, shouldRecenterTransform](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                if (shouldRecenterTransform)
                                {
                                    focusedNode->SetWorldTranslation(finalBounds.GetCenter());
                                }

                                BoundingBox finalBoundsLocal = finalBounds;
                                finalBoundsLocal = focusedNode->GetWorldMatrix().Inverse() * finalBoundsLocal;

                                focusedNode->SetLocalBounds(finalBoundsLocal);

                                editorSubsystem->SetSelectedManipulationMode(manipulationMode);
                                editorSubsystem->SetFocusedNode(focusedNode, true);
                            },
                            [focusedNode, originalBounds, manipulationMode, shouldRecenterTransform](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                if (shouldRecenterTransform)
                                {
                                    focusedNode->SetWorldTranslation(originalBounds.GetCenter());
                                }

                                BoundingBox originalBoundsLocal = originalBounds;
                                originalBoundsLocal = focusedNode->GetWorldMatrix().Inverse() * originalBoundsLocal;

                                focusedNode->SetLocalBounds(originalBoundsLocal);

                                editorSubsystem->SetSelectedManipulationMode(manipulationMode);
                                editorSubsystem->SetFocusedNode(focusedNode, true);
                            }
                        };
                    }));
            }
        }
    }

    m_dragData.Unset();
}

bool VolumeEditorGizmo::OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    Entity* entity = DynamicCast<Entity>(node);
    if (!entity)
    {
        return false;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    MaterialParameters newParameters = meshComponent->material->GetParameters();
    newParameters.albedo = Vec4f(0.7f, 0.35f, 0.0f, 0.35f);
    meshComponent->material->SetParameters(newParameters);

    return true;
}

bool VolumeEditorGizmo::OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    Entity* entity = DynamicCast<Entity>(node);
    if (!entity)
    {
        return false;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->material)
    {
        return false;
    }

    // Restore original face color
    if (const NodeTag& tag = node->GetTag("TransformWidgetElementColor"_sh))
    {
        MaterialParameters newParameters = meshComponent->material->GetParameters();
        newParameters.albedo = tag.data.TryGet<Vec4f>(Vec4f::Zero());

        meshComponent->material->SetParameters(newParameters);
    }

    return true;
}

bool VolumeEditorGizmo::OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
{
    if (!mouseEvent.mouseButtons[MouseButtonState::LEFT])
    {
        return false;
    }

    if (!m_dragData)
    {
        return false;
    }

    Handle<Node> focusedNode = m_focusedNode.Lock();

    if (!focusedNode.IsValid())
    {
        return false;
    }

    AssertDebug(mouseEvent.baseEvent->GetWindow() != nullptr);

    InputManager* inputMgr = mouseEvent.baseEvent->GetWindow()->GetInputManager();
    AssertDebug(inputMgr != nullptr);

    const Ray ray = camera->GetPickRay(inputMgr->GetVirtualMousePositionNormalized());

    RayHit planeRayHit;

    if (Optional<RayHit> planeRayHitOpt = ray.TestPlane(m_dragData->planePoint, m_dragData->planeNormal))
    {
        planeRayHit = *planeRayHitOpt;
    }
    else
    {
        return true;
    }

    const Vec3f worldOffset = planeRayHit.hitpoint - m_dragData->planePoint;
    const int faceIndex = m_dragData->faceIndex;
    const int axis = faceIndex / 2; // 0=X, 1=Y, 2=Z
    Vec3f axisDirection = Vec3f::Zero();
    axisDirection[axis] = 1.0f;
    const float displacement = worldOffset.Dot(axisDirection);

    BoundingBox newBounds = m_dragData->originalBounds;

    if (faceIndex % 2 == 0)
    {
        // positive face
        newBounds.max[axis] = m_dragData->originalBounds.max[axis] + displacement;

        // clamp
        if (newBounds.max[axis] < newBounds.min[axis] + MathUtil::epsilonF)
        {
            newBounds.max[axis] = newBounds.min[axis] + MathUtil::epsilonF;
        }
    }
    else // negative
    {
        newBounds.min[axis] = m_dragData->originalBounds.min[axis] + displacement;

        // clamp
        if (newBounds.min[axis] > newBounds.max[axis] - MathUtil::epsilonF)
        {
            newBounds.min[axis] = newBounds.max[axis] - MathUtil::epsilonF;
        }
    }

    m_currentBounds = newBounds;

    // set new bounds
    // const BoundingBox newBoundsLocal = focusedNode->GetWorldMatrix().Inverse() * newBounds;
    // focusedNode->SetLocalBounds(newBoundsLocal);

    UpdateFaceGeometry(newBounds, focusedNode->GetWorldTranslation());

    return true;
}

bool VolumeEditorGizmo::OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
{
    return false;
}

} // namespace Hyperion
