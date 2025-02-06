/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIObject.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIDataSource.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>

#include <ui/UIScriptDelegate.hpp> // must be included after dotnet headers

#include <util/MeshBuilder.hpp>

#include <math/MathUtil.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/UIComponent.hpp>

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
    NONE                = 0x0,
    BORDER_TOP_LEFT     = 0x1,
    BORDER_TOP_RIGHT    = 0x2,
    BORDER_BOTTOM_LEFT  = 0x4,
    BORDER_BOTTOM_RIGHT = 0x8
};

HYP_MAKE_ENUM_FLAGS(UIObjectFlags)

struct alignas(16) UIObjectMeshData
{
    uint32  flags = 0u;
    uint32  _pad0;
    Vec2u   size;
    Vec4f   clamped_aabb;
};

static_assert(sizeof(UIObjectMeshData) == sizeof(MeshComponentUserData), "UIObjectMeshData size must match sizeof(MeshComponentUserData)");

#pragma region UIObjectQuadMeshHelper

const Handle<Mesh> &UIObjectQuadMeshHelper::GetQuadMesh()
{
    static struct QuadMeshInitializer
    {
        QuadMeshInitializer()
        {
            Handle<Mesh> tmp = MeshBuilder::Quad();

            // Hack to make vertices be from 0..1 rather than -1..1

            auto mesh_data = tmp->GetStreamedMeshData();
            auto ref = mesh_data->AcquireRef();

            Array<Vertex> vertices = ref->GetMeshData().vertices;
            const Array<uint32> &indices = ref->GetMeshData().indices;

            for (Vertex &vert : vertices) {
                vert.position.x = (vert.position.x + 1.0f) * 0.5f;
                vert.position.y = (vert.position.y + 1.0f) * 0.5f;
            }

            mesh = CreateObject<Mesh>(StreamedMeshData::FromMeshData({ std::move(vertices), indices }));
            InitObject(mesh);
        }

        Handle<Mesh> mesh;
    } quad_mesh_initializer;

    return quad_mesh_initializer.mesh;
}

#pragma endregion UIObjectQuadMeshHelper

#pragma region UIObject

Handle<Mesh> UIObject::GetQuadMesh()
{
    return UIObjectQuadMeshHelper::GetQuadMesh();
}

// @TODO: Refactor UIObject to hold an array of RC<UIObject> for child elements,
// and iterate over that instead of constantly using the EntityManager and having to Lock()
// the Weak<UIObject> that UIComponent holds.

// OR: Rather than using Weak<UIObject> at all, we just store the raw UIObject pointer on UIComponent,
// and the destructor for UIObject will set it to nullptr.

