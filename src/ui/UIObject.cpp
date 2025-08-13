/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIObject.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIDataSource.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>

#include <ui/UIScriptDelegate.hpp> // must be included after dotnet headers

#include <util/MeshBuilder.hpp>

#include <core/math/MathUtil.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/components/UIComponent.hpp>

#include <rendering/Mesh.hpp>

#include <input/InputManager.hpp>

#include <core/threading/Threads.hpp>

#include <core/containers/Queue.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <system/AppContext.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

enum class UIObjectFlags : uint32
{
    NONE = 0x0,
    BORDER_TOP_LEFT = 0x1,
    BORDER_TOP_RIGHT = 0x2,
    BORDER_BOTTOM_LEFT = 0x4,
    BORDER_BOTTOM_RIGHT = 0x8
};

HYP_MAKE_ENUM_FLAGS(UIObjectFlags)

struct alignas(16) UIObjectMeshData
{
    uint32 flags = 0u;
    uint32 _pad0;
    Vec2u size;
    Vec4f clampedAabb;
};

static_assert(sizeof(UIObjectMeshData) == sizeof(MeshComponentUserData), "UIObjectMeshData size must match sizeof(MeshComponentUserData)");

#pragma region UIObjectQuadMeshHelper

const Handle<Mesh>& UIObjectQuadMeshHelper::GetQuadMesh()
{
    static struct QuadMeshInitializer
    {
        Handle<Mesh> quad;

        QuadMeshInitializer()
        {
            quad = MeshBuilder::Quad();
            // Hack to make vertices be from 0..1 rather than -1..1

            Assert(quad->GetAsset().IsValid());
            Assert(quad->GetAsset()->GetMeshData() != nullptr);

            MeshData newMeshData = *quad->GetAsset()->GetMeshData();

            for (Vertex& vert : newMeshData.vertexData)
            {
                vert.position.x = (vert.position.x + 1.0f) * 0.5f;
                vert.position.y = (vert.position.y + 1.0f) * 0.5f;
            }

            quad->SetMeshData(newMeshData);
            quad->SetName(NAME("UIObject_QuadMesh"));

            InitObject(quad);
        }
    } quadMeshInitializer;

    return quadMeshInitializer.quad;
}

#pragma endregion UIObjectQuadMeshHelper

#pragma region UIObject

Handle<Mesh> UIObject::GetQuadMesh()
{
    return UIObjectQuadMeshHelper::GetQuadMesh();
}

