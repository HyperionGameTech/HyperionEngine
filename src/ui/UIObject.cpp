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
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/UIComponent.hpp>

#include <input/InputManager.hpp>

#include <core/threading/Threads.hpp>

#include <core/containers/Queue.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/system/AppContext.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <util/profiling/ProfileScope.hpp>

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

struct UIObjectMeshData
{
    uint32 width = 0u;
    uint32 height = 0u;
    uint32 clamped_size_diff = 0u;
    uint32 additional_data = 0u;
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

UIObject::UIObject(UIObjectType type)
    : m_type(type),
      m_stage(nullptr),
      m_is_init(false),
      m_origin_alignment(UIObjectAlignment::TOP_LEFT),
      m_parent_alignment(UIObjectAlignment::TOP_LEFT),
      m_position(0, 0),
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
}

UIObject::~UIObject()
{
    HYP_LOG(UI, LogLevel::DEBUG, "Destroying UIObject with name: {}", GetName());

    m_node_proxy.Reset();

    // Remove the entity from the entity manager
    if (ID<Entity> entity = GetEntity()) {
        Scene *scene = GetScene();

        AssertThrow(scene != nullptr);
        AssertThrow(scene->GetEntityManager() != nullptr);

        if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(entity)) {
            ui_component->ui_object = nullptr;
        }

        scene->GetEntityManager()->RemoveEntity(entity);
    }
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
    
    OnInit.Bind(UIScriptDelegate< > { this, "OnInit", /* allow_nested */ false }).Detach();
    OnAttached.Bind(UIScriptDelegate< > { this, "OnAttached", /* allow_nested */ false }).Detach();
    OnRemoved.Bind(UIScriptDelegate< > { this, "OnRemoved", /* allow_nested */ false }).Detach();
    OnMouseDown.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnMouseDown", /* allow_nested */ false }).Detach();
    OnMouseUp.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnMouseUp", /* allow_nested */ false }).Detach();
    OnMouseDrag.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnMouseDrag", /* allow_nested */ false }).Detach();
    OnMouseHover.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnMouseHover", /* allow_nested */ false }).Detach();
    OnMouseLeave.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnMouseLeave", /* allow_nested */ false }).Detach();
    OnMouseMove.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnMouseMove", /* allow_nested */ false }).Detach();
    OnGainFocus.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnGainFocus", /* allow_nested */ false }).Detach();
    OnLoseFocus.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnLoseFocus", /* allow_nested */ false }).Detach();
    OnScroll.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnScroll", /* allow_nested */ false }).Detach();
    OnClick.Bind(UIScriptDelegate< const MouseEvent & > { this, "OnClick", /* allow_nested */ false }).Detach();
    OnKeyDown.Bind(UIScriptDelegate< const KeyboardEvent & > { this, "OnKeyDown", /* allow_nested */ false }).Detach();
    OnKeyUp.Bind(UIScriptDelegate< const KeyboardEvent & > { this, "OnKeyUp", /* allow_nested */ false }).Detach();

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

        return UIObjectIterationResult::CONTINUE;
    }, /* deep */ true);
}

void UIObject::Update_Internal(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    // If the scroll offset has changed, recalculate the position
    bool recalculate_position = false;
    recalculate_position |= m_scroll_offset.Advance(delta * 0.25f);

    if (recalculate_position) {
        m_deferred_updates |= UIObjectUpdateType::UPDATE_POSITION | UIObjectUpdateType::UPDATE_CHILDREN_SIZE;
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

        if (m_deferred_updates & (UIObjectUpdateType::UPDATE_MESH_DATA | UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA)) {
            UpdateMeshData(m_deferred_updates & UIObjectUpdateType::UPDATE_CHILDREN_MESH_DATA);
        }

        if (m_deferred_updates & (UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY | UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY)) {
            UpdateComputedVisibility(m_deferred_updates & UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY);
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
        {
            UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);
            
            UpdateSize();
            UpdatePosition();
        }

        UpdateClampedSize();

        UpdateMeshData();
        UpdateComputedDepth();

        SetDeferredUpdate(UIObjectUpdateType::UPDATE_CHILDREN_COMPUTED_VISIBILITY, true);
    }

    OnAttached();
}