UIObject::UIObject(UIObjectType type, ThreadID owner_thread_id)
    : m_type(type),
      m_owner_thread_id(owner_thread_id.IsValid() ? owner_thread_id : Threads::CurrentThreadID()),
      m_stage(nullptr),
      m_is_init(false),
      m_origin_alignment(UIObjectAlignment::TOP_LEFT),
      m_parent_alignment(UIObjectAlignment::TOP_LEFT),
      m_position(0, 0),
      m_is_position_absolute(false),
      m_size(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT })),
      m_inner_size(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT })),
      m_depth(0),
      m_text_size(-1.0f),
      m_border_radius(5),
      m_border_flags(UIObjectBorderFlags::NONE),
      m_focus_state(UIObjectFocusState::NONE),
      m_is_visible(true),
      m_computed_visibility(false),
      m_computed_depth(0),
      m_accepts_focus(true),
      m_receives_update(true),
      m_data_source_element_uuid(UUID::Invalid()),
      m_affects_parent_size(true),
      m_needs_repaint(true),
      m_deferred_updates(UIObjectUpdateType::NONE),
      m_locked_updates(UIObjectUpdateType::NONE)
{
    OnInit.Bind(UIScriptDelegate< > { this, "OnInit", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnAttached.Bind(UIScriptDelegate< > { this, "OnAttached", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnRemoved.Bind(UIScriptDelegate< > { this, "OnRemoved", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnChildAttached.Bind(UIScriptDelegate< UIObject * > { this, "OnChildAttached", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnChildRemoved.Bind(UIScriptDelegate< UIObject * > { this, "OnChildRemoved", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnMouseDown.Bind(UIScriptDelegate<MouseEvent> { this, "OnMouseDown", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnMouseUp.Bind(UIScriptDelegate<MouseEvent> { this, "OnMouseUp", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnMouseDrag.Bind(UIScriptDelegate<MouseEvent> { this, "OnMouseDrag", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnMouseHover.Bind(UIScriptDelegate<MouseEvent> { this, "OnMouseHover", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnMouseLeave.Bind(UIScriptDelegate<MouseEvent> { this, "OnMouseLeave", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnMouseMove.Bind(UIScriptDelegate<MouseEvent> { this, "OnMouseMove", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE}).Detach();
    OnGainFocus.Bind(UIScriptDelegate<MouseEvent> { this, "OnGainFocus", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnLoseFocus.Bind(UIScriptDelegate<MouseEvent> { this, "OnLoseFocus", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnScroll.Bind(UIScriptDelegate<MouseEvent> { this, "OnScroll", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnClick.Bind(UIScriptDelegate<MouseEvent> { this, "OnClick", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnKeyDown.Bind(UIScriptDelegate<KeyboardEvent> { this, "OnKeyDown", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
    OnKeyUp.Bind(UIScriptDelegate<KeyboardEvent> { this, "OnKeyUp", UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE }).Detach();
}

UIObject::UIObject()
    : UIObject(UIObjectType::OBJECT)
{
}

UIObject::~UIObject()
{
    // Remove the entity from the entity manager
    if (const Handle<Entity> &entity = GetEntity()) {
        Scene *scene = GetScene();

        AssertThrow(scene != nullptr);
        AssertThrow(scene->GetEntityManager() != nullptr);

        if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(entity)) {
            ui_component->ui_object = nullptr;
        }
    }

    m_node_proxy.Reset();
}

void UIObject::Init()
{
    HYP_SCOPE;

    if (m_type != UIObjectType::STAGE) {
        AssertThrowMsg(m_stage != nullptr, "Invalid UIStage pointer provided to UIObject!");
    }

    AssertThrowMsg(m_node_proxy.IsValid(), "Invalid NodeProxy provided to UIObject!");

    const Scene *scene = GetScene();
    AssertThrow(scene != nullptr);

    MeshComponent mesh_component;
    mesh_component.mesh = GetQuadMesh();
    mesh_component.material = CreateMaterial();
    mesh_component.user_data = MeshComponentUserData { };

    scene->GetEntityManager()->AddComponent<MeshComponent>(GetEntity(), std::move(mesh_component));
    scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(GetEntity(), BoundingBoxComponent { });
    
    m_is_init = true;

    OnInit();
}

void UIObject::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    AssertThrow(m_is_init);

    if (ReceivesUpdate()) {
        Update_Internal(delta);
    }

    // update in breadth-first order
    ForEachChildUIObject([this, delta](UIObject *child)
    {
        if (child->ReceivesUpdate()) {
            child->Update_Internal(delta);
        }

        return IterationResult::CONTINUE;
    }, /* deep */ true);
}

void UIObject::Update_Internal(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    // If the scroll offset has changed, recalculate the position
    Vec2f scroll_offset_delta;
    if (m_scroll_offset.Advance(delta, scroll_offset_delta)) {
        OnScrollOffsetUpdate(scroll_offset_delta);
    }

    if (m_deferred_updates) {
        bool updated_position_or_size = false;

        { // lock updates within scope; process clamped size at end
            UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);

            if (m_deferred_updates & (UIObjectUpdateType::UPDATE_SIZE | UIObjectUpdateType::UPDATE_CHILDREN_SIZE)) {
                UpdateSize(m_deferred_updates & UIObjectUpdateType::UPDATE_CHILDREN_SIZE);
                updated_position_or_size = true;
            }

            if (m_deferred_updates & (UIObjectUpdateType::UPDATE_POSITION | UIObjectUpdateType::UPDATE_CHILDREN_POSITION)) {
                UpdatePosition(m_deferred_updates & UIObjectUpdateType::UPDATE_CHILDREN_POSITION);
                updated_position_or_size = true;
            }
        }
            
        if (updated_position_or_size || (m_deferred_updates & (UIObjectUpdateType::UPDATE_CLAMPED_SIZE | UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE))) {
            UpdateClampedSize(updated_position_or_size || (m_deferred_updates & UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE));
        }

        if (m_deferred_updates & (UIObjectUpdateType::UPDATE_MATERIAL | UIObjectUpdateType::UPDATE_CHILDREN_MATERIAL)) {
            UpdateMaterial(m_deferred_updates & UIObjectUpdateType::UPDATE_CHILDREN_MATERIAL);
        }

        if (m_deferred_updates & (UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY | UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY)) {
            UpdateComputedVisibility(m_deferred_updates & UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY);
        }

        if (m_deferred_updates & (UIObjectUpdateType::UPDATE_MESH_DATA | UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA)) {
            UpdateMeshData(m_deferred_updates & UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA);
        }
    }

    if (m_computed_visibility && NeedsRepaint()) {
        Repaint();
    }
}

void UIObject::OnAttached_Internal(UIObject *parent)
{
    HYP_SCOPE;

    AssertThrow(parent != nullptr);

    m_stage = parent->GetStage();
    
    SetAllChildUIObjectsStage(m_stage);

    if (IsInit()) {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);
        
        UpdateSize();
        UpdatePosition();

        UpdateComputedDepth();

        SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, true);
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_CLAMPED_SIZE, true);
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, true);
        SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY, true);
    }

    OnAttached();
}

void UIObject::OnRemoved_Internal()
{
    HYP_SCOPE;

    m_stage = nullptr;

    SetAllChildUIObjectsStage(nullptr);

    if (IsInit()) {
        UpdateSize();
        UpdatePosition();

        UpdateMeshData();
        UpdateComputedVisibility();
        UpdateComputedDepth();
    }

    OnRemoved();
}

UIStage *UIObject::GetStage() const
{
    if (m_stage != nullptr) {
        return m_stage;
    }

    if (IsInstanceOf<UIStage>()) {
        return const_cast<UIStage *>(static_cast<const UIStage *>(this));
    }

    return nullptr;
}

void UIObject::SetStage(UIStage *stage)
{
    HYP_SCOPE;

    if (stage != m_stage) {
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

    UpdatePosition(/* update_children */ false);
}

Vec2f UIObject::GetOffsetPosition() const
{
    return m_offset_position;
}

Vec2f UIObject::GetAbsolutePosition() const
{
    HYP_SCOPE;

    if (const NodeProxy &node = GetNode()) {
        const Vec3f world_translation = node->GetWorldTranslation();

        return { world_translation.x, world_translation.y };
    }

    return Vec2f::Zero();
}

void UIObject::SetIsPositionAbsolute(bool is_position_absolute)
{
    HYP_SCOPE;

    if (m_is_position_absolute == is_position_absolute) {
        return;
    }

    m_is_position_absolute = is_position_absolute;

    UpdatePosition(/* update_children */ false);
}

void UIObject::UpdatePosition(bool update_children)
{
    HYP_SCOPE;

    if (!IsInit()) {
        return;
    }

    if (m_locked_updates & UIObjectUpdateType::UPDATE_POSITION) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_POSITION | (update_children ? UIObjectUpdateType::UPDATE_CHILDREN_POSITION : UIObjectUpdateType::NONE));

    const NodeProxy &node = GetNode();

    if (!node) {
        return;
    }

    ComputeOffsetPosition();

    UpdateNodeTransform();

    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->UpdatePosition(true);

            return IterationResult::CONTINUE;
        }, /* deep */ false);
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

    if (!IsInit()) {
        return;
    }

    UpdateSize();
}

UIObjectSize UIObject::GetInnerSize() const
{
    return m_inner_size;
}

void UIObject::SetInnerSize(UIObjectSize size)
{
    HYP_SCOPE;

    m_inner_size = size;

    if (!IsInit()) {
        return;
    }

    UpdateSize();
}

UIObjectSize UIObject::GetMaxSize() const
{
    return m_max_size;
}

void UIObject::SetMaxSize(UIObjectSize size)
{
    HYP_SCOPE;

    m_max_size = size;

    if (!IsInit()) {
        return;
    }

    UpdateSize();
}

void UIObject::UpdateSize(bool update_children)
{
    HYP_SCOPE;

    if (!IsInit()) {
        return;
    }

    if (m_locked_updates & UIObjectUpdateType::UPDATE_SIZE) {
        return;
    }

    UpdateSize_Internal(update_children);

    if (AffectsParentSize()) {
        ForEachParentUIObject([](UIObject *parent)
        {
            if (!parent->UseAutoSizing()) {
                return IterationResult::STOP;
            }

            parent->SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, /* update_children */ false);

            if (!parent->AffectsParentSize()) {
                return IterationResult::STOP;
            }

            return IterationResult::CONTINUE;
        });
    }

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY, true);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);
}

void UIObject::UpdateSize_Internal(bool update_children)
{
    if (m_locked_updates & UIObjectUpdateType::UPDATE_SIZE) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_SIZE | (update_children ? UIObjectUpdateType::UPDATE_CHILDREN_SIZE : UIObjectUpdateType::NONE));

    UpdateActualSizes(UpdateSizePhase::BEFORE_CHILDREN, UIObjectUpdateSizeFlags::DEFAULT);
    SetEntityAABB(CalculateAABB());

    Array<UIObject *> deferred_children;

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        ForEachChildUIObject([update_children, &deferred_children](UIObject *child)
        {
            if (child->GetSize().GetAllFlags() & (UIObjectSize::FILL | UIObjectSize::PERCENT)) {
                // Even if update_children is false, these objects will need to be updated anyway
                // as they are dependent on the size of this object
                // child->SetAffectsParentSize(false);
                deferred_children.PushBack(child);
            } else if (update_children) {
                child->UpdateSize_Internal(/* update_children */ true);
            }

            return IterationResult::CONTINUE;
        }, false);
    }

    // auto needs recalculation
    const bool needs_update_after_children = true;//UseAutoSizing();

    // // FILL needs to update the size of the children
    // // after the parent has updated its size
    // {
    //     UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);
    //     for (UIObject *child : deferred_children) {
    //         child->SetAffectsParentSize(true);
    //     }
    // }

    if (needs_update_after_children) {
        UpdateActualSizes(UpdateSizePhase::AFTER_CHILDREN, UIObjectUpdateSizeFlags::DEFAULT);
        SetEntityAABB(CalculateAABB());
    }

    // FILL needs to update the size of the children
    // after the parent has updated its size
    for (UIObject *child : deferred_children) {
        child->UpdateSize_Internal(/* update_children */ true);
    }

    if (IsPositionDependentOnSize()) {
        UpdatePosition(false);
    }

    ForEachChildUIObject([](UIObject *child)
    {
        if (child->IsPositionDependentOnParentSize()) {
            child->UpdatePosition(false);
        }

        return IterationResult::CONTINUE;
    }, /* deep */ false);

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_CLAMPED_SIZE, true);
}

void UIObject::UpdateClampedSize(bool update_children)
{
    if (m_locked_updates & UIObjectUpdateType::UPDATE_CLAMPED_SIZE) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_CLAMPED_SIZE | (update_children ? UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE : UIObjectUpdateType::NONE));

    const Vec2i size = GetActualSize();
    const Vec2f position = GetAbsolutePosition();

    BoundingBox parent_aabb_clamped;

    m_aabb_clamped = { Vec3f { float(position.x), float(position.y), 0.0f }, Vec3f { float(position.x) + float(size.x), float(position.y) + float(size.y), 0.0f } };

    if (UIObject *parent = GetParentUIObject()) {
        parent_aabb_clamped = parent->m_aabb_clamped;
        m_aabb_clamped = m_aabb_clamped.Intersection(parent->m_aabb_clamped);
    }

    UpdateNodeTransform();

    m_actual_size_clamped = Vec2i(m_aabb_clamped.GetExtent().GetXY());

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY, true);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);

    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->UpdateClampedSize(true);

            return IterationResult::CONTINUE;
        }, /* deep */ false);
    }
}

void UIObject::UpdateNodeTransform()
{
    if (!m_node_proxy.IsValid()) {
        return;
    }

    const Vec3f aabb_extent = m_aabb.GetExtent();
    const Vec3f aabb_clamped_extent = m_aabb_clamped.GetExtent();
    
    float z_value = float(GetComputedDepth());

    if (Node *parent_node = m_node_proxy->GetParent()) {
        z_value -= parent_node->GetWorldTranslation().z;
    }

    const Vec2f parent_scroll_offset = Vec2f(GetParentScrollOffset());

    Node *parent_node = m_node_proxy->GetParent();

    Transform world_transform = m_node_proxy->GetWorldTransform();

    if (m_is_position_absolute) {
        m_node_proxy->SetLocalTranslation(Vec3f {
            float(m_position.x) + m_offset_position.x,
            float(m_position.y) + m_offset_position.y,
            z_value
        });
    } else {
        m_node_proxy->SetLocalTranslation(Vec3f {
            float(m_position.x) + m_offset_position.x - parent_scroll_offset.x,
            float(m_position.y) + m_offset_position.y - parent_scroll_offset.y,
            z_value
        });
    }
}

Vec2i UIObject::GetScrollOffset() const
{
    return Vec2i(m_scroll_offset.GetValue());
}

void UIObject::SetScrollOffset(Vec2i scroll_offset, bool smooth)
{
    HYP_SCOPE;

    scroll_offset.x = m_actual_inner_size.x > m_actual_size.x
        ? MathUtil::Clamp(scroll_offset.x, 0, m_actual_inner_size.x - m_actual_size.x)
        : 0;

    scroll_offset.y = m_actual_inner_size.y > m_actual_size.y
        ? MathUtil::Clamp(scroll_offset.y, 0, m_actual_inner_size.y - m_actual_size.y)
        : 0;

    m_scroll_offset.SetTarget(Vec2f(scroll_offset));

    if (!smooth) {
        m_scroll_offset.SetValue(Vec2f(scroll_offset));
    }

    OnScrollOffsetUpdate(m_scroll_offset.GetValue());
}

void UIObject::SetFocusState(EnumFlags<UIObjectFocusState> focus_state)
{
    HYP_SCOPE;
    
    if (focus_state != m_focus_state) {
        SetFocusState_Internal(focus_state);
    }
}

void UIObject::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    m_focus_state = focus_state;
}

int UIObject::GetComputedDepth() const
{
    return m_computed_depth;
}

void UIObject::UpdateComputedDepth(bool update_children)
{
    HYP_SCOPE;

    int computed_depth = m_depth;

    if (UIObject *parent = GetParentUIObject()) {
        computed_depth += parent->GetComputedDepth() + 1;
    }

    m_computed_depth = computed_depth;

    if (update_children) {
        ForEachChildUIObject([](UIObject *ui_object)
        {
            ui_object->UpdateComputedDepth(/* update_children */ true);

            return IterationResult::CONTINUE;
        }, false);
    }
}

int UIObject::GetDepth() const
{
    return m_depth;
}

void UIObject::SetDepth(int depth)
{
    HYP_SCOPE;

    m_depth = MathUtil::Clamp(depth, UIStage::min_depth, UIStage::max_depth + 1);

    UpdateComputedDepth();
}

void UIObject::SetAcceptsFocus(bool accepts_focus)
{
    HYP_SCOPE;

    m_accepts_focus = accepts_focus;

    if (!accepts_focus && HasFocus(true)) {
        Blur();
    }
}

void UIObject::Focus()
{
    HYP_SCOPE;
    
    if (!AcceptsFocus()) {
        return;
    }

    if (GetFocusState() & UIObjectFocusState::FOCUSED) {
        return;
    }

    if (m_stage == nullptr) {
        return;
    }

    SetFocusState(GetFocusState() | UIObjectFocusState::FOCUSED);

    // Note: Calling `SetFocusedObject` between `SetFocusState` and `OnGainFocus` is intentional
    // as `SetFocusedObject` calls `Blur()` on any previously focused object (which may include a parent of this object)
    // Some UI object types may need to know if any child object is focused when handling `OnLoseFocus`
    m_stage->SetFocusedObject(RefCountedPtrFromThis());

    OnGainFocus(MouseEvent { });
}

void UIObject::Blur(bool blur_children)
{
    HYP_SCOPE;

    if (GetFocusState() & UIObjectFocusState::FOCUSED) {
        SetFocusState(GetFocusState() & ~UIObjectFocusState::FOCUSED);
        OnLoseFocus(MouseEvent { });
    }

    if (blur_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->Blur(false);

            return IterationResult::CONTINUE;
        });
    }

    if (m_stage == nullptr) {
        return;
    }

    if (m_stage->GetFocusedObject() != this) {
        return;
    }

    m_stage->SetFocusedObject(nullptr);
}

void UIObject::SetBorderRadius(uint32 border_radius)
{
    HYP_SCOPE;

    m_border_radius = border_radius;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);
}

void UIObject::SetBorderFlags(EnumFlags<UIObjectBorderFlags> border_flags)
{
    HYP_SCOPE;

    m_border_flags = border_flags;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);
}

UIObjectAlignment UIObject::GetOriginAlignment() const
{
    return m_origin_alignment;
}

void UIObject::SetOriginAlignment(UIObjectAlignment alignment)
{
    HYP_SCOPE;

    m_origin_alignment = alignment;

    UpdatePosition(/* update_children */ false);
}

UIObjectAlignment UIObject::GetParentAlignment() const
{
    return m_parent_alignment;
}

void UIObject::SetParentAlignment(UIObjectAlignment alignment)
{
    HYP_SCOPE;

    m_parent_alignment = alignment;

    UpdatePosition(/* update_children */ false);
}

void UIObject::SetAspectRatio(UIObjectAspectRatio aspect_ratio)
{
    HYP_SCOPE;

    m_aspect_ratio = aspect_ratio;

    if (!IsInit()) {
        return;
    }

    UpdateSize();
    UpdatePosition(/* update_children */ false);
}

void UIObject::SetPadding(Vec2i padding)
{
    HYP_SCOPE;

    m_padding = padding;

    if (!IsInit()) {
        return;
    }

    UpdateSize();
    UpdatePosition(/* update_children */ false);
}

void UIObject::SetBackgroundColor(const Color &background_color)
{
    HYP_SCOPE;

    m_background_color = background_color;

    // UpdateMaterial(false);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MATERIAL, false);
}

Color UIObject::GetTextColor() const
{
    HYP_SCOPE;

    if (uint32(m_text_color) == 0) {

        RC<UIObject> spawn_parent = GetClosestSpawnParent_Proc([](UIObject *parent)
        {
            return uint32(parent->m_text_color) != 0;
        });

        if (spawn_parent != nullptr) {
            return spawn_parent->m_text_color;
        }
    }

    return m_text_color;
}

void UIObject::SetTextColor(const Color &text_color)
{
    HYP_SCOPE;

    m_text_color = text_color;

    // UpdateMaterial(true);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MATERIAL, false);
}

void UIObject::SetText(const String &text)
{
    m_text = text;
}

float UIObject::GetTextSize() const
{
    HYP_SCOPE;

    if (m_text_size <= 0.0f) {
        RC<UIObject> spawn_parent = GetClosestSpawnParent_Proc([](UIObject *parent)
        {
            return parent->m_text_size > 0.0f;
        });

        if (spawn_parent != nullptr) {
            return spawn_parent->m_text_size;
        }

        return 16.0f; // default font size
    }

    return m_text_size;
}

void UIObject::SetTextSize(float text_size)
{
    HYP_SCOPE;

    if (m_text_size == text_size || (m_text_size <= 0.0f && text_size <= 0.0f)) {
        return;
    }

    m_text_size = text_size;

    if (!IsInit()) {
        return;
    }

    UpdateSize();
    UpdatePosition(/* update_children */ false);

    OnTextSizeUpdate();
}

bool UIObject::IsVisible() const
{
    return m_is_visible;
}

void UIObject::SetIsVisible(bool is_visible)
{
    HYP_SCOPE;

    if (is_visible == m_is_visible) {
        return;
    }

    m_is_visible = is_visible;

    if (const NodeProxy &node = GetNode()) {
        if (m_is_visible) {
            if (m_affects_parent_size) {
                node->SetFlags(node->GetFlags() & ~NodeFlags::EXCLUDE_FROM_PARENT_AABB);
            }
        } else {
            node->SetFlags(node->GetFlags() | NodeFlags::EXCLUDE_FROM_PARENT_AABB);
        }
    }

    if (IsInit()) {
        // Will add UPDATE_COMPUTED_VISIBILITY deferred update indirectly.

        UpdateSize();
        UpdatePosition(/* update_children */ false);
    }
}

void UIObject::UpdateComputedVisibility(bool update_children)
{
    HYP_SCOPE;

    if (m_locked_updates & UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY | (update_children ? UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY : UIObjectUpdateType::NONE));

    bool computed_visibility = m_computed_visibility;

    // If the object is visible and has a stage (or if this is a UIStage), consider it
    const bool has_stage = m_stage != nullptr || IsInstanceOf<UIStage>();

    if (IsVisible() && has_stage) {
        if (UIObject *parent_ui_object = GetParentUIObject()) {
            computed_visibility = parent_ui_object->GetComputedVisibility()
                && m_aabb_clamped.IsValid()
                && parent_ui_object->m_aabb_clamped.Overlaps(m_aabb_clamped);
        } else {
            computed_visibility = true;
        }
    } else {
        computed_visibility = false;
    }

    if (m_computed_visibility != computed_visibility) {
        m_computed_visibility = computed_visibility;

        if (const Scene *scene = GetScene()) {
            if (m_computed_visibility) {
                scene->GetEntityManager()->AddTag<EntityTag::UI_OBJECT_VISIBLE>(GetEntity());
            } else {
                scene->GetEntityManager()->RemoveTag<EntityTag::UI_OBJECT_VISIBLE>(GetEntity());
            }
        }

        OnComputedVisibilityChange_Internal();
    }

    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->UpdateComputedVisibility(true);

            return IterationResult::CONTINUE;
        }, /* deep */ false);
    }
}