UIObject::UIObject(const ThreadId& ownerThreadId)
    : m_stage(nullptr),
      m_originAlignment(UIObjectAlignment::TOP_LEFT),
      m_parentAlignment(UIObjectAlignment::TOP_LEFT),
      m_position(0, 0),
      m_size(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT })),
      m_innerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT })),
      m_depth(0),
      m_textSize(-1.0f),
      m_computedTextSize(-1.0f),
      m_borderRadius(5),
      m_borderFlags(UIObjectBorderFlags::NONE),
      m_focusState(UIObjectFocusState::NONE),
      m_isVisible(true),
      m_computedVisibility(false),
      m_isEnabled(true),
      m_acceptsFocus(true),
      m_affectsParentSize(true),
      m_needsRepaint(true),
      m_isPositionAbsolute(false),
      m_allowMaterialUpdate(false),
      m_computedDepth(0),
      m_dataSourceElementUuid(UUID::Invalid()),
      m_deferredUpdates(UIObjectUpdateType::NONE),
      m_lockedUpdates(UIObjectUpdateType::NONE)
{
    OnInit.BindManaged("OnInit", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnAttached.BindManaged("OnAttached", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnRemoved.BindManaged("OnRemoved", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnChildAttached.BindManaged("OnChildAttached", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnChildRemoved.BindManaged("OnChildRemoved", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnMouseDown.BindManaged("OnMouseDown", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnMouseUp.BindManaged("OnMouseUp", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnMouseDrag.BindManaged("OnMouseDrag", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnMouseHover.BindManaged("OnMouseHover", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnMouseLeave.BindManaged("OnMouseLeave", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnMouseMove.BindManaged("OnMouseMove", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnGainFocus.BindManaged("OnGainFocus", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnLoseFocus.BindManaged("OnLoseFocus", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnScroll.BindManaged("OnScroll", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnClick.BindManaged("OnClick", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnKeyDown.BindManaged("OnKeyDown", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnKeyUp.BindManaged("OnKeyUp", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnTextChange.BindManaged("OnTextChange", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnSizeChange.BindManaged("OnSizeChange", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnComputedVisibilityChange.BindManaged("OnComputedVisibilityChange", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnEnabled.BindManaged("OnEnabled", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnDisabled.BindManaged("OnDisabled", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
    OnValueChange.BindManaged("OnValueChange", GetManagedObjectResource(), UIEventHandlerResult::OK).Detach();
}

UIObject::UIObject()
    : UIObject(ThreadId::invalid)
{
}

UIObject::~UIObject()
{
    m_childUiObjects.Clear();

    m_verticalScrollbar.Reset();
    m_horizontalScrollbar.Reset();

    OnInit.RemoveAllDetached();
    OnAttached.RemoveAllDetached();
    OnRemoved.RemoveAllDetached();
    OnChildAttached.RemoveAllDetached();
    OnChildRemoved.RemoveAllDetached();
    OnMouseDown.RemoveAllDetached();
    OnMouseUp.RemoveAllDetached();
    OnMouseDrag.RemoveAllDetached();
    OnMouseHover.RemoveAllDetached();
    OnMouseLeave.RemoveAllDetached();
    OnMouseMove.RemoveAllDetached();
    OnGainFocus.RemoveAllDetached();
    OnLoseFocus.RemoveAllDetached();
    OnScroll.RemoveAllDetached();
    OnClick.RemoveAllDetached();
    OnKeyDown.RemoveAllDetached();
    OnKeyUp.RemoveAllDetached();
    OnComputedVisibilityChange.RemoveAllDetached();
    OnEnabled.RemoveAllDetached();
    OnDisabled.RemoveAllDetached();
}

void UIObject::Init()
{
    HYP_SCOPE;

    Entity::Init();

    if (!m_affectsParentSize || !m_isVisible)
    {
        SetFlags(GetFlags() | NodeFlags::EXCLUDE_FROM_PARENT_AABB);
    }

    // Set it to ignore parent scale so size of the UI object is not affected by the parent
    SetFlags(GetFlags() | NodeFlags::IGNORE_PARENT_SCALE);

    MeshComponent meshComponent;
    meshComponent.mesh = GetQuadMesh();
    meshComponent.material = CreateMaterial();
    meshComponent.instanceData.enableAutoInstancing = true;
    meshComponent.userData = MeshComponentUserData {};

    AddComponent<MeshComponent>(std::move(meshComponent));
    AddComponent<BoundingBoxComponent>(BoundingBoxComponent {});

    // temp. For backwards compatibility
    AddComponent<UIComponent>(UIComponent { this });

    SetReady(true);

    OnInit();
}

void UIObject::Update(float delta)
{
    HYP_SCOPE;

    AssertReady();

    /// @TODO: Built a tree structure of objects that need to be updated
    // in the next tick. sorted by breadth in the child nodes list
    // built it out as we mark updates as needed.

    if (NeedsUpdate())
    {
        Update_Internal(delta);
    }

    // update in breadth-first order
    ForEachChildUIObject([this, delta](UIObject* child)
        {
            if (child->NeedsUpdate())
            {
                child->Update_Internal(delta);
            }

            return IterationResult::CONTINUE;
        },
        /* deep */ true);
}

void UIObject::Update_Internal(float delta)
{
    HYP_SCOPE;

    // If the scroll offset has changed, recalculate the position
    Vec2f scrollOffsetDelta;
    if (m_scrollOffset.Advance(delta, scrollOffsetDelta))
    {
        OnScrollOffsetUpdate(scrollOffsetDelta);
    }

    if (m_deferredUpdates)
    {
        bool updatedPositionOrSize = false;

        { // lock updates within scope; process clamped size at end
            UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);

            if (m_deferredUpdates & (UIObjectUpdateType::UPDATE_SIZE | UIObjectUpdateType::UPDATE_CHILDREN_SIZE))
            {
                UpdateSize(m_deferredUpdates & UIObjectUpdateType::UPDATE_CHILDREN_SIZE);
                updatedPositionOrSize = true;
            }

            if (m_deferredUpdates & (UIObjectUpdateType::UPDATE_POSITION | UIObjectUpdateType::UPDATE_CHILDREN_POSITION))
            {
                UpdatePosition(m_deferredUpdates & UIObjectUpdateType::UPDATE_CHILDREN_POSITION);
                updatedPositionOrSize = true;
            }
        }

        if (updatedPositionOrSize || (m_deferredUpdates & (UIObjectUpdateType::UPDATE_CLAMPED_SIZE | UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE)))
        {
            UpdateClampedSize(updatedPositionOrSize || (m_deferredUpdates & UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE));
        }

        if (m_deferredUpdates & (UIObjectUpdateType::UPDATE_MATERIAL | UIObjectUpdateType::UPDATE_CHILDREN_MATERIAL))
        {
            UpdateMaterial(m_deferredUpdates & UIObjectUpdateType::UPDATE_CHILDREN_MATERIAL);
        }

        if (m_deferredUpdates & (UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY | UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY))
        {
            UpdateComputedVisibility(m_deferredUpdates & UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY);
        }

        if (m_deferredUpdates & (UIObjectUpdateType::UPDATE_MESH_DATA | UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA))
        {
            UpdateMeshData(m_deferredUpdates & UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA);
        }
    }

    if (m_computedVisibility && NeedsRepaint())
    {
        Repaint();
    }
}

void UIObject::OnAttached_Internal(UIObject* parent)
{
    HYP_SCOPE;

    Assert(parent != nullptr);

    SetStage_Internal(parent->GetStage());

    if (IsInitCalled())
    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);

        UpdateSize();
        UpdatePosition();

        UpdateComputedDepth();
        UpdateComputedTextSize();

        SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, true);
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_CLAMPED_SIZE, true);
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, true);
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY, true);
    }

    if (m_isEnabled && !parent->IsEnabled())
    {
        OnDisabled();
    }

    OnAttached();
}

void UIObject::OnRemoved_Internal()
{
    HYP_SCOPE;

    if (m_textSize <= 0.0f)
    {
        // invalidate computed text size
        m_computedTextSize = -1.0f;
    }

    SetStage_Internal(nullptr);

    if (UIObject* parent = GetParentUIObject())
    {
        if (m_isEnabled && !parent->IsEnabled())
        {
            OnEnabled();
        }
    }

    if (IsInitCalled())
    {
        UpdateSize();
        UpdatePosition();

        UpdateMeshData();

        UpdateComputedVisibility();
        UpdateComputedDepth();
        UpdateComputedTextSize();
    }

    OnRemoved();
}

UIStage* UIObject::GetStage() const
{
    if (m_stage != nullptr)
    {
        return m_stage;
    }

    // if (IsA<UIStage>()) {
    //     return const_cast<UIStage *>(static_cast<const UIStage *>(this));
    // }

    return nullptr;
}

void UIObject::SetStage(UIStage* stage)
{
    HYP_SCOPE;

    if (stage != m_stage)
    {
        SetStage_Internal(stage);
    }
}

Name UIObject::GetName() const
{
    return m_name;
}

void UIObject::SetName(Name name)
{
    m_name = name;
}

Vec2i UIObject::GetPosition() const
{
    return m_position;
}

void UIObject::SetPosition(Vec2i position)
{
    HYP_SCOPE;

    m_position = position;

    UpdatePosition(/* updateChildren */ false);
}

Vec2f UIObject::GetOffsetPosition() const
{
    return m_offsetPosition;
}

Vec2f UIObject::GetAbsolutePosition() const
{
    HYP_SCOPE;

    const Vec3f worldTranslation = GetWorldTranslation();

    return { worldTranslation.x, worldTranslation.y };
}

void UIObject::SetIsPositionAbsolute(bool isPositionAbsolute)
{
    HYP_SCOPE;

    if (m_isPositionAbsolute == isPositionAbsolute)
    {
        return;
    }

    m_isPositionAbsolute = isPositionAbsolute;

    UpdatePosition(/* updateChildren */ false);
}

void UIObject::UpdatePosition(bool updateChildren)
{
    HYP_SCOPE;

    if (!IsInitCalled())
    {
        return;
    }

    if (m_lockedUpdates & UIObjectUpdateType::UPDATE_POSITION)
    {
        return;
    }

    m_deferredUpdates &= ~(UIObjectUpdateType::UPDATE_POSITION | (updateChildren ? UIObjectUpdateType::UPDATE_CHILDREN_POSITION : UIObjectUpdateType::NONE));

    ComputeOffsetPosition();

    UpdateNodeTransform();

    if (updateChildren)
    {
        ForEachChildUIObject([](UIObject* child)
            {
                child->UpdatePosition(true);

                return IterationResult::CONTINUE;
            },
            /* deep */ false);
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_CLAMPED_SIZE, true);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY, true);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);
}

UIObjectSize UIObject::GetSize() const
{
    return m_size;
}

void UIObject::SetSize(UIObjectSize size)
{
    HYP_SCOPE;

    m_size = size;

    if (!IsInitCalled())
    {
        return;
    }

    UpdateSize();
}

UIObjectSize UIObject::GetInnerSize() const
{
    return m_innerSize;
}

void UIObject::SetInnerSize(UIObjectSize size)
{
    HYP_SCOPE;

    m_innerSize = size;

    if (!IsInitCalled())
    {
        return;
    }

    UpdateSize();
}

UIObjectSize UIObject::GetMaxSize() const
{
    return m_maxSize;
}

void UIObject::SetMaxSize(UIObjectSize size)
{
    HYP_SCOPE;

    m_maxSize = size;

    if (!IsInitCalled())
    {
        return;
    }

    UpdateSize();
}

void UIObject::UpdateSize(bool updateChildren)
{
    HYP_SCOPE;

    if (!IsInitCalled())
    {
        return;
    }

    if (m_lockedUpdates & UIObjectUpdateType::UPDATE_SIZE)
    {
        return;
    }

    UpdateSize_Internal(updateChildren);

    if (AffectsParentSize())
    {
        ForEachParentUIObject([](UIObject* parent)
            {
                if (!parent->UseAutoSizing())
                {
                    return IterationResult::STOP;
                }

                parent->SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, /* updateChildren */ false);

                if (!parent->AffectsParentSize())
                {
                    return IterationResult::STOP;
                }

                return IterationResult::CONTINUE;
            });
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY, true);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);
}

void UIObject::UpdateSize_Internal(bool updateChildren)
{
    if (m_lockedUpdates & UIObjectUpdateType::UPDATE_SIZE)
    {
        return;
    }

    m_deferredUpdates &= ~(UIObjectUpdateType::UPDATE_SIZE | (updateChildren ? UIObjectUpdateType::UPDATE_CHILDREN_SIZE : UIObjectUpdateType::NONE));

    UpdateActualSizes(UpdateSizePhase::BEFORE_CHILDREN, UIObjectUpdateSizeFlags::DEFAULT);

    SetEntityAABB(CalculateAABB());
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY);

    Array<UIObject*> deferredChildren;

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        ForEachChildUIObject([updateChildren, &deferredChildren](UIObject* child)
            {
                if (child->GetSize().GetAllFlags() & (UIObjectSize::FILL | UIObjectSize::PERCENT))
                {
                    // Even if updateChildren is false, these objects will need to be updated anyway
                    // as they are dependent on the size of this object
                    // child->SetAffectsParentSize(false);
                    deferredChildren.PushBack(child);
                }
                else if (updateChildren)
                {
                    child->UpdateSize_Internal(/* updateChildren */ true);
                }

                return IterationResult::CONTINUE;
            },
            false);
    }

    // auto needs recalculation
    const bool needsUpdateAfterChildren = true; // UseAutoSizing();

    // // FILL needs to update the size of the children
    // // after the parent has updated its size
    // {
    //     UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);
    //     for (UIObject *child : deferredChildren) {
    //         child->SetAffectsParentSize(true);
    //     }
    // }

    if (needsUpdateAfterChildren)
    {
        UpdateActualSizes(UpdateSizePhase::AFTER_CHILDREN, UIObjectUpdateSizeFlags::DEFAULT);

        SetEntityAABB(CalculateAABB());
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY);
    }

    // FILL needs to update the size of the children
    // after the parent has updated its size
    for (UIObject* child : deferredChildren)
    {
        child->UpdateSize_Internal(/* updateChildren */ true);
    }

    if (IsPositionDependentOnSize())
    {
        UpdatePosition(false);
    }

    ForEachChildUIObject([](UIObject* child)
        {
            if (child->IsPositionDependentOnParentSize())
            {
                child->UpdatePosition(false);
            }

            return IterationResult::CONTINUE;
        },
        /* deep */ false);

    OnSizeChange();

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_CLAMPED_SIZE, true);
}

void UIObject::UpdateClampedSize(bool updateChildren)
{
    if (m_lockedUpdates & UIObjectUpdateType::UPDATE_CLAMPED_SIZE)
    {
        return;
    }

    m_deferredUpdates &= ~(UIObjectUpdateType::UPDATE_CLAMPED_SIZE | (updateChildren ? UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE : UIObjectUpdateType::NONE));

    const Vec2i size = GetActualSize();
    const Vec2f position = GetAbsolutePosition();

    BoundingBox parentAabbClamped;

    m_aabbClamped = { Vec3f { float(position.x), float(position.y), 0.0f }, Vec3f { float(position.x) + float(size.x), float(position.y) + float(size.y), 0.0f } };

    if (UIObject* parent = GetParentUIObject())
    {
        parentAabbClamped = parent->m_aabbClamped;
        m_aabbClamped = m_aabbClamped.Intersection(parent->m_aabbClamped);
    }

    UpdateNodeTransform();

    if (m_aabbClamped.IsValid() && m_aabbClamped.IsFinite())
    {
        m_actualSizeClamped = Vec2i(m_aabbClamped.GetExtent().GetXY());
    }
    else
    {
        m_actualSizeClamped = Vec2i::Zero();
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY, true);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);

    if (updateChildren)
    {
        ForEachChildUIObject([](UIObject* child)
            {
                child->UpdateClampedSize(true);

                return IterationResult::CONTINUE;
            },
            /* deep */ false);
    }
}

void UIObject::UpdateNodeTransform()
{
    const Vec3f aabbExtent = GetWorldAABB().GetExtent();
    const Vec3f aabbClampedExtent = m_aabbClamped.GetExtent();

    float zValue = float(GetComputedDepth());

    if (Node* parentNode = GetParent())
    {
        zValue -= parentNode->GetWorldTranslation().z;
    }

    const Vec2f parentScrollOffset = Vec2f(GetParentScrollOffset());

    Node* parentNode = GetParent();

    Transform worldTransform = GetWorldTransform();

    if (m_isPositionAbsolute)
    {
        SetLocalTranslation(Vec3f {
            float(m_position.x) + m_offsetPosition.x,
            float(m_position.y) + m_offsetPosition.y,
            zValue });
    }
    else
    {
        SetLocalTranslation(Vec3f {
            float(m_position.x) + m_offsetPosition.x - parentScrollOffset.x,
            float(m_position.y) + m_offsetPosition.y - parentScrollOffset.y,
            zValue });
    }
}

Vec2i UIObject::GetScrollOffset() const
{
    return Vec2i(m_scrollOffset.GetValue());
}

void UIObject::SetScrollOffset(Vec2i scrollOffset, bool smooth)
{
    HYP_SCOPE;

    scrollOffset.x = (m_actualInnerSize.x > m_actualSize.x && CanScrollOnAxis(SA_HORIZONTAL))
        ? MathUtil::Clamp(scrollOffset.x, 0, m_actualInnerSize.x - m_actualSize.x)
        : 0;

    scrollOffset.y = (m_actualInnerSize.y > m_actualSize.y && CanScrollOnAxis(SA_VERTICAL))
        ? MathUtil::Clamp(scrollOffset.y, 0, m_actualInnerSize.y - m_actualSize.y)
        : 0;

    m_scrollOffset.SetTarget(Vec2f(scrollOffset));

    if (!smooth)
    {
        m_scrollOffset.SetValue(Vec2f(scrollOffset));
    }

    OnScrollOffsetUpdate(m_scrollOffset.GetValue());
}

void UIObject::ScrollToChild(UIObject* child)
{
    HYP_SCOPE;

    if (!child || child == this)
    {
        return;
    }

    // Check if the child is a descendant of this object
    if (!child->IsOrHasParent(this))
    {
        return;
    }

    // Already in view of this object
    if (m_aabbClamped.Contains(child->m_aabbClamped))
    {
        return;
    }

    // Child is set to not visible
    if (!child->IsVisible())
    {
        return;
    }

    // Get the position of the child relative to this object
    Vec2f childPosition = child->GetAbsolutePosition() - GetAbsolutePosition();

    Vec2i scrollOffset = GetScrollOffset();

    Vec2i newScrollOffset = scrollOffset;
    Vec2i childSize = child->GetActualSize();

    if (childPosition.x < 0)
    {
        newScrollOffset.x = MathUtil::Clamp(int(scrollOffset.x + childPosition.x), 0, m_actualInnerSize.x - m_actualSize.x);
    }
    else if (childPosition.x + childSize.x > m_actualSize.x)
    {
        newScrollOffset.x = MathUtil::Clamp(int(scrollOffset.x + childPosition.x) + childSize.x - m_actualSize.x, 0, m_actualInnerSize.x - m_actualSize.x);
    }

    if (childPosition.y < 0)
    {
        newScrollOffset.y = MathUtil::Clamp(int(scrollOffset.y + childPosition.y), 0, m_actualInnerSize.y - m_actualSize.y);
    }
    else if (childPosition.y + childSize.y > m_actualSize.y)
    {
        newScrollOffset.y = MathUtil::Clamp(int(scrollOffset.y + childPosition.y) + childSize.y - m_actualSize.y, 0, m_actualInnerSize.y - m_actualSize.y);
    }

    if (newScrollOffset != scrollOffset)
    {
        SetScrollOffset(newScrollOffset, /* smooth */ false);
    }
}

void UIObject::SetFocusState(EnumFlags<UIObjectFocusState> focusState)
{
    HYP_SCOPE;

    if (focusState != m_focusState)
    {
        SetFocusState_Internal(focusState);
    }
}

void UIObject::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focusState)
{
    m_focusState = focusState;
}

int UIObject::GetComputedDepth() const
{
    return m_computedDepth;
}

void UIObject::UpdateComputedDepth(bool updateChildren)
{
    HYP_SCOPE;

    int computedDepth = m_depth;

    if (UIObject* parent = GetParentUIObject())
    {
        computedDepth += parent->GetComputedDepth() + 1;
    }

    m_computedDepth = computedDepth;

    if (updateChildren)
    {
        ForEachChildUIObject([](UIObject* uiObject)
            {
                uiObject->UpdateComputedDepth(/* updateChildren */ true);

                return IterationResult::CONTINUE;
            },
            false);
    }
}

int UIObject::GetDepth() const
{
    return m_depth;
}

void UIObject::SetDepth(int depth)
{
    HYP_SCOPE;

    m_depth = MathUtil::Clamp(depth, UIStage::g_minDepth, UIStage::g_maxDepth + 1);

    UpdateComputedDepth();
}

void UIObject::SetAcceptsFocus(bool acceptsFocus)
{
    HYP_SCOPE;

    m_acceptsFocus = acceptsFocus;

    if (!acceptsFocus && HasFocus(true))
    {
        Blur();
    }
}

void UIObject::Focus()
{
    HYP_SCOPE;

    if (!AcceptsFocus())
    {
        return;
    }

    if (GetFocusState() & UIObjectFocusState::FOCUSED)
    {
        return;
    }

    SetFocusState(GetFocusState() | UIObjectFocusState::FOCUSED);

    if (m_stage == nullptr)
    {
        return;
    }

    // Note: Calling `SetFocusedObject` between `SetFocusState` and `OnGainFocus` is intentional
    // as `SetFocusedObject` calls `Blur()` on any previously focused object (which may include a parent of this object)
    // Some UI object types may need to know if any child object is focused when handling `OnLoseFocus`
    m_stage->SetFocusedObject(HandleFromThis());

    OnGainFocus(MouseEvent {});
}

void UIObject::Blur(bool blurChildren)
{
    HYP_SCOPE;

    if (GetFocusState() & UIObjectFocusState::FOCUSED)
    {
        SetFocusState(GetFocusState() & ~UIObjectFocusState::FOCUSED);
        OnLoseFocus(MouseEvent {});
    }

    if (blurChildren)
    {
        ForEachChildUIObject([](UIObject* child)
            {
                child->Blur(false);

                return IterationResult::CONTINUE;
            });
    }

    if (m_stage == nullptr)
    {
        return;
    }

    if (m_stage->GetFocusedObject().GetUnsafe() == this)
    {
        return;
    }

    m_stage->SetFocusedObject(nullptr);
}

void UIObject::SetBorderRadius(uint32 borderRadius)
{
    HYP_SCOPE;

    m_borderRadius = borderRadius;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);
}

void UIObject::SetBorderFlags(EnumFlags<UIObjectBorderFlags> borderFlags)
{
    HYP_SCOPE;

    m_borderFlags = borderFlags;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);
}

UIObjectAlignment UIObject::GetOriginAlignment() const
{
    return m_originAlignment;
}

void UIObject::SetOriginAlignment(UIObjectAlignment alignment)
{
    HYP_SCOPE;

    m_originAlignment = alignment;

    UpdatePosition(/* updateChildren */ false);
}

UIObjectAlignment UIObject::GetParentAlignment() const
{
    return m_parentAlignment;
}

void UIObject::SetParentAlignment(UIObjectAlignment alignment)
{
    HYP_SCOPE;

    m_parentAlignment = alignment;

    UpdatePosition(/* updateChildren */ false);
}

void UIObject::SetAspectRatio(UIObjectAspectRatio aspectRatio)
{
    HYP_SCOPE;

    m_aspectRatio = aspectRatio;

    if (!IsInitCalled())
    {
        return;
    }

    UpdateSize();
    UpdatePosition(/* updateChildren */ false);
}

void UIObject::SetPadding(Vec2i padding)
{
    HYP_SCOPE;

    m_padding = padding;

    if (!IsInitCalled())
    {
        return;
    }

    UpdateSize();
    UpdatePosition(/* updateChildren */ false);
}

void UIObject::SetBackgroundColor(const Color& backgroundColor)
{
    HYP_SCOPE;

    if (m_backgroundColor == backgroundColor)
    {
        return;
    }

    m_backgroundColor = backgroundColor;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MATERIAL, false);
}

Color UIObject::ComputeBlendedBackgroundColor() const
{
    HYP_SCOPE;

    Vec4f blendedColor = Vec4f(m_backgroundColor);

    if (m_backgroundColor.GetAlpha() < 1.0f)
    {
        const UIObject* parentUiObject = GetParentUIObject();

        blendedColor = blendedColor * m_backgroundColor.GetAlpha()
            + (parentUiObject ? Vec4f(parentUiObject->ComputeBlendedBackgroundColor()) : Vec4f::Zero()) * (1.0f - m_backgroundColor.GetAlpha());
    }

    return blendedColor;
}

Color UIObject::GetTextColor() const
{
    HYP_SCOPE;

    if (uint32(m_textColor) == 0)
    {
        Handle<UIObject> spawnParent = GetClosestSpawnParent_Proc([](UIObject* parent)
            {
                return uint32(parent->m_textColor) != 0;
            });

        if (spawnParent != nullptr)
        {
            return spawnParent->m_textColor;
        }
    }

    return m_textColor;
}

void UIObject::SetTextColor(const Color& textColor)
{
    HYP_SCOPE;

    if (textColor == m_textColor)
    {
        return;
    }

    m_textColor = textColor;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MATERIAL, true);
}

void UIObject::SetText(const String& text)
{
    HYP_SCOPE;

    if (m_text == text)
    {
        return;
    }

    m_text = text;
}

float UIObject::GetTextSize() const
{
    HYP_SCOPE;

    return m_computedTextSize;
}

void UIObject::SetTextSize(float textSize)
{
    HYP_SCOPE;

    if (m_textSize == textSize || (m_textSize <= 0.0f && textSize <= 0.0f))
    {
        return;
    }

    m_textSize = textSize;

    m_computedTextSize = -1.0f;

    if (!IsInitCalled())
    {
        return;
    }

    UpdateComputedTextSize();
}

bool UIObject::IsVisible() const
{
    return m_isVisible;
}

void UIObject::SetIsVisible(bool isVisible)
{
    HYP_SCOPE;

    if (isVisible == m_isVisible)
    {
        return;
    }

    m_isVisible = isVisible;

    if (m_isVisible)
    {
        if (m_affectsParentSize)
        {
            SetFlags(GetFlags() & ~NodeFlags::EXCLUDE_FROM_PARENT_AABB);
        }
    }
    else
    {
        SetFlags(GetFlags() | NodeFlags::EXCLUDE_FROM_PARENT_AABB);
    }

    if (IsInitCalled())
    {
        // Will add UPDATE_COMPUTED_VISIBILITY deferred update indirectly.

        UpdateSize();
        UpdatePosition(/* updateChildren */ false);
    }
}

void UIObject::UpdateComputedVisibility(bool updateChildren)
{
    HYP_SCOPE;

    if (m_lockedUpdates & UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY)
    {
        return;
    }

    m_deferredUpdates &= ~(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY | (updateChildren ? UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY : UIObjectUpdateType::NONE));

    bool computedVisibility = m_computedVisibility;

    // If the object is visible and has a stage (or if this is a UIStage), consider it
    const bool hasStage = m_stage != nullptr || IsA<UIStage>();

    if (IsVisible() && hasStage)
    {
        if (UIObject* parentUiObject = GetParentUIObject())
        {
            computedVisibility = parentUiObject->GetComputedVisibility()
                && m_aabbClamped.IsValid()
                && parentUiObject->m_aabbClamped.Overlaps(m_aabbClamped);
        }
        else
        {
            computedVisibility = true;
        }
    }
    else
    {
        computedVisibility = false;
    }

    if (m_computedVisibility != computedVisibility)
    {
        m_computedVisibility = computedVisibility;

        if (const Scene* scene = GetScene())
        {
            if (m_computedVisibility)
            {
                AddTag<EntityTag::UI_OBJECT_VISIBLE>();
            }
            else
            {
                RemoveTag<EntityTag::UI_OBJECT_VISIBLE>();
            }
        }

        OnComputedVisibilityChange();
    }

    if (updateChildren)
    {
        ForEachChildUIObject([](UIObject* child)
            {
                child->UpdateComputedVisibility(true);

                return IterationResult::CONTINUE;
            },
            /* deep */ false);
    }
}

bool UIObject::IsEnabled() const
{
    if (!m_isEnabled)
    {
        return false;
    }

    bool isEnabled = true;

    ForEachParentUIObject([&isEnabled](UIObject* parent)
        {
            if (!parent->m_isEnabled)
            {
                isEnabled = false;

                return IterationResult::STOP;
            }

            return IterationResult::CONTINUE;
        });

    return isEnabled;
}

void UIObject::SetIsEnabled(bool isEnabled)
{
    HYP_SCOPE;

    if (isEnabled == m_isEnabled)
    {
        return;
    }

    m_isEnabled = isEnabled;

    if (isEnabled)
    {
        OnEnabled();
    }
    else
    {
        OnDisabled();
    }

    ForEachChildUIObject([isEnabled](UIObject* child)
        {
            if (child->m_isEnabled)
            {
                if (isEnabled)
                {
                    child->OnEnabled();
                }
                else
                {
                    child->OnDisabled();
                }
            }

            return IterationResult::CONTINUE;
        },
        /* deep */ true);
}

void UIObject::SetCurrentValue(HypData&& value, bool triggerEvent)
{
    HYP_SCOPE;

    m_currentValue = std::move(value);

    if (triggerEvent)
    {
        OnValueChange(m_currentValue);
    }
}

void UIObject::UpdateComputedTextSize()
{
    if (m_computedTextSize > 0.0f)
    {
        // Already computed. Needs to be invalidated by being set to -1.0f to be recomputed.
        return;
    }

    if (m_textSize <= 0.0f)
    {
        Handle<UIObject> spawnParent = GetClosestSpawnParent_Proc([](UIObject* parent)
            {
                return parent->m_textSize > 0.0f;
            });

        if (spawnParent != nullptr)
        {
            m_computedTextSize = spawnParent->m_textSize;
        }
        else
        {
            m_computedTextSize = 16.0f; // default font size
        }
    }
    else
    {
        m_computedTextSize = m_textSize;
    }

    UpdateSize();
    UpdatePosition(/* updateChildren */ false);

    OnTextSizeUpdate();
}

bool UIObject::HasFocus(bool includeChildren) const
{
    HYP_SCOPE;

    if (GetFocusState() & UIObjectFocusState::FOCUSED)
    {
        return true;
    }

    if (!includeChildren)
    {
        return false;
    }

    bool hasFocus = false;

    // check if any child has focus
    ForEachChildUIObject([&hasFocus](UIObject* child)
        {
            // Don't include children in the `HasFocus` check as we're already iterating over them
            if (child->HasFocus(false))
            {
                hasFocus = true;

                return IterationResult::STOP;
            }

            return IterationResult::CONTINUE;
        });

    return hasFocus;
}

void UIObject::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return;
    }

    Assert(!uiObject->IsOrHasParent(this));

    AddChild(uiObject);

    Assert(!m_childUiObjects.Contains(uiObject));
    m_childUiObjects.PushBack(uiObject);

    uiObject->OnAttached_Internal(this);

    OnChildAttached(uiObject.Get());
}