void UIObject::OnRemoved_Internal()
{
    HYP_SCOPE;

    m_stage = nullptr;

    SetAllChildUIObjectsStage(nullptr);

    if (IsInit()) {
        {
            UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);

            UpdateSize();
            UpdatePosition();
        }

        UpdateClampedSize();

        UpdateMeshData();
        UpdateComputedVisibility();
        UpdateComputedDepth();
    }

    OnRemoved();
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

    UpdatePosition();
    UpdateClampedSize();
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

    const Vec2f parent_scroll_offset = Vec2f(GetParentScrollOffset());

    float z_value = float(GetComputedDepth());

    if (Node *parent_node = node->GetParent()) {
        z_value -= parent_node->GetWorldTranslation().z;
    }

    node->SetLocalTranslation(Vec3f {
        float(m_position.x) + m_offset_position.x - parent_scroll_offset.x,
        float(m_position.y) + m_offset_position.y - parent_scroll_offset.y,
        z_value
    });

    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->UpdatePosition(false);

            return UIObjectIterationResult::CONTINUE;
        });
    }

    // node->LockTransform();

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
    
    UIObject *parent = nullptr;

    if ((parent = GetParentUIObject()) && parent->UseAutoSizing()) {
        parent->UpdateSize(/* update_children */ true);

        return;
    }

    UpdateSize_Internal(update_children);

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
                child->SetAffectsParentSize(false);
                deferred_children.PushBack(child);
            } else if (update_children) {
                child->UpdateSize_Internal(/* update_children */ true);
            }

            return UIObjectIterationResult::CONTINUE;
        }, false);
    }

    // auto needs recalculation
    const bool needs_update_after_children = true;//UseAutoSizing();

    // FILL needs to update the size of the children
    // after the parent has updated its size
    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_SIZE);
        for (UIObject *child : deferred_children) {
            child->SetAffectsParentSize(true);
        }
    }

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

        return UIObjectIterationResult::CONTINUE;
    }, /* deep */ false);

    // @NOTE Don't set update_children to true here, as all children have already been updated
    // and thus have UPDATE_CLAMPED_SIZE deferred update set
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_CLAMPED_SIZE, false);
}

void UIObject::UpdateClampedSize(bool update_children)
{
    if (m_locked_updates & UIObjectUpdateType::UPDATE_CLAMPED_SIZE) {
        return;
    }

    m_deferred_updates &= ~(UIObjectUpdateType::UPDATE_CLAMPED_SIZE | (update_children ? UIObjectUpdateType::UPDATE_CHILDREN_CLAMPED_SIZE : UIObjectUpdateType::NONE));

    const Vec2i size = GetActualSize();
    const Vec2f position = GetAbsolutePosition();

    m_aabb_clamped = { Vec3f { float(position.x), float(position.y), 0.0f }, Vec3f { float(position.x) + float(size.x), float(position.y) + float(size.y), 0.0f } };

    if (UIObject *parent = GetParentUIObject()) {
        m_aabb_clamped = m_aabb_clamped.Intersection(parent->m_aabb_clamped);
    }

    m_actual_size_clamped = Vec2i(m_aabb_clamped.GetExtent().GetXY());

    SetDeferredUpdate(UIObjectUpdateType::UPDATE_COMPUTED_VISIBILITY, true);
    SetDeferredUpdate(UIObjectUpdateType::UPDATE_MESH_DATA, false);

    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->UpdateClampedSize(true);

            return UIObjectIterationResult::CONTINUE;
        }, /* deep */ false);
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

    UpdatePosition();
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

            return UIObjectIterationResult::CONTINUE;
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

            return UIObjectIterationResult::CONTINUE;
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

    UpdatePosition();
    UpdateClampedSize();
}

UIObjectAlignment UIObject::GetParentAlignment() const
{
    return m_parent_alignment;
}

void UIObject::SetParentAlignment(UIObjectAlignment alignment)
{
    HYP_SCOPE;

    m_parent_alignment = alignment;

    UpdatePosition();
    UpdateClampedSize();
}