bool UIObject::HasFocus(bool include_children) const
{
    HYP_SCOPE;

    if (GetFocusState() & UIObjectFocusState::FOCUSED) {
        return true;
    }

    if (!include_children) {
        return false;
    }

    bool has_focus = false;

    // check if any child has focus
    ForEachChildUIObject([&has_focus](UIObject *child)
    {
        // Don't include children in the `HasFocus` check as we're already iterating over them
        if (child->HasFocus(false)) {
            has_focus = true;

            return IterationResult::STOP;
        }

        return IterationResult::CONTINUE;
    });

    return has_focus;
}

bool UIObject::IsOrHasParent(const UIObject *other) const
{
    HYP_SCOPE;

    if (!other) {
        return false;
    }

    if (this == other) {
        return true;
    }

    const NodeProxy &this_node = GetNode();
    const NodeProxy &other_node = other->GetNode();

    if (!this_node.IsValid() || !other_node.IsValid()) {
        return false;
    }

    return this_node->IsOrHasParent(other_node.Get());
}

void UIObject::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return;
    }

    AssertThrow(!ui_object->IsOrHasParent(this));

    const NodeProxy &node = GetNode();

    if (!node) {
        HYP_LOG(UI, Error, "Parent UI object has no attachable node: {}", GetName());

        return;
    }

    if (NodeProxy child_node = ui_object->GetNode()) {
        if (child_node->GetParent() != nullptr && !child_node->Remove()) {
            HYP_LOG(UI, Error, "Failed to remove child node '{}' from current parent {}", child_node->GetName(), child_node->GetParent()->GetName());

            return;
        }

        node->AddChild(child_node);
    } else {
        HYP_LOG(UI, Error, "Child UI object '{}' has no attachable node", ui_object->GetName());

        return;
    }

    m_child_ui_objects.PushBack(ui_object);
    ui_object->OnAttached_Internal(this);

    OnChildAttached(ui_object.Get());
}