bool UIObject::RemoveChildUIObject(UIObject* uiObject)
{
    HYP_SCOPE;

    if (!uiObject)
    {
        return false;
    }

    UIObject* parentUiObject = uiObject->GetParentUIObject();

    if (!parentUiObject)
    {
        return false;
    }

    if (parentUiObject == this)
    {
        uiObject->OnRemoved_Internal();

        OnChildRemoved(uiObject);

        auto it = m_childUiObjects.Find(uiObject);
        Assert(it != m_childUiObjects.End());

        m_childUiObjects.Erase(it);

        uiObject->Remove();

        if (UseAutoSizing())
        {
            UpdateSize();
        }

        return true;
    }
    else
    {
        return parentUiObject->RemoveChildUIObject(uiObject);
    }

    HYP_LOG(UI, Error, "Failed to remove UIObject {} from parent!", uiObject->GetName());

    return false;
}

int UIObject::RemoveAllChildUIObjects()
{
    HYP_SCOPE;

    int numRemoved = 0;

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        Array<UIObject*> children = GetChildUIObjects(false);

        for (UIObject* child : children)
        {
            if (RemoveChildUIObject(child))
            {
                ++numRemoved;
            }
            else
            {
                HYP_LOG(UI, Error, "Failed to remove UIObject {} from parent {}!", child->GetName(), GetName());
            }
        }
    }

    if (numRemoved > 0 && UseAutoSizing())
    {
        UpdateSize();
    }

    return numRemoved;
}