void UIObject::SetAspectRatio(UIObjectAspectRatio aspect_ratio)
{
    HYP_SCOPE;

    m_aspect_ratio = aspect_ratio;

    if (!IsInit()) {
        return;
    }

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);

        UpdateSize();
        UpdatePosition();
    }

    UpdateClampedSize();
}

void UIObject::SetPadding(Vec2i padding)
{
    HYP_SCOPE;

    m_padding = padding;

    if (!IsInit()) {
        return;
    }

    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);

        UpdateSize();
        UpdatePosition();
    }

    UpdateClampedSize();
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
        if (const UIObject *parent = GetParentUIObject()) {
            return parent->GetTextColor();
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
        UIObject *parent = GetParentUIObject();

        if (parent != nullptr) {
            return parent->GetTextSize();
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


    {
        UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);

        UpdateSize();
        UpdatePosition();
    }

    UpdateClampedSize();

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

        {
            UILockedUpdatesScope scope(*this, UIObjectUpdateType::UPDATE_CLAMPED_SIZE);

            UpdateSize();
            UpdatePosition();
        }

        UpdateClampedSize();
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
    const bool has_stage = m_stage != nullptr || InstanceClass() == UIStage::Class();

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

        OnComputedVisibilityChange_Internal();
    }

    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->UpdateComputedVisibility();

            return UIObjectIterationResult::CONTINUE;
        }, false);
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

            return UIObjectIterationResult::STOP;
        }

        return UIObjectIterationResult::CONTINUE;
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
        HYP_LOG(UI, LogLevel::ERR, "Parent UI object has no attachable node: {}", GetName());

        return;
    }

    if (NodeProxy child_node = ui_object->GetNode()) {
        if (child_node->GetParent() != nullptr && !child_node->Remove()) {
            HYP_LOG(UI, LogLevel::ERR, "Failed to remove child node '{}' from current parent {}", child_node->GetName(), child_node->GetParent()->GetName());

            return;
        }

        node->AddChild(child_node);
    } else {
        HYP_LOG(UI, LogLevel::ERR, "Child UI object '{}' has no attachable node", ui_object->GetName());

        return;
    }

    m_child_ui_objects.PushBack(ui_object);
    ui_object->OnAttached_Internal(this);
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

                    auto it = m_child_ui_objects.FindAs(ui_object);

                    if (it != m_child_ui_objects.End()) {
                        m_child_ui_objects.Erase(it);
                        
                        if (UseAutoSizing()) {
                            UpdateSize();
                        }

                        return true;
                    }

                    HYP_LOG(UI, LogLevel::ERR, "Failed to remove UIObject {} from parent: Parent UIObject {} did not have the child object in its children array!", ui_object->GetName(), parent_ui_object->GetName());

                    return false;
                } else {
                    return parent_ui_object->RemoveChildUIObject(ui_object);
                }
            } else {
                HYP_LOG(UI, LogLevel::ERR, "Failed to remove UIObject {} from parent: Parent node ({}) had no UIComponent!", ui_object->GetName(), parent_node->GetName());

                return false;
            }
        }
    }

    HYP_LOG(UI, LogLevel::ERR, "Failed to remove UIObject {} from parent!", ui_object->GetName());

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

        // temp testing
        for (const RC<UIObject> &child_ui_object : m_child_ui_objects) {
            HYP_LOG(UI, LogLevel::DEBUG, "Remove child {} from {} -- {} refs", child_ui_object->GetName(), GetName(), child_ui_object.GetRefCountData_Internal()->UseCount_Strong());
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

            return UIObjectIterationResult::STOP;
        }

        return UIObjectIterationResult::CONTINUE;
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

            return UIObjectIterationResult::STOP;
        }

        return UIObjectIterationResult::CONTINUE;
    }, deep);

    return found_object;
}