bool UIObject::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return false;
    }

    if (const NodeProxy &child_node = ui_object->GetNode()) {
        if (child_node->IsOrHasParent(node.Get())) {
            Node *parent_node = child_node->GetParent();
            AssertThrow(parent_node != nullptr);

            if (UIComponent *ui_component = parent_node->GetScene()->GetEntityManager()->TryGetComponent<UIComponent>(parent_node->GetEntity())) {
                UIObject *parent_ui_object = ui_component->ui_object;
                AssertThrow(parent_ui_object != nullptr);

                if (parent_ui_object == this) {
                    AssertThrow(child_node->Remove());

                    ui_object->OnRemoved_Internal();

                    OnChildRemoved(ui_object);

                    auto it = m_child_ui_objects.FindAs(ui_object);

                    if (it != m_child_ui_objects.End()) {
                        m_child_ui_objects.Erase(it);
                        
                        if (UseAutoSizing()) {
                            UpdateSize();
                        }

                        return true;
                    }

                    HYP_LOG(UI, Error, "Failed to remove UIObject {} from parent: Parent UIObject {} did not have the child object in its children array!", ui_object->GetName(), parent_ui_object->GetName());

                    return false;
                } else {
                    return parent_ui_object->RemoveChildUIObject(ui_object);
                }
            } else {
                HYP_LOG(UI, Error, "Failed to remove UIObject {} from parent: Parent node ({}) had no UIComponent!", ui_object->GetName(), parent_node->GetName());

                return false;
            }
        }
    }

    HYP_LOG(UI, Error, "Failed to remove UIObject {} from parent!", ui_object->GetName());

    return false;
}

int UIObject::RemoveAllChildUIObjects()
{
    HYP_SCOPE;

    int num_removed = 0;

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        // const NodeProxy &node = GetNode();

        // for (const RC<UIObject> &child_ui_object : m_child_ui_objects) {
        //     if (const NodeProxy &child_node = child_ui_object->GetNode()) {
        //         child_node->Remove();
        //     }
        // }

        // num_removed = int(m_child_ui_objects.Size());

        for (const RC<UIObject> &child_ui_object : m_child_ui_objects) {
            HYP_LOG(UI, Debug, "Remove child {} from {} -- {} refs", child_ui_object->GetName(), GetName(), child_ui_object.GetRefCountData_Internal()->UseCount_Strong());
        }

        // m_child_ui_objects.Clear();

        Array<UIObject *> children = GetChildUIObjects(false);

        for (UIObject *child : children) {
            if (RemoveChildUIObject(child)) {
                ++num_removed;
            }
        }
    }

    if (num_removed > 0 && UseAutoSizing()) {
        UpdateSize();
    }

    return num_removed;
}

int UIObject::RemoveAllChildUIObjects(ProcRef<bool, UIObject *> predicate)
{
    HYP_SCOPE;

    int num_removed = 0;

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);

        Array<UIObject *> children = FilterChildUIObjects(predicate, false);

        for (UIObject *child : children) {
            if (RemoveChildUIObject(child)) {
                ++num_removed;
            }
        }
    }

    if (num_removed > 0 && UseAutoSizing()) {
        UpdateSize();
    }

    return num_removed;
}

bool UIObject::RemoveFromParent()
{
    HYP_SCOPE;
    
    if (UIObject *parent = GetParentUIObject()) {
        return parent->RemoveChildUIObject(this);
    }

    return false;
}

RC<UIObject> UIObject::DetachFromParent()
{
    HYP_SCOPE;
    
    RC<UIObject> this_ref_counted = RefCountedPtrFromThis();

    if (UIObject *parent = GetParentUIObject()) {
        parent->RemoveChildUIObject(this);
    }

    return this_ref_counted;
}

RC<UIObject> UIObject::FindChildUIObject(WeakName name, bool deep) const
{
    HYP_SCOPE;

    RC<UIObject> found_object;

    ForEachChildUIObject([name, &found_object](UIObject *child)
    {
        if (child->GetName() == name) {
            found_object = child->RefCountedPtrFromThis();

            return IterationResult::STOP;
        }

        return IterationResult::CONTINUE;
    }, deep);

    return found_object;
}

RC<UIObject> UIObject::FindChildUIObject(ProcRef<bool, UIObject *> predicate, bool deep) const
{
    HYP_SCOPE;
    
    RC<UIObject> found_object;

    ForEachChildUIObject([&found_object, &predicate](UIObject *child)
    {
        if (predicate(child)) {
            found_object = child->RefCountedPtrFromThis();

            return IterationResult::STOP;
        }

        return IterationResult::CONTINUE;
    }, deep);

    return found_object;
}

const NodeProxy &UIObject::GetNode() const
{
    return m_node_proxy;
}

World *UIObject::GetWorld() const
{
    if (m_node_proxy.IsValid()) {
        return m_node_proxy->GetWorld();
    }

    return nullptr;
}

BoundingBox UIObject::GetWorldAABB() const
{
    HYP_SCOPE;

    if (const NodeProxy &node = GetNode()) {
        return node->GetWorldAABB();
    }

    return BoundingBox::Empty();
}

BoundingBox UIObject::GetLocalAABB() const
{
    HYP_SCOPE;
    
    if (const NodeProxy &node = GetNode()) {
        return node->GetLocalAABB();
    }

    return BoundingBox::Empty();
}

void UIObject::SetEntityAABB(const BoundingBox &aabb)
{
    HYP_SCOPE;

    Transform transform;

    if (Scene *scene = GetScene()) {
        BoundingBoxComponent &bounding_box_component = scene->GetEntityManager()->GetComponent<BoundingBoxComponent>(GetEntity());
        bounding_box_component.local_aabb = aabb;

        if (const NodeProxy &node = GetNode()) {
            node->SetEntityAABB(aabb);

            transform = node->GetWorldTransform();
        }

        bounding_box_component.world_aabb = aabb * transform;
        bounding_box_component.transform_hash_code = transform.GetHashCode();
    }

    m_aabb = aabb * transform;

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY);
}

BoundingBox UIObject::CalculateAABB() const
{
    HYP_SCOPE;

    const Vec3f min = Vec3f::Zero();
    const Vec3f max = Vec3f { float(m_actual_size.x), float(m_actual_size.y), 0.0f };

    return BoundingBox { min, max };
}

BoundingBox UIObject::CalculateInnerAABB_Internal() const
{
    HYP_SCOPE;

    if (const NodeProxy &node = GetNode()) {
        const BoundingBox aabb = node->GetLocalAABB();

        if (aabb.IsFinite() && aabb.IsValid()) {
            return aabb;
        }
    }

    return BoundingBox::Empty();
}

void UIObject::SetAffectsParentSize(bool affects_parent_size)
{
    HYP_SCOPE;

    if (m_affects_parent_size == affects_parent_size) {
        return;
    }

    m_affects_parent_size = affects_parent_size;

    if (!m_is_visible) {
        return;
    }

    if (m_node_proxy.IsValid()) {
        if (m_affects_parent_size) {
            m_node_proxy->SetFlags(m_node_proxy->GetFlags() & ~NodeFlags::EXCLUDE_FROM_PARENT_AABB);
        } else {
            m_node_proxy->SetFlags(m_node_proxy->GetFlags() | NodeFlags::EXCLUDE_FROM_PARENT_AABB);
        }
    }
}