int UIObject::RemoveAllChildUIObjects(ProcRef<bool(UIObject*)> predicate)
{
    HYP_SCOPE;

    int numRemoved = 0;

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        Array<UIObject*> children = FilterChildUIObjects(predicate, false);

        for (UIObject* child : children)
        {
            if (RemoveChildUIObject(child))
            {
                ++numRemoved;
            }
        }
    }

    if (numRemoved > 0 && UseAutoSizing())
    {
        UpdateSize();
    }

    return numRemoved;
}

void UIObject::ClearDeep()
{
    HYP_SCOPE;

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        Array<UIObject*> children = GetChildUIObjects(false);

        for (UIObject* child : children)
        {
            child->ClearDeep();
        }

        RemoveAllChildUIObjects();
    }

    if (UseAutoSizing())
    {
        UpdateSize();
    }
}

bool UIObject::RemoveFromParent()
{
    HYP_SCOPE;

    if (UIObject* parent = GetParentUIObject())
    {
        return parent->RemoveChildUIObject(this);
    }

    return false;
}

Handle<UIObject> UIObject::DetachFromParent()
{
    HYP_SCOPE;

    Handle<UIObject> strongThis = HandleFromThis();

    if (UIObject* parent = GetParentUIObject())
    {
        parent->RemoveChildUIObject(this);
    }

    return strongThis;
}