const NodeProxy &UIObject::GetNode() const
{
    return m_node_proxy;
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

    // BoundingBox aabb = m_aabb;

    // ForEachChildUIObject([&aabb](UIObject *child)
    // {
    //     // aabb = aabb.Union(child->m_aabb);

    //     const Vec2f absolute_position = child->GetAbsolutePosition();

    //     // aabb = aabb.Union(BoundingBox {
    //     //     Vec3f { float(absolute_position.x), float(absolute_position.x + child->GetActualSize().x), 0.0f },
    //     //     Vec3f { float(absolute_position.y), float(absolute_position.y + child->GetActualSize().y), 0.0f }
    //     // });

    //     aabb = aabb.Union(child->m_aabb);

    //     return UIObjectIterationResult::CONTINUE;
    // }, /* deep */ false);

    // return aabb;

    if (const NodeProxy &node = GetNode()) {
        const BoundingBox node_aabb = node->GetLocalAABB();

        if (node_aabb.IsFinite() && node_aabb.IsValid()) {
            return node_aabb;
        }
    }

    // if (const NodeProxy &node = GetNode()) {
    //     BoundingBox aabb = node->GetEntityAABB();

    //     ForEachChildUIObject([&aabb](UIObject *child)
    //     {
    //         if (!child->AffectsParentSize()) {
    //             return UIObjectIterationResult::CONTINUE;
    //         }

    //         if (const NodeProxy &node = child->GetNode()) {
    //             if (!(node->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB)) {
    //                 aabb = aabb.Union(node->GetEntityAABB() * node->GetLocalTransform());
    //             }
    //         }

    //         return UIObjectIterationResult::CONTINUE;
    //     }, /* deep */ false);

    //     // for (const NodeProxy &child : node->GetChildren()) {
    //     //     if (!child.IsValid()) {
    //     //         continue;
    //     //     }

    //     //     if (!(child->GetFlags() & NodeFlags::EXCLUDE_FROM_PARENT_AABB)) {
    //     //         aabb = aabb.Union(child->GetEntityAABB() * child->GetLocalTransform());
    //     //     }
    //     // }

    //     return aabb;
    // }

    return BoundingBox::Empty();
}

void UIObject::SetAffectsParentSize(bool affects_parent_size)
{
    HYP_SCOPE;

    if (m_affects_parent_size == affects_parent_size) {
        return;
    }

    m_affects_parent_size = affects_parent_size;

    if (const NodeProxy &node = GetNode()) {
        if (affects_parent_size) {
            node->SetFlags(node->GetFlags() & ~NodeFlags::EXCLUDE_FROM_PARENT_AABB);
        } else {
            node->SetFlags(node->GetFlags() | NodeFlags::EXCLUDE_FROM_PARENT_AABB);
        }
    }

    if (UIObject *parent = GetParentUIObject(); parent && parent->IsInit()) {
        parent->UpdateSize();
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

    if (!GetEntity().IsValid() || !scene) {
        return Handle<Material>::empty;
    }

    if (const MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity())) {
        return mesh_component->material;
    }

    return Handle<Material>::empty;
}