MaterialAttributes UIObject::GetMaterialAttributes() const
{
    HYP_SCOPE;

    return MaterialAttributes {
        .shader_definition  = ShaderDefinition { NAME("UIObject"), ShaderProperties(static_mesh_vertex_attributes) },
        .bucket             = Bucket::BUCKET_UI,
        .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                            BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
        .cull_faces         = FaceCullMode::BACK,
        .flags              = MaterialAttributeFlags::NONE
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

    return Material::TextureSet { };
}

Handle<Material> UIObject::CreateMaterial() const
{
    HYP_SCOPE;

    Material::TextureSet material_textures = GetMaterialTextures();
    
    if (AllowMaterialUpdate()) {
        Handle<Material> material = CreateObject<Material>(
            Name::Unique("UIObjectMaterial"),
            GetMaterialAttributes(),
            GetMaterialParameters(),
            material_textures
        );

        material->SetIsDynamic(true);

        InitObject(material);

        return material;
    } else {
        return MaterialCache::GetInstance()->GetOrCreate(
            Name::Unique("UIObjectMaterial"),
            GetMaterialAttributes(),
            GetMaterialParameters(),
            material_textures
        );
    }
}

const Handle<Material> &UIObject::GetMaterial() const
{
    HYP_SCOPE;

    const Scene *scene = GetScene();
    const Handle<Entity> &entity = GetEntity();

    if (!entity.IsValid() || !scene) {
        return Handle<Material>::empty;
    }

    if (const MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity)) {
        return mesh_component->material;
    }

    return Handle<Material>::empty;
}

const Handle<Mesh> &UIObject::GetMesh() const
{
    HYP_SCOPE;
    
    const Scene *scene = GetScene();
    const Handle<Entity> &entity = GetEntity();

    if (!entity.IsValid() || !scene) {
        return Handle<Mesh>::empty;
    }

    if (const MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity)) {
        return mesh_component->mesh;
    }

    return Handle<Mesh>::empty;
}

UIObject *UIObject::GetParentUIObject() const
{
    HYP_SCOPE;
    
    const Scene *scene = GetScene();

    if (!scene) {
        return nullptr;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return nullptr;
    }

    Node *parent_node = node->GetParent();

    while (parent_node != nullptr) {
        if (parent_node->GetEntity().IsValid()) {
            if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(parent_node->GetEntity())) {
                if (ui_component->ui_object != nullptr) {
                    return ui_component->ui_object;
                }
            }
        }

        parent_node = parent_node->GetParent();
    }

    return nullptr;
}

UIObject *UIObject::GetClosestParentUIObject(UIObjectType type) const
{
    HYP_SCOPE;
    
    const Scene *scene = GetScene();

    if (!scene) {
        return nullptr;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return nullptr;
    }

    Node *parent_node = node->GetParent();

    while (parent_node) {
        if (parent_node->GetEntity().IsValid()) {
            if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(parent_node->GetEntity())) {
                if (ui_component->ui_object != nullptr) {
                    if (ui_component->ui_object->GetType() == type) {
                        return ui_component->ui_object;
                    }
                }
            }
        }

        parent_node = parent_node->GetParent();
    }

    return nullptr;
}

RC<UIObject> UIObject::GetClosestParentUIObject_Proc(const ProcRef<bool, UIObject *> &proc) const
{
    HYP_SCOPE;
    
    const Scene *scene = GetScene();

    if (!scene) {
        return nullptr;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return nullptr;
    }

    Node *parent_node = node->GetParent();

    while (parent_node) {
        if (parent_node->GetEntity().IsValid()) {
            if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(parent_node->GetEntity())) {
                if (ui_component->ui_object != nullptr) {
                    if (proc(ui_component->ui_object)) {
                        return ui_component->ui_object->RefCountedPtrFromThis();
                    }
                }
            }
        }

        parent_node = parent_node->GetParent();
    }

    return nullptr;
}

RC<UIObject> UIObject::GetClosestSpawnParent_Proc(const ProcRef<bool, UIObject *> &proc) const
{
    HYP_SCOPE;

    RC<UIObject> parent_ui_object = m_spawn_parent.Lock();

    while (parent_ui_object != nullptr) {
        if (proc(parent_ui_object.Get())) {
            return parent_ui_object;
        }

        parent_ui_object = parent_ui_object->m_spawn_parent.Lock();
    }

    return nullptr;
}

Vec2i UIObject::GetParentScrollOffset() const
{
    HYP_SCOPE;
    
    if (UIObject *parent_ui_object = GetParentUIObject()) {
        return parent_ui_object->GetScrollOffset();
    }

    return Vec2i::Zero();
}

Scene *UIObject::GetScene() const
{
    HYP_SCOPE;
    
    if (const NodeProxy &node = GetNode()) {
        return node->GetScene();
    }

    return nullptr;
}

void UIObject::UpdateActualSizes(UpdateSizePhase phase, EnumFlags<UIObjectUpdateSizeFlags> flags)
{
    HYP_SCOPE;
    
    if (flags & UIObjectUpdateSizeFlags::MAX_SIZE) {
        if (m_max_size.GetValue().x != 0 || m_max_size.GetValue().y != 0) {
            ComputeActualSize(m_max_size, m_actual_max_size, phase);
        }
    }

    if (flags & UIObjectUpdateSizeFlags::OUTER_SIZE) {
        ComputeActualSize(m_size, m_actual_size, phase, false);
    }

    if (flags & UIObjectUpdateSizeFlags::INNER_SIZE) {
        ComputeActualSize(m_inner_size, m_actual_inner_size, phase, true);
    }

    if (flags & UIObjectUpdateSizeFlags::OUTER_SIZE) {
        m_actual_size.x = m_actual_max_size.x != 0 ? MathUtil::Min(m_actual_size.x, m_actual_max_size.x) : m_actual_size.x;
        m_actual_size.y = m_actual_max_size.y != 0 ? MathUtil::Min(m_actual_size.y, m_actual_max_size.y) : m_actual_size.y;
    }
}