Handle<UIObject> UIObject::FindChildUIObject(WeakName name, bool deep) const
{
    HYP_SCOPE;

    Handle<UIObject> foundObject;

    ForEachChildUIObject([name, &foundObject](UIObject* child)
        {
            if (child->GetName() == name)
            {
                foundObject = MakeStrongRef(child);

                return IterationResult::STOP;
            }

            return IterationResult::CONTINUE;
        },
        deep);

    return foundObject;
}

Handle<UIObject> UIObject::FindChildUIObject(ProcRef<bool(UIObject*)> predicate, bool deep) const
{
    HYP_SCOPE;

    Handle<UIObject> foundObject;

    ForEachChildUIObject([&foundObject, &predicate](UIObject* child)
        {
            if (predicate(child))
            {
                foundObject = MakeStrongRef(child);

                return IterationResult::STOP;
            }

            return IterationResult::CONTINUE;
        },
        deep);

    return foundObject;
}

BoundingBox UIObject::CalculateAABB() const
{
    HYP_SCOPE;

    const Vec3f min = Vec3f::Zero();
    const Vec3f max = Vec3f { float(m_actualSize.x), float(m_actualSize.y), 0.0f };

    return BoundingBox { min, max };
}

BoundingBox UIObject::CalculateInnerAABB_Internal() const
{
    HYP_SCOPE;

    return GetLocalAABB();
}

void UIObject::SetAffectsParentSize(bool affectsParentSize)
{
    HYP_SCOPE;

    if (m_affectsParentSize == affectsParentSize)
    {
        return;
    }

    m_affectsParentSize = affectsParentSize;

    if (!m_isVisible)
    {
        return;
    }

    if (m_affectsParentSize)
    {
        SetFlags(GetFlags() & ~NodeFlags::EXCLUDE_FROM_PARENT_AABB);
    }
    else
    {
        SetFlags(GetFlags() | NodeFlags::EXCLUDE_FROM_PARENT_AABB);
    }
}

void UIObject::SetAllowMaterialUpdate(bool allowMaterialUpdate)
{
    HYP_SCOPE;

    if (allowMaterialUpdate == m_allowMaterialUpdate)
    {
        return;
    }

    m_allowMaterialUpdate = allowMaterialUpdate;

    UpdateMaterial(false);
}

MaterialAttributes UIObject::GetMaterialAttributes() const
{
    HYP_SCOPE;

    return MaterialAttributes {
        .shaderDefinition = ShaderDefinition { NAME("UIObject"), ShaderProperties(staticMeshVertexAttributes) },
        .blendFunction = BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
            BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA),
        .cullFaces = FCM_BACK,
        .flags = MAF_NONE
    };
}

Material::ParameterTable UIObject::GetMaterialParameters() const
{
    HYP_SCOPE;

    return Material::ParameterTable {
        { Material::MATERIAL_KEY_ALBEDO, Vec4f(GetBackgroundColor()) }
    };
}

Material::TextureSet UIObject::GetMaterialTextures() const
{
    HYP_SCOPE;

    return Material::TextureSet {};
}

Handle<Material> UIObject::CreateMaterial() const
{
    HYP_SCOPE;

    Material::TextureSet materialTextures = GetMaterialTextures();

    if (AllowMaterialUpdate())
    {
        Handle<Material> material = CreateObject<Material>(
            m_name,
            GetMaterialAttributes(),
            GetMaterialParameters(),
            materialTextures);

        material->SetIsDynamic(true);

        InitObject(material);

        return material;
    }
    else
    {
        return MaterialCache::GetInstance()->GetOrCreate(
            GetMaterialAttributes(),
            GetMaterialParameters(),
            materialTextures);
    }
}

const Handle<Material>& UIObject::GetMaterial() const
{
    HYP_SCOPE;

    if (const MeshComponent* meshComponent = TryGetComponent<MeshComponent>())
    {
        return meshComponent->material;
    }

    return Handle<Material>::empty;
}

const Handle<Mesh>& UIObject::GetMesh() const
{
    HYP_SCOPE;

    if (const MeshComponent* meshComponent = TryGetComponent<MeshComponent>())
    {
        return meshComponent->mesh;
    }

    return Handle<Mesh>::empty;
}

UIObject* UIObject::GetParentUIObject() const
{
    HYP_SCOPE;

    const Scene* scene = GetScene();

    if (!scene)
    {
        return nullptr;
    }

    Node* parentNode = GetParent();

    while (parentNode != nullptr)
    {
        if (UIObject* parentUiObject = ObjCast<UIObject>(parentNode))
        {
            return parentUiObject;
        }

        parentNode = parentNode->GetParent();
    }

    return nullptr;
}

Handle<UIObject> UIObject::GetClosestParentUIObject_Proc(const ProcRef<bool(UIObject*)>& proc) const
{
    HYP_SCOPE;

    const Scene* scene = GetScene();

    if (!scene)
    {
        return Handle<UIObject>::empty;
    }

    Node* parentNode = GetParent();

    while (parentNode)
    {
        if (UIObject* parentUiObject = ObjCast<UIObject>(parentNode))
        {
            if (proc(parentUiObject))
            {
                return MakeStrongRef(parentUiObject);
            }
        }

        parentNode = parentNode->GetParent();
    }

    return Handle<UIObject>::empty;
}

Handle<UIObject> UIObject::GetClosestSpawnParent_Proc(const ProcRef<bool(UIObject*)>& proc) const
{
    HYP_SCOPE;

    Handle<UIObject> parentUiObject = m_spawnParent.Lock();

    while (parentUiObject != nullptr)
    {
        if (proc(parentUiObject.Get()))
        {
            return parentUiObject;
        }

        parentUiObject = parentUiObject->m_spawnParent.Lock();
    }

    return Handle<UIObject>::empty;
}