const Handle<Mesh> &UIObject::GetMesh() const
{
    HYP_SCOPE;
    
    const Scene *scene = GetScene();

    if (!GetEntity().IsValid() || !scene) {
        return Handle<Mesh>::empty;
    }

    if (const MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity())) {
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

    while (parent_node) {
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

RC<UIObject> UIObject::GetClosestParentUIObjectWithPredicate(const ProcRef<bool, UIObject *> &predicate) const
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
                    if (predicate(ui_component->ui_object)) {
                        return ui_component->ui_object->RefCountedPtrFromThis();
                    }
                }
            }
        }

        parent_node = parent_node->GetParent();
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
    
    Vec2i self_padding = { 0, 0 };
    Vec2i parent_size = { 0, 0 };
    Vec2i parent_padding = { 0, 0 };

    UIObject *parent_ui_object = GetParentUIObject();

    if (is_inner) {
        parent_size = GetActualSize();
        parent_padding = GetPadding();
    } else if (parent_ui_object != nullptr) {
        self_padding = GetPadding();
        parent_size = parent_ui_object->GetActualSize();
        parent_padding = parent_ui_object->GetPadding();
    } else if (m_stage != nullptr) {
        self_padding = GetPadding();
        parent_size = m_stage->GetSurfaceSize();
    } else if (InstanceClass() == UIStage::Class()) {
        actual_size = static_cast<UIStage *>(this)->GetSurfaceSize();
    } else {
        return;
    }

    Optional<Vec3f> inner_extent;

    if (in_size.GetAllFlags() & UIObjectSize::AUTO) {
        if (phase == UpdateSizePhase::AFTER_CHILDREN) {
            inner_extent = CalculateInnerAABB_Internal().GetExtent();
        }
    }

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

            break;
        case UIObjectSize::FILL:
            if (!is_inner) {
                actual_size[component_index] = MathUtil::Max(parent_size[component_index] - m_position[component_index] - parent_padding[component_index] * 2, 0);
            }

            break;
        case UIObjectSize::AUTO:
            if (phase == UpdateSizePhase::AFTER_CHILDREN) {
                actual_size[component_index] = MathUtil::Floor((*inner_extent)[component_index]);
                actual_size[component_index] += self_padding[component_index] * 2;
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

                return UIObjectIterationResult::CONTINUE;
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

    //             return UIObjectIterationResult::CONTINUE;
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
            child->UpdateMeshData();

            return UIObjectIterationResult::CONTINUE;
        }, false);
    }
    
    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity());

    if (!mesh_component) {
        return;
    }

    UIObjectMeshData ui_object_mesh_data { };
    ui_object_mesh_data.width = m_actual_size.x;
    ui_object_mesh_data.height = m_actual_size.y;
    
    Vec2i clamped_size_diff = m_actual_size - m_actual_size_clamped;

    if (clamped_size_diff.x < 0 || clamped_size_diff.y < 0) {
        HYP_LOG(UI, LogLevel::WARNING, "Clamped size diff is negative for UI object: {}, Size: {}, Clamped Size: {}, Clamped Size Diff: {}, valid: {}, World Transform: {}", GetName(), m_actual_size, m_actual_size_clamped, clamped_size_diff, m_aabb_clamped.IsValid(), GetNode().GetWorldTransform().GetScale());
    }

    clamped_size_diff = MathUtil::Max(clamped_size_diff, Vec2i::Zero());
    ui_object_mesh_data.clamped_size_diff = (uint32(clamped_size_diff.x) & 0xFFFFu) | ((uint32(clamped_size_diff.y) & 0xFFFFu) << 16u);

    ui_object_mesh_data.additional_data = (m_border_radius & 0xFFu)
        | ((uint32(m_border_flags) & 0xFu) << 8u)
        | ((uint32(m_focus_state) & 0xFFu) << 16u);

    mesh_component->user_data.Set(ui_object_mesh_data);
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
            child->UpdateMaterial();

            return UIObjectIterationResult::CONTINUE;
        }, false);
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
        HYP_LOG(UI, LogLevel::DEBUG, "Creating new material for UI object (static): {} #{}", GetName(), new_material.GetID().Value());

        mesh_component->material = std::move(new_material);
        mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;

        return;
    }

    bool parameters_changed = material_parameters != current_material->GetParameters();
    bool textures_changed = material_textures != current_material->GetTextures();
    
    if (parameters_changed || textures_changed) {
        Handle<Material> new_material;
        
        if (current_material->IsDynamic()) {
            // // temp
            // AssertThrowMsg(current_material.s_container->GetRefCountStrong(current_material.GetID().ToIndex()) == 2, "ref count : %u",
            //     current_material.s_container->GetRefCountStrong(current_material.GetID().ToIndex()));

            new_material = current_material;
        } else {
            new_material = current_material->Clone();

            // // temp
            // AssertThrowMsg(new_material.s_container->GetRefCountStrong(new_material.GetID().ToIndex()) == 2, "ref count : %u",
            //     new_material.s_container->GetRefCountStrong(new_material.GetID().ToIndex()));

            HYP_LOG(UI, LogLevel::DEBUG, "Cloning material for UI object (dynamic): {} #{}", GetName(), new_material.GetID().Value());

            // release the old material
            // g_safe_deleter->SafeRelease(std::move(mesh_component->material));

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
    
    // const Scene *scene = GetScene();

    // if (!scene) {
    //     return false;
    // }

    // const NodeProxy &node = GetNode();

    // if (!node) {
    //     return false;
    // }

    // for (const NodeProxy &descendent : node->GetDescendents()) {
    //     if (!descendent || !descendent->GetEntity()) {
    //         continue;
    //     }

    //     if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(descendent->GetEntity())) {
    //         if (ui_component->ui_object != nullptr) {
    //             return true;
    //         }
    //     }
    // }

    // return false;
}

ScriptComponent *UIObject::GetScriptComponent(bool deep) const
{
    HYP_SCOPE;

    Node *node = m_node_proxy.Get();

    while (node != nullptr) {
        if (node->GetEntity().IsValid() && node->GetScene() != nullptr) {
            if (ScriptComponent *script_component = node->GetScene()->GetEntityManager()->TryGetComponent<ScriptComponent>(node->GetEntity())) {
                return script_component;
            }
        }

        if (!deep) {
            return nullptr;
        }

        node = node->GetParent();
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

    const NodeProxy &node = GetNode();

    if (!node) {
        return;
    }

    if (scene->GetEntityManager()->HasComponent<ScriptComponent>(GetEntity())) {
        scene->GetEntityManager()->RemoveComponent<ScriptComponent>(GetEntity());
    }
    
    scene->GetEntityManager()->AddComponent<ScriptComponent>(GetEntity(), std::move(script_component));
}

void UIObject::RemoveScriptComponent()
{
    HYP_SCOPE;

    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    if (!scene->GetEntityManager()->HasComponent<ScriptComponent>(GetEntity())) {
        return;
    }

    scene->GetEntityManager()->RemoveComponent<ScriptComponent>(GetEntity());
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

            return UIObjectIterationResult::STOP;
        }

        ++current_index;

        return UIObjectIterationResult::CONTINUE;
    }, false);

    return child_object_ptr ? child_object_ptr->RefCountedPtrFromThis() : nullptr;
}