void UIObject::ComputeActualSize(const UIObjectSize &in_size, Vec2i &actual_size, UpdateSizePhase phase, bool is_inner)
{
    HYP_SCOPE;

    actual_size = Vec2i { 0, 0 };
    
    Vec2i self_padding { 0, 0 };
    Vec2i parent_size { 0, 0 };
    Vec2i parent_padding { 0, 0 };

    Vec2i horizontal_scrollbar_size { 0, 0 };
    Vec2i vertical_scrollbar_size { 0, 0 };

    UIObject *parent_ui_object = GetParentUIObject();

    if (is_inner) {
        parent_size = GetActualSize();
        parent_padding = GetPadding();

        horizontal_scrollbar_size[1] = m_horizontal_scrollbar != nullptr ? 25 : 0;
        vertical_scrollbar_size[0] = m_vertical_scrollbar != nullptr ? 25 : 0;
    } else if (parent_ui_object != nullptr) {
        self_padding = GetPadding();
        parent_size = parent_ui_object->GetActualSize();
        parent_padding = parent_ui_object->GetPadding();

        horizontal_scrollbar_size[1] = parent_ui_object->m_horizontal_scrollbar != nullptr ? 25 : 0;
        vertical_scrollbar_size[0] = parent_ui_object->m_vertical_scrollbar != nullptr ? 25 : 0;
    } else if (m_stage != nullptr) {
        self_padding = GetPadding();
        parent_size = m_stage->GetSurfaceSize();
    } else if (IsInstanceOf<UIStage>()) {
        actual_size = static_cast<UIStage *>(this)->GetSurfaceSize();
    } else {
        return;
    }

    const Vec3f inner_extent = CalculateInnerAABB_Internal().GetExtent();

    const auto UpdateSizeComponent = [&](uint32 flags, int component_index)
    {
        // percentage based size of parent ui object / surface
        switch (flags) {
        case UIObjectSize::PIXEL:
            actual_size[component_index] = in_size.GetValue()[component_index];

            break;
        case UIObjectSize::PERCENT:
            actual_size[component_index] = MathUtil::Floor(float(in_size.GetValue()[component_index]) * 0.01f * float(parent_size[component_index]));

            // Reduce size due to parent object's padding
            actual_size[component_index] -= parent_padding[component_index] * 2;

            actual_size[component_index] -= horizontal_scrollbar_size[component_index];
            actual_size[component_index] -= vertical_scrollbar_size[component_index];

            break;
        case UIObjectSize::FILL:
            if (!is_inner) {
                actual_size[component_index] = MathUtil::Max(parent_size[component_index] - m_position[component_index] - parent_padding[component_index] * 2 - horizontal_scrollbar_size[component_index] - vertical_scrollbar_size[component_index], 0);
            }

            break;
        case UIObjectSize::AUTO:
            if (phase == UpdateSizePhase::AFTER_CHILDREN) {
                actual_size[component_index] = MathUtil::Floor(inner_extent[component_index]);
                actual_size[component_index] += self_padding[component_index] * 2;
                actual_size[component_index] += horizontal_scrollbar_size[component_index];
                actual_size[component_index] += vertical_scrollbar_size[component_index];
            }

            break;
        default:
            HYP_UNREACHABLE();
        }
    };

    UpdateSizeComponent(in_size.GetFlagsX(), 0);
    UpdateSizeComponent(in_size.GetFlagsY(), 1);

    if (in_size.GetAllFlags() & UIObjectSize::AUTO) {
        if (phase != UpdateSizePhase::AFTER_CHILDREN) {
            // fix child object's offsets:
            // - when resizing, the node just sees objects offset by X where X is some number based on the previous size
            //   which is then included in the GetLocalAABB() calculation.
            // - now, the parent actual size has its AUTO components set to 0 (or min size), so we update based on that
            ForEachChildUIObject([](UIObject *child)
            {
                if (child->IsPositionDependentOnParentSize()) {
                    child->UpdatePosition(/* update_children */ false);
                }

                return IterationResult::CONTINUE;
            }, false);
        }
    }


    // actual_size = Vec2i { 0, 0 };

    // // percentage based size of parent ui object / surface
    // if (in_size.GetFlagsX() & UIObjectSize::PIXEL) {
    //     actual_size.x = in_size.GetValue().x;
    // }

    // if (in_size.GetFlagsY() & UIObjectSize::PIXEL) {
    //     actual_size.y = in_size.GetValue().y;
    // }

    // // percentage based size of parent ui object / surface
    // if (in_size.GetFlagsX() & UIObjectSize::PERCENT) {
    //     actual_size.x = MathUtil::Floor(float(in_size.GetValue().x) * 0.01f * float(parent_size.x));

    //     // Reduce size due to parent object's padding
    //     actual_size.x -= parent_padding.x * 2;
    // }

    // if (in_size.GetFlagsY() & UIObjectSize::PERCENT) {
    //     actual_size.y = MathUtil::Floor(float(in_size.GetValue().y) * 0.01f * float(parent_size.y));

    //     // Reduce size due to parent object's padding
    //     actual_size.y -= parent_padding.y * 2;
    // }

    // if (in_size.GetFlagsX() & UIObjectSize::FILL) {
    //     if (!is_inner) {
    //         actual_size.x = MathUtil::Max(parent_size.x - m_position.x - parent_padding.x * 2, 0);
    //     }
    // }

    // if (in_size.GetFlagsY() & UIObjectSize::FILL) {
    //     if (!is_inner) {
    //         actual_size.y = MathUtil::Max(parent_size.y - m_position.y - parent_padding.y * 2, 0);
    //     }
    // }

    // if (in_size.GetAllFlags() & UIObjectSize::AUTO) {
    //     // Calculate "auto" sizing based on children. Must be executed after children have their sizes calculated.
    //     if (phase == UpdateSizePhase::AFTER_CHILDREN) {
    //         Vec2f dynamic_size;

    //         const Vec3f inner_extent = CalculateInnerAABB_Internal().GetExtent();

    //         if ((in_size.GetFlagsX() & UIObjectSize::AUTO) && (in_size.GetFlagsY() & UIObjectSize::AUTO)) {
    //             // If both X and Y are set to auto, use the AABB size (we can't calculate a ratio if both are auto)
    //             dynamic_size = Vec2f { inner_extent.x, inner_extent.y };
    //         } else {
    //             const float inner_width = (in_size.GetFlagsX() & UIObjectSize::AUTO) ? inner_extent.x : float(actual_size.x);
    //             const float inner_height = (in_size.GetFlagsY() & UIObjectSize::AUTO) ? inner_extent.y : float(actual_size.y);

    //             dynamic_size = Vec2f {
    //                 inner_width,
    //                 inner_height
    //             };
    //         }

    //         if (in_size.GetFlagsX() & UIObjectSize::AUTO) {
    //             actual_size.x = MathUtil::Floor(dynamic_size.x);
    //             actual_size.x += self_padding.x * 2;
    //         }

    //         if (in_size.GetFlagsY() & UIObjectSize::AUTO) {
    //             actual_size.y = MathUtil::Floor(dynamic_size.y);
    //             actual_size.y += self_padding.y * 2;
    //         }
    //     } else {
    //         // fix child object's offsets:
    //         // - when resizing, the node just sees objects offset by X where X is some number based on the previous size
    //         //   which is then included in the GetLocalAABB() calculation.
    //         // - now, the parent actual size has its AUTO components set to 0 (or min size), so we update based on that
    //         ForEachChildUIObject([this](UIObject *object)
    //         {
    //             object->UpdatePosition(/* update_children */ false);

    //             return IterationResult::CONTINUE;
    //         }, false);
    //     }
    // }


    // make sure the actual size is at least 0
    actual_size = MathUtil::Max(actual_size, Vec2i { 0, 0 });
}

void UIObject::ComputeOffsetPosition()
{
    HYP_SCOPE;
    
    Vec2f offset_position { 0.0f, 0.0f };

    switch (m_origin_alignment) {
    case UIObjectAlignment::TOP_LEFT:
        // no offset
        break;
    case UIObjectAlignment::TOP_RIGHT:
        offset_position -= Vec2f(float(m_actual_inner_size.x), 0.0f);

        break;
    case UIObjectAlignment::CENTER:
        offset_position -= Vec2f(float(m_actual_inner_size.x) * 0.5f, float(m_actual_inner_size.y) * 0.5f);

        break;
    case UIObjectAlignment::BOTTOM_LEFT:
        offset_position -= Vec2f(0.0f, float(m_actual_inner_size.y));

        break;
    case UIObjectAlignment::BOTTOM_RIGHT:
        offset_position -= Vec2f(float(m_actual_inner_size.x), float(m_actual_inner_size.y));

        break;
    }

    // where to position the object relative to its parent
    if (UIObject *parent_ui_object = GetParentUIObject()) {
        const Vec2f parent_padding(parent_ui_object->GetPadding());
        const Vec2i parent_actual_size(parent_ui_object->GetActualSize());

        switch (m_parent_alignment) {
        case UIObjectAlignment::TOP_LEFT:
            offset_position += parent_padding;

            break;
        case UIObjectAlignment::TOP_RIGHT:
            // // auto layout breaks with this alignment
            // if (!(parent_ui_object->GetSize().GetFlagsX() & UIObjectSize::AUTO)) {
                offset_position += Vec2f(float(parent_actual_size.x) - parent_padding.x, parent_padding.y);
            // }

            break;
        case UIObjectAlignment::CENTER:
            // // auto layout breaks with this alignment
            // if (!(parent_ui_object->GetSize().GetAllFlags() & UIObjectSize::AUTO)) {
                offset_position += Vec2f(float(parent_actual_size.x) * 0.5f, float(parent_actual_size.y) * 0.5f);
            // }

            break;
        case UIObjectAlignment::BOTTOM_LEFT:
            // // auto layout breaks with this alignment
            // if (!(parent_ui_object->GetSize().GetFlagsY() & UIObjectSize::AUTO)) {
                offset_position += Vec2f(parent_padding.x, float(parent_actual_size.y) - parent_padding.y);
            // }

            break;
        case UIObjectAlignment::BOTTOM_RIGHT:
            // // auto layout breaks with this alignment
            // if (!(parent_ui_object->GetSize().GetAllFlags() & UIObjectSize::AUTO)) {
                offset_position += Vec2f(float(parent_actual_size.x) - parent_padding.x, float(parent_actual_size.y) - parent_padding.y);
            // }

            break;
        }
    }

    m_offset_position = offset_position;
}

void UIObject::UpdateMeshData(bool update_children)
{
    HYP_SCOPE;

    if (m_locked_updates & UIObjectUpdateType::UPDATE_MESH_DATA) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_MESH_DATA | (update_children ? UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA : UIObjectUpdateType::NONE));
    
    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            // Do not update children in the next call; ForEachChildUIObject runs for all descendants
            child->UpdateMeshData(true);

            return IterationResult::CONTINUE;
        }, /* deep */ false);
    }

    UpdateMeshData_Internal();
}