Vec2i UIObject::GetParentScrollOffset() const
{
    HYP_SCOPE;

    if (UIObject* parentUiObject = GetParentUIObject())
    {
        return parentUiObject->GetScrollOffset();
    }

    return Vec2i::Zero();
}

void UIObject::UpdateActualSizes(UpdateSizePhase phase, EnumFlags<UIObjectUpdateSizeFlags> flags)
{
    HYP_SCOPE;

    if (flags & UIObjectUpdateSizeFlags::MAX_SIZE)
    {
        if (m_maxSize.GetValue().x != 0 || m_maxSize.GetValue().y != 0)
        {
            ComputeActualSize(m_maxSize, m_actualMaxSize, phase);
        }
    }

    if (flags & UIObjectUpdateSizeFlags::OUTER_SIZE)
    {
        ComputeActualSize(m_size, m_actualSize, phase, false);
    }

    if (flags & UIObjectUpdateSizeFlags::INNER_SIZE)
    {
        ComputeActualSize(m_innerSize, m_actualInnerSize, phase, true);
    }

    if (flags & UIObjectUpdateSizeFlags::OUTER_SIZE)
    {
        m_actualSize.x = m_actualMaxSize.x != 0 ? MathUtil::Min(m_actualSize.x, m_actualMaxSize.x) : m_actualSize.x;
        m_actualSize.y = m_actualMaxSize.y != 0 ? MathUtil::Min(m_actualSize.y, m_actualMaxSize.y) : m_actualSize.y;
    }
}

void UIObject::ComputeActualSize(const UIObjectSize& inSize, Vec2i& actualSize, UpdateSizePhase phase, bool isInner)
{
    HYP_SCOPE;

    actualSize = Vec2i { 0, 0 };

    Vec2i selfPadding { 0, 0 };
    Vec2i parentSize { 0, 0 };
    Vec2i parentPadding { 0, 0 };

    Vec2i horizontalScrollbarSize { 0, 0 };
    Vec2i verticalScrollbarSize { 0, 0 };

    UIObject* parentUiObject = GetParentUIObject();

    if (isInner)
    {
        parentSize = GetActualSize();
        parentPadding = GetPadding();

        horizontalScrollbarSize[1] = m_horizontalScrollbar != nullptr && m_horizontalScrollbar->IsVisible() ? scrollbarSize : 0;
        verticalScrollbarSize[0] = m_verticalScrollbar != nullptr && m_verticalScrollbar->IsVisible() ? scrollbarSize : 0;
    }
    else if (parentUiObject != nullptr)
    {
        selfPadding = GetPadding();
        parentSize = parentUiObject->GetActualSize();
        parentPadding = parentUiObject->GetPadding();

        horizontalScrollbarSize[1] = parentUiObject->m_horizontalScrollbar != nullptr && parentUiObject->m_horizontalScrollbar->IsVisible() ? scrollbarSize : 0;
        verticalScrollbarSize[0] = parentUiObject->m_verticalScrollbar != nullptr && parentUiObject->m_verticalScrollbar->IsVisible() ? scrollbarSize : 0;
    }
    else if (m_stage != nullptr)
    {
        selfPadding = GetPadding();
        parentSize = m_stage->GetSurfaceSize();
    }
    else if (IsA<UIStage>())
    {
        actualSize = ObjCast<UIStage>(this)->GetSurfaceSize();
    }
    else
    {
        return;
    }

    const BoundingBox innerAabb = CalculateInnerAABB_Internal();

    if (!innerAabb.IsValid() || !innerAabb.IsFinite())
    {
        // If the inner AABB is not valid, we can't calculate the size
        actualSize = Vec2i { 0, 0 };

        HYP_LOG_ONCE(UI, Warning, "UIObject '{}' has an invalid inner AABB. Cannot compute size.", GetName());

        return;
    }

    const Vec3f innerExtent = innerAabb.GetExtent();
    AssertDebug(MathUtil::IsFinite(innerExtent) && !MathUtil::IsNaN(innerExtent));

    const auto updateSizeComponent = [&](uint32 flags, int componentIndex)
    {
        // percentage based size of parent ui object / surface
        switch (flags)
        {
        case UIObjectSize::PIXEL:
            actualSize[componentIndex] = inSize.GetValue()[componentIndex];

            break;
        case UIObjectSize::PERCENT:
            actualSize[componentIndex] = MathUtil::Floor(float(inSize.GetValue()[componentIndex]) * 0.01f * float(parentSize[componentIndex]));

            // Reduce size due to parent object's padding
            actualSize[componentIndex] -= parentPadding[componentIndex] * 2;

            actualSize[componentIndex] -= horizontalScrollbarSize[componentIndex];
            actualSize[componentIndex] -= verticalScrollbarSize[componentIndex];

            break;
        case UIObjectSize::FILL:
            if (!isInner)
            {
                actualSize[componentIndex] = MathUtil::Max(parentSize[componentIndex] - m_position[componentIndex] - parentPadding[componentIndex] * 2 - horizontalScrollbarSize[componentIndex] - verticalScrollbarSize[componentIndex], 0);
            }

            break;
        case UIObjectSize::AUTO:
            if (phase == UpdateSizePhase::AFTER_CHILDREN)
            {
                actualSize[componentIndex] = MathUtil::Floor(innerExtent[componentIndex]);
                actualSize[componentIndex] += selfPadding[componentIndex] * 2;
                actualSize[componentIndex] += horizontalScrollbarSize[componentIndex];
                actualSize[componentIndex] += verticalScrollbarSize[componentIndex];
            }

            break;
        default:
            HYP_UNREACHABLE();
        }
    };

    updateSizeComponent(inSize.GetFlagsX(), 0);
    updateSizeComponent(inSize.GetFlagsY(), 1);

    if (inSize.GetAllFlags() & UIObjectSize::AUTO)
    {
        if (phase != UpdateSizePhase::AFTER_CHILDREN)
        {
            // fix child object's offsets:
            // - when resizing, the node just sees objects offset by X where X is some number based on the previous size
            //   which is then included in the GetLocalAABB() calculation.
            // - now, the parent actual size has its AUTO components set to 0 (or min size), so we update based on that
            ForEachChildUIObject([](UIObject* child)
                {
                    if (child->IsPositionDependentOnParentSize())
                    {
                        child->UpdatePosition(/* updateChildren */ false);
                    }

                    return IterationResult::CONTINUE;
                },
                false);
        }
    }

    // make sure the actual size is at least 0
    actualSize = MathUtil::Max(actualSize, Vec2i { 0, 0 });
}

void UIObject::ComputeOffsetPosition()
{
    HYP_SCOPE;

    Vec2f offsetPosition { 0.0f, 0.0f };

    switch (m_originAlignment)
    {
    case UIObjectAlignment::TOP_LEFT:
        // no offset
        break;
    case UIObjectAlignment::TOP_RIGHT:
        offsetPosition -= Vec2f(float(m_actualInnerSize.x), 0.0f);

        break;
    case UIObjectAlignment::CENTER:
        offsetPosition -= Vec2f(float(m_actualInnerSize.x) * 0.5f, float(m_actualInnerSize.y) * 0.5f);

        break;
    case UIObjectAlignment::BOTTOM_LEFT:
        offsetPosition -= Vec2f(0.0f, float(m_actualInnerSize.y));

        break;
    case UIObjectAlignment::BOTTOM_RIGHT:
        offsetPosition -= Vec2f(float(m_actualInnerSize.x), float(m_actualInnerSize.y));

        break;
    }

    // where to position the object relative to its parent
    if (UIObject* parentUiObject = GetParentUIObject())
    {
        const Vec2f parentPadding(parentUiObject->GetPadding());
        const Vec2i parentActualSize(parentUiObject->GetActualSize());

        switch (m_parentAlignment)
        {
        case UIObjectAlignment::TOP_LEFT:
            offsetPosition += parentPadding;

            break;
        case UIObjectAlignment::TOP_RIGHT:
            // // auto layout breaks with this alignment
            // if (!(parentUiObject->GetSize().GetFlagsX() & UIObjectSize::AUTO)) {
            offsetPosition += Vec2f(float(parentActualSize.x) - parentPadding.x, parentPadding.y);
            // }

            break;
        case UIObjectAlignment::CENTER:
            // // auto layout breaks with this alignment
            // if (!(parentUiObject->GetSize().GetAllFlags() & UIObjectSize::AUTO)) {
            offsetPosition += Vec2f(float(parentActualSize.x) * 0.5f, float(parentActualSize.y) * 0.5f);
            // }

            break;
        case UIObjectAlignment::BOTTOM_LEFT:
            // // auto layout breaks with this alignment
            // if (!(parentUiObject->GetSize().GetFlagsY() & UIObjectSize::AUTO)) {
            offsetPosition += Vec2f(parentPadding.x, float(parentActualSize.y) - parentPadding.y);
            // }

            break;
        case UIObjectAlignment::BOTTOM_RIGHT:
            // // auto layout breaks with this alignment
            // if (!(parentUiObject->GetSize().GetAllFlags() & UIObjectSize::AUTO)) {
            offsetPosition += Vec2f(float(parentActualSize.x) - parentPadding.x, float(parentActualSize.y) - parentPadding.y);
            // }

            break;
        }
    }

    m_offsetPosition = offsetPosition;
}