void UIObject::SetNodeProxy(NodeProxy node_proxy)
{
    if (m_node_proxy.IsValid() && m_node_proxy.GetEntity().IsValid() && m_node_proxy->GetScene() != nullptr) {
        const RC<EntityManager> &entity_manager = m_node_proxy->GetScene()->GetEntityManager();

        if (entity_manager->HasComponent<UIComponent>(m_node_proxy->GetEntity())) {
            entity_manager->RemoveComponent<UIComponent>(m_node_proxy->GetEntity());
        }
    }

    m_node_proxy = std::move(node_proxy);

    if (m_node_proxy.IsValid()) {
        if (!m_node_proxy.GetEntity().IsValid()) {
            AssertThrow(m_node_proxy->GetScene() != nullptr);

            m_node_proxy->SetEntity(m_node_proxy->GetScene()->GetEntityManager()->AddEntity());
        }

        m_node_proxy->GetScene()->GetEntityManager()->AddComponent<UIComponent>(m_node_proxy->GetEntity(), UIComponent { this });

        if (!m_affects_parent_size) {
            m_node_proxy->SetFlags(m_node_proxy->GetFlags() | NodeFlags::EXCLUDE_FROM_PARENT_AABB);
        }
    }
}

const NodeTag &UIObject::GetNodeTag(Name key) const
{
    static const NodeTag empty_tag { };

    if (m_node_proxy.IsValid()) {
        return m_node_proxy->GetTag(key);
    }

    return empty_tag;
}

void UIObject::SetNodeTag(Name key, const NodeTag &tag)
{
    if (m_node_proxy.IsValid()) {
        m_node_proxy->AddTag(key, tag);
    }
}

bool UIObject::HasNodeTag(Name key) const
{
    if (m_node_proxy.IsValid()) {
        return m_node_proxy->HasTag(key);
    }

    return false;
}

bool UIObject::RemoveNodeTag(Name key)
{
    if (m_node_proxy.IsValid()) {
        return m_node_proxy->RemoveTag(key);
    }

    return false;
}