void UIObject::UpdateMeshData_Internal()
{
    HYP_SCOPE;

    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity());

    if (!mesh_component) {
        return;
    }

    UIObjectMeshData ui_object_mesh_data { };
    ui_object_mesh_data.size = Vec2u(m_actual_size);

    ui_object_mesh_data.clamped_aabb = Vec4f(
        m_aabb_clamped.min.x,
        m_aabb_clamped.min.y,
        m_aabb_clamped.max.x,
        m_aabb_clamped.max.y
    );

    ui_object_mesh_data.flags = (m_border_radius & 0xFFu)
        | ((uint32(m_border_flags) & 0xFu) << 8u)
        | ((uint32(m_focus_state) & 0xFFu) << 16u);

    mesh_component->user_data.Set(ui_object_mesh_data);

    Matrix4 instance_transform;
    instance_transform[0][0] = m_aabb_clamped.max.x - m_aabb_clamped.min.x;
    instance_transform[1][1] = m_aabb_clamped.max.y - m_aabb_clamped.min.y;
    instance_transform[2][2] = 1.0f;
    instance_transform[0][3] = m_aabb_clamped.min.x;
    instance_transform[1][3] = m_aabb_clamped.min.y;

    Vec4f instance_texcoords = Vec4f { 0.0f, 0.0f, 1.0f, 1.0f };

    Vec4f instance_offsets = Vec4f(GetAbsolutePosition() - m_aabb_clamped.min.GetXY(), 0.0f, 0.0f);

    Vec4f instance_sizes = Vec4f(Vec2f(m_actual_size), m_aabb_clamped.max.GetXY() - m_aabb_clamped.min.GetXY());

    mesh_component->instance_data.num_instances = 1;
    mesh_component->instance_data.SetBufferData(0, &instance_transform, 1);
    mesh_component->instance_data.SetBufferData(1, &instance_texcoords, 1);
    mesh_component->instance_data.SetBufferData(2, &instance_offsets, 1);
    mesh_component->instance_data.SetBufferData(3, &instance_sizes, 1);

    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void UIObject::UpdateMaterial(bool update_children)
{
    HYP_SCOPE;

    if (m_locked_updates & UIObjectUpdateType::UPDATE_MATERIAL) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_MATERIAL | (update_children ? UIObjectUpdateType::UPDATE_CHILDREN_MATERIAL : UIObjectUpdateType::NONE));
    
    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->UpdateMaterial(true);

            return IterationResult::CONTINUE;
        }, /* deep */ false);
    }

    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity());

    if (!mesh_component) {
        return;
    }

    const Handle<Material> &current_material = mesh_component->material;

    MaterialAttributes material_attributes = GetMaterialAttributes();
    Material::ParameterTable material_parameters = GetMaterialParameters();
    Material::TextureSet material_textures = GetMaterialTextures();

    if (!current_material.IsValid() || current_material->GetRenderAttributes() != material_attributes || current_material->GetTextures() != material_textures || current_material->GetParameters() != material_parameters) {
        // need to get a new Material if attributes have changed
        Handle<Material> new_material = CreateMaterial();
        HYP_LOG(UI, Debug, "Creating new material for UI object (static): {} #{}", GetName(), new_material.GetID().Value());

        mesh_component->material = std::move(new_material);
        mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;

        return;
    }

    bool parameters_changed = material_parameters != current_material->GetParameters();
    bool textures_changed = material_textures != current_material->GetTextures();
    
    if (parameters_changed || textures_changed) {
        Handle<Material> new_material;
        
        if (current_material->IsDynamic()) {
            new_material = current_material;
        } else {
            new_material = current_material->Clone();

            HYP_LOG(UI, Debug, "Cloning material for UI object (dynamic): {} #{}", GetName(), new_material.GetID().Value());

            mesh_component->material = new_material;
            mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
        }

        AssertThrow(new_material->IsDynamic());

        if (parameters_changed) {
            new_material->SetParameters(material_parameters);
        }

        if (textures_changed) {
            new_material->SetTextures(material_textures);
        }

        InitObject(new_material);
    }

    SetNeedsRepaintFlag();
}

bool UIObject::HasChildUIObjects() const
{
    HYP_SCOPE;

    return m_child_ui_objects.Any();
}

ScriptComponent *UIObject::GetScriptComponent(bool deep) const
{
    HYP_SCOPE;

    const UIObject *current_ui_object = this;
    RC<UIObject> parent_ui_object;

    while (current_ui_object != nullptr) {
        Node *node = current_ui_object->GetNode().Get();

        if (node != nullptr) {
            Scene *scene = node->GetScene();
            const Handle<Entity> &entity = node->GetEntity();

            if (entity.IsValid() && scene != nullptr) {
                if (ScriptComponent *script_component = scene->GetEntityManager()->TryGetComponent<ScriptComponent>(entity)) {
                    return script_component;
                }
            }
        }

        if (!deep) {
            return nullptr;
        }

        parent_ui_object = current_ui_object->m_spawn_parent.Lock();
        current_ui_object = parent_ui_object.Get();
    }

    return nullptr;
}

void UIObject::SetScriptComponent(ScriptComponent &&script_component)
{  
    HYP_SCOPE;
    
    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    const Handle<Entity> &entity = GetEntity();

    if (!entity.IsValid()) {
        return;
    }

    if (scene->GetEntityManager()->HasComponent<ScriptComponent>(entity)) {
        scene->GetEntityManager()->RemoveComponent<ScriptComponent>(entity);
    }
    
    scene->GetEntityManager()->AddComponent<ScriptComponent>(entity, std::move(script_component));
}

void UIObject::RemoveScriptComponent()
{
    HYP_SCOPE;

    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    const Handle<Entity> &entity = GetEntity();

    if (!entity.IsValid()) {
        return;
    }

    if (!scene->GetEntityManager()->HasComponent<ScriptComponent>(entity)) {
        return;
    }

    scene->GetEntityManager()->RemoveComponent<ScriptComponent>(entity);
}

RC<UIObject> UIObject::GetChildUIObject(int index) const
{
    HYP_SCOPE;

    UIObject *child_object_ptr = nullptr;

    int current_index = 0;

    ForEachChildUIObject([&child_object_ptr, &current_index, index](UIObject *child)
    {
        if (current_index == index) {
            child_object_ptr = child;

            return IterationResult::STOP;
        }

        ++current_index;

        return IterationResult::CONTINUE;
    }, /* deep */ false);

    return child_object_ptr ? child_object_ptr->RefCountedPtrFromThis() : nullptr;
}

void UIObject::SetNodeProxy(NodeProxy node_proxy)
{
    if (m_node_proxy.IsValid() && m_node_proxy.GetEntity().IsValid() && m_node_proxy->GetScene() != nullptr) {
        const Handle<Entity> &entity = m_node_proxy->GetEntity();
        const RC<EntityManager> &entity_manager = m_node_proxy->GetScene()->GetEntityManager();

        if (entity_manager->HasComponent<UIComponent>(entity)) {
            entity_manager->RemoveComponent<UIComponent>(entity);
        }
    }

    m_node_proxy = std::move(node_proxy);

    if (m_node_proxy.IsValid()) {
        if (!m_node_proxy.GetEntity().IsValid()) {
            AssertThrow(m_node_proxy->GetScene() != nullptr);

            m_node_proxy->SetEntity(m_node_proxy->GetScene()->GetEntityManager()->AddEntity());
        }

        m_node_proxy->GetScene()->GetEntityManager()->AddComponent<UIComponent>(m_node_proxy->GetEntity(), UIComponent { this });

        if (!m_affects_parent_size || !m_is_visible) {
            m_node_proxy->SetFlags(m_node_proxy->GetFlags() | NodeFlags::EXCLUDE_FROM_PARENT_AABB);
        }
    }
}

const NodeTag &UIObject::GetNodeTag(WeakName key) const
{
    static const NodeTag empty_tag { };

    if (m_node_proxy.IsValid()) {
        return m_node_proxy->GetTag(key);
    }

    return empty_tag;
}

void UIObject::SetNodeTag(NodeTag &&tag)
{
    if (m_node_proxy.IsValid()) {
        m_node_proxy->AddTag(std::move(tag));
    }
}

bool UIObject::HasNodeTag(WeakName key) const
{
    if (m_node_proxy.IsValid()) {
        return m_node_proxy->HasTag(key);
    }

    return false;
}

bool UIObject::RemoveNodeTag(WeakName key)
{
    if (m_node_proxy.IsValid()) {
        return m_node_proxy->RemoveTag(key);
    }

    return false;
}