void UIObject::UpdateMeshData(bool updateChildren)
{
    HYP_SCOPE;

    if (m_lockedUpdates & UIObjectUpdateType::UPDATE_MESH_DATA)
    {
        return;
    }

    m_deferredUpdates &= ~(UIObjectUpdateType::UPDATE_MESH_DATA | (updateChildren ? UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA : UIObjectUpdateType::NONE));

    if (updateChildren)
    {
        ForEachChildUIObject([](UIObject* child)
            {
                // Do not update children in the next call; ForEachChildUIObject runs for all descendants
                child->UpdateMeshData(true);

                return IterationResult::CONTINUE;
            },
            /* deep */ false);
    }

    UpdateMeshData_Internal();
}

void UIObject::UpdateMeshData_Internal()
{
    HYP_SCOPE;

    const Scene* scene = GetScene();

    if (!scene)
    {
        return;
    }

    MeshComponent* meshComponent = TryGetComponent<MeshComponent>();

    if (!meshComponent)
    {
        return;
    }

    UIObjectMeshData uiObjectMeshData {};
    uiObjectMeshData.size = Vec2u(m_actualSize);

    uiObjectMeshData.clampedAabb = Vec4f(
        m_aabbClamped.min.x,
        m_aabbClamped.min.y,
        m_aabbClamped.max.x,
        m_aabbClamped.max.y);

    uiObjectMeshData.flags = (m_borderRadius & 0xFFu)
        | ((uint32(m_borderFlags) & 0xFu) << 8u)
        | ((uint32(m_focusState) & 0xFFu) << 16u);

    meshComponent->userData.Set(uiObjectMeshData);

    Matrix4 instanceTransform;
    instanceTransform[0][0] = m_aabbClamped.max.x - m_aabbClamped.min.x;
    instanceTransform[1][1] = m_aabbClamped.max.y - m_aabbClamped.min.y;
    instanceTransform[2][2] = 1.0f;
    instanceTransform[0][3] = m_aabbClamped.min.x;
    instanceTransform[1][3] = m_aabbClamped.min.y;

    Vec4f instanceTexcoords = Vec4f { 0.0f, 0.0f, 1.0f, 1.0f };

    Vec4f instanceOffsets = Vec4f(GetAbsolutePosition() - m_aabbClamped.min.GetXY(), 0.0f, 0.0f);

    Vec4f instanceSizes = Vec4f(Vec2f(m_actualSize), m_aabbClamped.max.GetXY() - m_aabbClamped.min.GetXY());

    meshComponent->instanceData.numInstances = 1;
    meshComponent->instanceData.SetBufferData(0, &instanceTransform, 1);
    meshComponent->instanceData.SetBufferData(1, &instanceTexcoords, 1);
    meshComponent->instanceData.SetBufferData(2, &instanceOffsets, 1);
    meshComponent->instanceData.SetBufferData(3, &instanceSizes, 1);

    AddTag<EntityTag::UPDATE_RENDER_PROXY>();
}

void UIObject::UpdateMaterial(bool updateChildren)
{
    HYP_SCOPE;

    if (m_lockedUpdates & UIObjectUpdateType::UPDATE_MATERIAL)
    {
        return;
    }

    m_deferredUpdates &= ~(UIObjectUpdateType::UPDATE_MATERIAL | (updateChildren ? UIObjectUpdateType::UPDATE_CHILDREN_MATERIAL : UIObjectUpdateType::NONE));

    if (updateChildren)
    {
        ForEachChildUIObject([](UIObject* child)
            {
                child->UpdateMaterial(true);

                return IterationResult::CONTINUE;
            },
            /* deep */ false);
    }

    MeshComponent* meshComponent = TryGetComponent<MeshComponent>();

    if (!meshComponent)
    {
        return;
    }

    const Handle<Material>& currentMaterial = meshComponent->material;

    MaterialAttributes materialAttributes = GetMaterialAttributes();
    Material::ParameterTable materialParameters = GetMaterialParameters();
    Material::TextureSet materialTextures = GetMaterialTextures();

    if (!currentMaterial.IsValid()
        || (!currentMaterial->IsDynamic() && AllowMaterialUpdate()) // set to dynamic if we allow material updates
        || currentMaterial->GetRenderAttributes() != materialAttributes)
    {
        // need to get a new Material if attributes have changed
        Handle<Material> newMaterial = CreateMaterial();

        meshComponent->material = std::move(newMaterial);

        AddTag<EntityTag::UPDATE_RENDER_PROXY>();

        return;
    }

    bool parametersChanged = materialParameters != currentMaterial->GetParameters();
    bool texturesChanged = materialTextures != currentMaterial->GetTextures();

    if (parametersChanged || texturesChanged)
    {
        Handle<Material> newMaterial;

        if (currentMaterial->IsDynamic())
        {
            newMaterial = currentMaterial;
        }
        else
        {
            newMaterial = currentMaterial->Clone();
            InitObject(newMaterial);

            meshComponent->material = newMaterial;

            AddTag<EntityTag::UPDATE_RENDER_PROXY>();
        }

        Assert(newMaterial->IsDynamic());

        if (parametersChanged)
        {
            newMaterial->SetParameters(materialParameters);
        }

        if (texturesChanged)
        {
            newMaterial->SetTextures(materialTextures);
        }
    }

    SetNeedsRepaintFlag();
}

bool UIObject::HasChildUIObjects() const
{
    HYP_SCOPE;

    return m_childUiObjects.Any();
}

ScriptComponent* UIObject::GetScriptComponent(bool deep) const
{
    HYP_SCOPE;

    const UIObject* currentUiObject = this;
    Handle<UIObject> parentUiObject;

    while (currentUiObject != nullptr)
    {
        if (ScriptComponent* scriptComponent = currentUiObject->TryGetComponent<ScriptComponent>())
        {
            return scriptComponent;
        }

        if (!deep)
        {
            return nullptr;
        }

        parentUiObject = currentUiObject->m_spawnParent.Lock();
        currentUiObject = parentUiObject.Get();
    }

    return nullptr;
}

void UIObject::SetScriptComponent(ScriptComponent&& scriptComponent)
{
    HYP_SCOPE;

    if (HasComponent<ScriptComponent>())
    {
        Assert(RemoveComponent<ScriptComponent>());
    }

    AddComponent<ScriptComponent>(std::move(scriptComponent));
}

void UIObject::RemoveScriptComponent()
{
    HYP_SCOPE;

    if (!HasComponent<ScriptComponent>())
    {
        return;
    }

    RemoveComponent<ScriptComponent>();
}

Handle<UIObject> UIObject::GetChildUIObject(int index) const
{
    HYP_SCOPE;

    Handle<UIObject> foundObject;

    int currentIndex = 0;

    ForEachChildUIObject([&foundObject, &currentIndex, index](UIObject* child)
        {
            if (currentIndex == index)
            {
                foundObject = MakeStrongRef(child);

                return IterationResult::STOP;
            }

            ++currentIndex;

            return IterationResult::CONTINUE;
        },
        /* deep */ false);

    return foundObject;
}