void UIObject::CollectObjects(ProcRef<void, UIObject *> proc, Array<UIObject *> &out_deferred_child_objects, bool only_visible) const
{
    HYP_SCOPE;

    Scene *scene = GetScene();

    if (!scene) {
        return;
    }
    
    // Visibility affects all child nodes as well, so return from here.
    if (only_visible && !GetComputedVisibility()) {
        return;
    }

    proc(const_cast<UIObject *>(this));

    Array<Pair<Node *, UIObject *>> children;
    children.Reserve(m_node_proxy->GetChildren().Size());

    for (const auto &it : m_node_proxy->GetChildren()) {
        if (!it.IsValid()) {
            continue;
        }

        UIComponent *ui_component = it->GetEntity().IsValid()
            ? scene->GetEntityManager()->TryGetComponent<UIComponent>(it->GetEntity())
            : nullptr;


        if (!ui_component) {
            continue;
        }

        if (!ui_component->ui_object) {
            continue;
        }

        children.PushBack({
            it.Get(),
            ui_component->ui_object
        });
    }

    std::sort(children.Begin(), children.End(), [](const Pair<Node *, UIObject *> &lhs, const Pair<Node *, UIObject *> &rhs)
    {
        return lhs.second->GetDepth() < rhs.second->GetDepth();
    });

    for (const Pair<Node *, UIObject *> &it : children) {
        it.second->CollectObjects(proc, out_deferred_child_objects, only_visible);
    }
}

void UIObject::CollectObjects(ProcRef<void, UIObject *> proc, bool only_visible) const
{
    HYP_SCOPE;
    
    Array<UIObject *> deferred_child_objects;
    CollectObjects(proc, deferred_child_objects, only_visible);

    for (UIObject *child_object : deferred_child_objects) {
        child_object->CollectObjects(proc, only_visible);
    }
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

        return UIObjectIterationResult::CONTINUE;
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

        return UIObjectIterationResult::CONTINUE;
    }, deep);

    return child_objects;
}

template <class Lambda>
void UIObject::ForEachChildUIObject(Lambda &&lambda, bool deep) const
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

    if (!deep) {
        // If not deep, iterate using the child UI objects list - more efficient this way
        for (const RC<UIObject> &child : m_child_ui_objects) {
            if (!child) {
                continue;
            }

            const UIObjectIterationResult iteration_result = lambda(child.Get());

            // stop iterating if stop was set to true
            if (iteration_result == UIObjectIterationResult::STOP) {
                return;
            }
        }

        return;
    }

    // breadth-first search for all UIComponents in nested children
    Queue<const Node *> queue;
    queue.Push(node.Get());

    while (queue.Any()) {
        const Node *parent = queue.Pop();

        for (const NodeProxy &child : parent->GetChildren()) {
            if (!child || !child->GetEntity()) {
                continue;
            }

            if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(child->GetEntity())) {
                if (ui_component->ui_object != nullptr) {
                    const UIObjectIterationResult iteration_result = lambda(ui_component->ui_object);

                    // stop iterating if stop was set to true
                    if (iteration_result == UIObjectIterationResult::STOP) {
                        return;
                    }

                    if (!deep) {
                        // Do not continue searching for more children below this node
                        continue;
                    }
                }
            }

            queue.Push(child.Get());
        }
    }
}

void UIObject::ForEachChildUIObject_Proc(ProcRef<UIObjectIterationResult, UIObject *> proc, bool deep) const
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
                    const UIObjectIterationResult iteration_result = lambda(ui_component->ui_object);

                    // stop iterating if stop was set to true
                    if (iteration_result == UIObjectIterationResult::STOP) {
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
            return UIObjectIterationResult::CONTINUE;
        }

        ui_object->SetStage_Internal(stage);

        return UIObjectIterationResult::CONTINUE;
    }, false);
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

        return UIObjectIterationResult::CONTINUE;
    }, true);
}

void UIObject::OnTextSizeUpdate()
{
    HYP_SCOPE;
    
    ForEachChildUIObject([](UIObject *child)
    {
        child->OnTextSizeUpdate_Internal();

        return UIObjectIterationResult::CONTINUE;
    }, true);
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

#pragma endregion UIObject

} // namespace hyperion