void UIObject::CollectObjects(ProcRef<void, UIObject *> proc, bool only_visible) const
{
    HYP_SCOPE;

    Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    if (only_visible) {
        for (auto [entity, ui_component, _] : scene->GetEntityManager()->GetEntitySet<UIComponent, EntityTagComponent<EntityTag::UI_OBJECT_VISIBLE>>().GetScopedView(DataAccessFlags::ACCESS_READ)) {
            if (!ui_component.ui_object) {
                continue;
            }

            proc(ui_component.ui_object);
        }
    } else {
        for (auto [entity, ui_component] : scene->GetEntityManager()->GetEntitySet<UIComponent>().GetScopedView(DataAccessFlags::ACCESS_READ)) {
            if (!ui_component.ui_object) {
                continue;
            }

            proc(ui_component.ui_object);
        }
    }

    // // Visibility affects all child nodes as well, so return from here.
    // if (only_visible && !GetComputedVisibility()) {
    //     return;
    // }

    // proc(const_cast<UIObject *>(this));

    // Array<Pair<Node *, UIObject *>> children;
    // children.Reserve(m_node_proxy->GetChildren().Size());

    // for (const auto &it : m_node_proxy->GetChildren()) {
    //     if (!it.IsValid()) {
    //         continue;
    //     }

    //     UIComponent *ui_component = it->GetEntity().IsValid()
    //         ? scene->GetEntityManager()->TryGetComponent<UIComponent>(it->GetEntity())
    //         : nullptr;


    //     if (!ui_component) {
    //         continue;
    //     }

    //     if (!ui_component->ui_object) {
    //         continue;
    //     }

    //     children.PushBack({
    //         it.Get(),
    //         ui_component->ui_object
    //     });
    // }

    // std::sort(children.Begin(), children.End(), [](const Pair<Node *, UIObject *> &lhs, const Pair<Node *, UIObject *> &rhs)
    // {
    //     return lhs.second->GetDepth() < rhs.second->GetDepth();
    // });

    // for (const Pair<Node *, UIObject *> &it : children) {
    //     it.second->CollectObjects(proc, out_deferred_child_objects, only_visible);
    // }
}

void UIObject::CollectObjects(Array<UIObject *> &out_objects, bool only_visible) const
{
    CollectObjects([&out_objects](UIObject *ui_object)
    {
        out_objects.PushBack(ui_object);
    }, only_visible);
}

Vec2f UIObject::TransformScreenCoordsToRelative(Vec2i coords) const
{
    HYP_SCOPE;
    
    const Vec2i actual_size = GetActualSize();
    const Vec2f absolute_position = GetAbsolutePosition();
    
    return (Vec2f(coords) - absolute_position) / Vec2f(actual_size);
}

Array<UIObject *> UIObject::GetChildUIObjects(bool deep) const
{
    HYP_SCOPE;
    
    Array<UIObject *> child_objects;

    ForEachChildUIObject([&child_objects](UIObject *child)
    {
        child_objects.PushBack(child);

        return IterationResult::CONTINUE;
    }, deep);

    return child_objects;
}

Array<UIObject *> UIObject::FilterChildUIObjects(ProcRef<bool, UIObject *> predicate, bool deep) const
{
    HYP_SCOPE;
    
    Array<UIObject *> child_objects;

    ForEachChildUIObject([&child_objects, &predicate](UIObject *child)
    {
        if (predicate(child)) {
            child_objects.PushBack(child);
        }

        return IterationResult::CONTINUE;
    }, deep);

    return child_objects;
}

template <class Lambda>
void UIObject::ForEachChildUIObject(Lambda &&lambda, bool deep) const
{
    HYP_SCOPE;
    
    if (!deep) {
        // If not deep, iterate using the child UI objects list - more efficient this way
        for (const RC<UIObject> &child : m_child_ui_objects) {
            if (!child) {
                continue;
            }

            const IterationResult iteration_result = lambda(child.Get());

            // stop iterating if stop was set to true
            if (iteration_result == IterationResult::STOP) {
                return;
            }
        }

        return;
    }

    Queue<const UIObject *> queue;
    queue.Push(this);

    while (queue.Any()) {
        const UIObject *parent = queue.Pop();

        for (const RC<UIObject> &child : parent->m_child_ui_objects) {
            const IterationResult iteration_result = lambda(child.Get());

            // stop iterating if stop was set to true
            if (iteration_result == IterationResult::STOP) {
                return;
            }

            queue.Push(child.Get());
        }
    }
}

void UIObject::ForEachChildUIObject_Proc(ProcRef<IterationResult, UIObject *> proc, bool deep) const
{
    ForEachChildUIObject(proc, deep);
}

template <class Lambda>
void UIObject::ForEachParentUIObject(Lambda &&lambda) const
{
    HYP_SCOPE;
    
    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return;
    }

    Node *parent_node = node->GetParent();

    while (parent_node != nullptr) {
        if (parent_node->GetEntity().IsValid()) {
            if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(parent_node->GetEntity())) {
                if (ui_component->ui_object != nullptr) {
                    const IterationResult iteration_result = lambda(ui_component->ui_object);

                    // stop iterating if stop was set to true
                    if (iteration_result == IterationResult::STOP) {
                        return;
                    }
                }
            }
        }

        parent_node = parent_node->GetParent();
    }
}

void UIObject::SetAllChildUIObjectsStage(UIStage *stage)
{
    HYP_SCOPE;
    
    ForEachChildUIObject([this, stage](UIObject *ui_object)
    {
        if (ui_object == this) {
            return IterationResult::CONTINUE;
        }

        ui_object->SetStage_Internal(stage);

        return IterationResult::CONTINUE;
    }, /* deep */ false);
}

void UIObject::SetStage_Internal(UIStage *stage)
{
    HYP_SCOPE;

    m_stage = stage;

    OnFontAtlasUpdate_Internal();
    OnTextSizeUpdate_Internal();

    SetAllChildUIObjectsStage(stage);
}

void UIObject::OnFontAtlasUpdate()
{
    HYP_SCOPE;
    
    // Update font atlas for all children
    ForEachChildUIObject([](UIObject *child)
    {
        child->OnFontAtlasUpdate_Internal();

        return IterationResult::CONTINUE;
    }, /* deep */ true);
}

void UIObject::OnTextSizeUpdate()
{
    HYP_SCOPE;
    
    ForEachChildUIObject([](UIObject *child)
    {
        child->OnTextSizeUpdate_Internal();

        return IterationResult::CONTINUE;
    }, /* deep */ true);
}

void UIObject::OnScrollOffsetUpdate(Vec2f delta)
{
    HYP_SCOPE;
    
    // Update child element's offset positions - they are dependent on parent scroll offset
    ForEachChildUIObject([](UIObject *child)
    {
        child->UpdatePosition(/* update_children */ false);

        return IterationResult::CONTINUE;
    }, /* deep */ false);


    SetDeferredUpdate(UIObjectUpdateType::UPDATE_CLAMPED_SIZE, true);

    OnScrollOffsetUpdate_Internal(delta);
}

void UIObject::SetDataSource(const RC<UIDataSourceBase> &data_source)
{
    HYP_SCOPE;

    m_data_source_on_change_handler.Reset();
    m_data_source_on_element_add_handler.Reset();
    m_data_source_on_element_remove_handler.Reset();
    m_data_source_on_element_update_handler.Reset();

    m_data_source = data_source;

    SetDataSource_Internal(m_data_source.Get());
}

void UIObject::SetDataSource_Internal(UIDataSourceBase *data_source)
{
    HYP_SCOPE;

    if (!data_source) {
        return;
    }
}

bool UIObject::NeedsRepaint() const
{
    return m_needs_repaint.Get(MemoryOrder::ACQUIRE);
}

void UIObject::SetNeedsRepaintFlag(bool needs_repaint)
{ 
    m_needs_repaint.Set(needs_repaint, MemoryOrder::RELEASE);
}

void UIObject::Repaint()
{
    HYP_SCOPE;
    
    if (Repaint_Internal()) {
        SetNeedsRepaintFlag(false);
    }
}

bool UIObject::Repaint_Internal()
{
    return true;
}

RC<UIObject> UIObject::CreateUIObject(const HypClass *hyp_class, Name name, Vec2i position, UIObjectSize size)
{
    HYP_SCOPE;
    
    if (!hyp_class) {
        return nullptr;
    }

    AssertThrowMsg(hyp_class->HasParent(UIObject::Class()), "Cannot spawn instance of class that is not a subclass of UIObject");

    Threads::AssertOnThread(m_owner_thread_id);

    HypData ui_object_hyp_data;
    hyp_class->CreateInstance(ui_object_hyp_data);

    RC<UIObject> ui_object = ui_object_hyp_data.Get<RC<UIObject>>();

    // AssertThrow(IsInit());
    AssertThrow(GetNode().IsValid());

    if (!name.IsValid()) {
        name = CreateNameFromDynamicString(ANSIString("Unnamed_") + hyp_class->GetName().LookupString());
    }

    NodeProxy node_proxy(MakeRefCountedPtr<Node>(name.LookupString()));
    // Set it to ignore parent scale so size of the UI object is not affected by the parent
    node_proxy->SetFlags(node_proxy->GetFlags() | NodeFlags::IGNORE_PARENT_SCALE);

    UIStage *stage = GetStage();

    ui_object->m_spawn_parent = WeakRefCountedPtrFromThis();
    ui_object->m_stage = stage;

    ui_object->SetNodeProxy(node_proxy);
    ui_object->SetName(name);
    ui_object->SetPosition(position);
    ui_object->SetSize(size);

    ui_object->Init();

    return std::move(ui_object);
}

#pragma endregion UIObject

} // namespace hyperion