void UIObject::CollectObjects(ProcRef<void(UIObject*)> proc, bool onlyVisible) const
{
    HYP_SCOPE;

    Scene* scene = GetScene();

    if (!scene)
    {
        return;
    }

    if (onlyVisible)
    {
        for (auto [entity, uiComponent, _] : scene->GetEntityManager()->GetEntitySet<UIComponent, EntityTagComponent<EntityTag::UI_OBJECT_VISIBLE>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!uiComponent.uiObject)
            {
                continue;
            }

            proc(uiComponent.uiObject);
        }
    }
    else
    {
        for (auto [entity, uiComponent] : scene->GetEntityManager()->GetEntitySet<UIComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!uiComponent.uiObject)
            {
                continue;
            }

            proc(uiComponent.uiObject);
        }
    }

    // // Visibility affects all child nodes as well, so return from here.
    // if (onlyVisible && !GetComputedVisibility()) {
    //     return;
    // }

    // proc(const_cast<UIObject *>(this));

    // Array<Pair<Node *, UIObject *>> children;
    // children.Reserve(m_node->GetChildren().Size());

    // for (const auto &it : m_node->GetChildren()) {
    //     if (!it.IsValid()) {
    //         continue;
    //     }

    //     UIComponent *uiComponent = it->GetEntity().IsValid()
    //         ? scene->GetEntityManager()->TryGetComponent<UIComponent>(it->GetEntity())
    //         : nullptr;

    //     if (!uiComponent) {
    //         continue;
    //     }

    //     if (!uiComponent->uiObject) {
    //         continue;
    //     }

    //     children.PushBack({
    //         it.Get(),
    //         uiComponent->uiObject
    //     });
    // }

    // std::sort(children.Begin(), children.End(), [](const Pair<Node *, UIObject *> &lhs, const Pair<Node *, UIObject *> &rhs)
    // {
    //     return lhs.second->GetDepth() < rhs.second->GetDepth();
    // });

    // for (const Pair<Node *, UIObject *> &it : children) {
    //     it.second->CollectObjects(proc, outDeferredChildObjects, onlyVisible);
    // }
}

void UIObject::CollectObjects(Array<UIObject*>& outObjects, bool onlyVisible) const
{
    CollectObjects([&outObjects](UIObject* uiObject)
        {
            outObjects.PushBack(uiObject);
        },
        onlyVisible);
}

Vec2f UIObject::TransformScreenCoordsToRelative(Vec2i coords) const
{
    HYP_SCOPE;

    const Vec2i actualSize = GetActualSize();
    const Vec2f absolutePosition = GetAbsolutePosition();

    return (Vec2f(coords) - absolutePosition) / Vec2f(actualSize);
}

Array<UIObject*> UIObject::GetChildUIObjects(bool deep) const
{
    HYP_SCOPE;

    Array<UIObject*> childObjects;

    ForEachChildUIObject([&childObjects](UIObject* child)
        {
            childObjects.PushBack(child);

            return IterationResult::CONTINUE;
        },
        deep);

    return childObjects;
}

Array<UIObject*> UIObject::FilterChildUIObjects(ProcRef<bool(UIObject*)> predicate, bool deep) const
{
    HYP_SCOPE;

    Array<UIObject*> childObjects;

    ForEachChildUIObject([&childObjects, &predicate](UIObject* child)
        {
            if (predicate(child))
            {
                childObjects.PushBack(child);
            }

            return IterationResult::CONTINUE;
        },
        deep);

    return childObjects;
}

template <class Lambda>
void UIObject::ForEachChildUIObject(Lambda&& lambda, bool deep) const
{
    HYP_SCOPE;

    if (!deep)
    {
        // If not deep, iterate using the child UI objects list - more efficient this way
        for (const Handle<UIObject>& child : m_childUiObjects)
        {
            if (!child)
            {
                continue;
            }

            const IterationResult iterationResult = lambda(child.Get());

            // stop iterating if stop was set to true
            if (iterationResult == IterationResult::STOP)
            {
                return;
            }
        }

        return;
    }

    Queue<const UIObject*> queue;
    queue.Push(this);

    while (queue.Any())
    {
        const UIObject* parent = queue.Pop();

        for (const Handle<UIObject>& child : parent->m_childUiObjects)
        {
            const IterationResult iterationResult = lambda(child.Get());

            // stop iterating if stop was set to true
            if (iterationResult == IterationResult::STOP)
            {
                return;
            }

            queue.Push(child.Get());
        }
    }
}

void UIObject::ForEachChildUIObject_Proc(ProcRef<IterationResult(UIObject*)> proc, bool deep) const
{
    ForEachChildUIObject(proc, deep);
}

template <class Lambda>
void UIObject::ForEachParentUIObject(Lambda&& lambda) const
{
    HYP_SCOPE;

    Node* parentNode = GetParent();

    while (parentNode != nullptr)
    {
        if (UIObject* parentUiObject = ObjCast<UIObject>(parentNode))
        {
            const IterationResult iterationResult = lambda(parentUiObject);

            // stop iterating if stop was set to true
            if (iterationResult == IterationResult::STOP)
            {
                return;
            }
        }

        parentNode = parentNode->GetParent();
    }
}

void UIObject::SetStage_Internal(UIStage* stage)
{
    HYP_SCOPE;

    m_stage = stage;

    OnFontAtlasUpdate_Internal();
    OnTextSizeUpdate_Internal();

    ForEachChildUIObject([this, stage](UIObject* uiObject)
        {
            if (uiObject == this)
            {
                return IterationResult::CONTINUE;
            }

            uiObject->SetStage_Internal(stage);

            return IterationResult::CONTINUE;
        },
        /* deep */ false);
}

void UIObject::OnFontAtlasUpdate()
{
    HYP_SCOPE;

    // Update font atlas for all children
    ForEachChildUIObject([](UIObject* child)
        {
            child->OnFontAtlasUpdate_Internal();

            return IterationResult::CONTINUE;
        },
        /* deep */ true);
}

void UIObject::OnTextSizeUpdate()
{
    HYP_SCOPE;

    ForEachChildUIObject([](UIObject* child)
        {
            child->OnTextSizeUpdate_Internal();

            return IterationResult::CONTINUE;
        },
        /* deep */ true);
}

void UIObject::OnScrollOffsetUpdate(Vec2f delta)
{
    HYP_SCOPE;

    // Update child element's offset positions - they are dependent on parent scroll offset
    ForEachChildUIObject([](UIObject* child)
        {
            child->UpdatePosition(/* updateChildren */ false);

            return IterationResult::CONTINUE;
        },
        /* deep */ false);

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_CLAMPED_SIZE, true);

    OnScrollOffsetUpdate_Internal(delta);
}

void UIObject::SetDataSource(const Handle<UIDataSourceBase>& dataSource)
{
    HYP_SCOPE;

    if (dataSource == m_dataSource)
    {
        return;
    }

    m_dataSourceOnChangeHandler.Reset();
    m_dataSourceOnElementAddHandler.Reset();
    m_dataSourceOnElementRemoveHandler.Reset();
    m_dataSourceOnElementUpdateHandler.Reset();

    m_dataSource = dataSource;

    SetDataSource_Internal(m_dataSource.Get());
}

void UIObject::SetDataSource_Internal(UIDataSourceBase* dataSource)
{
    HYP_SCOPE;

    if (!dataSource)
    {
        return;
    }
}

bool UIObject::NeedsRepaint() const
{
    return m_needsRepaint.Get(MemoryOrder::ACQUIRE);
}

void UIObject::SetNeedsRepaintFlag(bool needsRepaint)
{
    m_needsRepaint.Set(needsRepaint, MemoryOrder::RELEASE);
}

void UIObject::Repaint()
{
    HYP_SCOPE;

    if (Repaint_Internal())
    {
        SetNeedsRepaintFlag(false);
    }
}

bool UIObject::Repaint_Internal()
{
    return true;
}

Handle<UIObject> UIObject::CreateUIObject(const HypClass* hypClass, Name name, Vec2i position, UIObjectSize size)
{
    HYP_SCOPE;

    if (!hypClass)
    {
        return Handle<UIObject>::empty;
    }

    Assert(hypClass->IsDerivedFrom(UIObject::Class()), "Cannot spawn instance of class that is not a subclass of UIObject");

    AssertOnOwnerThread();

    HypData uiObjectHypData;
    if (!hypClass->CreateInstance(uiObjectHypData))
    {
        return Handle<UIObject>::empty;
    }

    if (!uiObjectHypData.IsValid())
    {
        return Handle<UIObject>::empty;
    }

    Handle<UIObject> uiObject = std::move(uiObjectHypData).Get<Handle<UIObject>>();
    Assert(uiObject != nullptr);

    // Assert(IsInitCalled());

    if (!name.IsValid())
    {
        name = Name::Unique(ANSIString("Unnamed_") + hypClass->GetName().LookupString());
    }

    UIStage* stage = GetStage();

    uiObject->m_spawnParent = WeakHandleFromThis();
    uiObject->m_stage = stage;

    uiObject->SetName(name);
    uiObject->SetPosition(position);
    uiObject->SetSize(size);

    InitObject(uiObject);

    return uiObject;
}

#pragma endregion UIObject

} // namespace hyperion
