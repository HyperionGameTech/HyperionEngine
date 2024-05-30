/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIObject.hpp>
#include <ui/UIStage.hpp>

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

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>

#include <input/InputManager.hpp>

#include <core/threading/Threads.hpp>
#include <core/containers/Queue.hpp>
#include <core/system/AppContext.hpp>
#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

enum class UIObjectIterationResult : uint8
{
    CONTINUE = 0,
    STOP
};

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
    uint32 focus_state = uint32(UIObjectFocusState::NONE);
    uint32 width = 0u;
    uint32 height = 0u;
    uint32 additional_data = 0u;
};

static_assert(sizeof(UIObjectMeshData) == sizeof(MeshComponentUserData), "UIObjectMeshData size must match sizeof(MeshComponentUserData)");

#pragma region UIObject

Handle<Mesh> UIObject::GetQuadMesh()
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
      m_border_radius(5),
      m_border_flags(UIObjectBorderFlags::NONE),
      m_focus_state(UIObjectFocusState::NONE),
      m_is_visible(true),
      m_computed_visibility(true),
      m_accepts_focus(true)
{
}

UIObject::UIObject(UIStage *stage, NodeProxy node_proxy, UIObjectType type)
    : UIObject(type)
{
    AssertThrowMsg(stage != nullptr, "Invalid UIStage pointer provided to UIObject!");
    AssertThrowMsg(node_proxy.IsValid(), "Invalid NodeProxy provided to UIObject!");
    
    m_stage = stage;
    m_node_proxy = std::move(node_proxy);
}

UIObject::~UIObject()
{
}

void UIObject::Init()
{
    const Scene *scene = GetScene();
    AssertThrow(scene != nullptr);

    scene->GetEntityManager()->AddComponent(GetEntity(), MeshComponent { GetQuadMesh(), GetMaterial() });
    scene->GetEntityManager()->AddComponent(GetEntity(), VisibilityStateComponent { });
    scene->GetEntityManager()->AddComponent(GetEntity(), BoundingBoxComponent { });

    struct ScriptedDelegate
    {
        UIObject    *ui_object;
        String      method_name;

        ScriptedDelegate(UIObject *ui_object, const String &method_name)
            : ui_object(ui_object),
              method_name(method_name)
        {
        }

        UIEventHandlerResult operator()(const MouseEvent &)
        {
            AssertThrow(ui_object != nullptr);

            if (!ui_object->GetEntity().IsValid()) {
                HYP_LOG(UI, LogLevel::WARNING, "Entity invalid for UIObject with name: {}", ui_object->GetName());

                return UIEventHandlerResult::ERR;
            }

            if (!ui_object->GetScene()) {
                HYP_LOG(UI, LogLevel::WARNING, "Scene invalid for UIObject with name: {}", ui_object->GetName());

                return UIEventHandlerResult::ERR;
            }

            ScriptComponent *script_component = ui_object->GetScene()->GetEntityManager()->TryGetComponent<ScriptComponent>(ui_object->GetEntity());

            if (!script_component) {
                // DebugLog(LogType::Info, "[%s] No Script component for UIObject with name: %s\n", method_name.Data(), *ui_object->GetName());

                // No script component, do not call.
                return UIEventHandlerResult::OK;
            }

            if (!script_component->object) {
                HYP_LOG(UI, LogLevel::WARNING, "Script component has no object for UIObject with name: {}", ui_object->GetName());

                return UIEventHandlerResult::ERR;
            }
            
            if (dotnet::Class *class_ptr = script_component->object->GetClass()) {
                if (dotnet::ManagedMethod *method_ptr = class_ptr->GetMethod(method_name)) {
                    // Stubbed method, do not call
                    if (method_ptr->HasAttribute("Hyperion.ScriptMethodStub")) {
                        // Stubbed method, do not bother calling it
                        HYP_LOG(UI, LogLevel::INFO, "Stubbed method {} for UI object with name: {}", method_name, ui_object->GetName());

                        return UIEventHandlerResult::OK;
                    }

                    return script_component->object->InvokeMethod<UIEventHandlerResult>(method_ptr);
                }
            }

            HYP_LOG(UI, LogLevel::ERR, "Failed to call method {} for UI object with name: {}", method_name, ui_object->GetName());

            return UIEventHandlerResult::ERR;
        }
    };

    OnMouseHover.Bind(ScriptedDelegate { this, "OnMouseHover" }).Detach();
    OnMouseLeave.Bind(ScriptedDelegate { this, "OnMouseLeave" }).Detach();
    OnMouseDrag.Bind(ScriptedDelegate { this, "OnMouseDrag" }).Detach();
    OnMouseMove.Bind(ScriptedDelegate { this, "OnMouseMove" }).Detach();
    OnMouseUp.Bind(ScriptedDelegate { this, "OnMouseUp" }).Detach();
    OnMouseDown.Bind(ScriptedDelegate { this, "OnMouseDown" }).Detach();
    OnGainFocus.Bind(ScriptedDelegate { this, "OnGainFocus" }).Detach();
    OnLoseFocus.Bind(ScriptedDelegate { this, "OnLoseFocus" }).Detach();
    OnScroll.Bind(ScriptedDelegate { this, "OnScroll" }).Detach();
    OnClick.Bind(ScriptedDelegate { this, "OnClick" }).Detach();

    // set `m_is_init` to true before calling `UpdatePosition` and `UpdateSize` to allow them to run
    m_is_init = true;

    UpdateSize();
    UpdatePosition();
    UpdateMeshData();
}

void UIObject::Update(GameCounter::TickUnit delta)
{
    Update_Internal(delta);

    if (const NodeProxy &node = GetNode()) {
        // temp
        AssertThrow(node->GetScene() == GetScene());
        for (auto &child1 : node->GetDescendents()) {
            AssertThrow(child1->GetScene() == GetScene());
        }
    }

    ForEachChildUIObject([this, delta](const RC<UIObject> &child)
    {
        child->Update_Internal(delta);

        return UIObjectIterationResult::CONTINUE;
    });
}

void UIObject::Update_Internal(GameCounter::TickUnit delta)
{
    // Do nothing
}

void UIObject::OnAttached_Internal(UIObject *parent)
{
    AssertThrow(parent != nullptr);

    m_stage = parent->GetStage();
    SetAllChildUIObjectsStage(parent->GetStage());
}

void UIObject::OnDetached_Internal()
{
    m_stage = nullptr;

    SetAllChildUIObjectsStage(nullptr);
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
    m_position = position;

    UpdatePosition();
    UpdateMeshData();
}

Vec2f UIObject::GetOffsetPosition() const
{
    return m_offset_position;
}

Vec2f UIObject::GetAbsolutePosition() const
{
    if (const NodeProxy &node = GetNode()) {
        const Vec3f world_translation = node->GetWorldTranslation();

        return { world_translation.x, world_translation.y };
    }

    return Vec2f::Zero();
}

void UIObject::UpdatePosition(bool update_children)
{
    if (!IsInit()) {
        return;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return;
    }

    ComputeOffsetPosition();

    const Vec2f offset_position = m_offset_position - Vec2f(GetParentScrollOffset());

    float z_value = 1.0f;

    if (m_depth != 0) {
        z_value = float(m_depth);

        if (Node *parent_node = node->GetParent()) {
            z_value -= parent_node->GetWorldTranslation().z;
        }
    }

    node->UnlockTransform();

    node->SetLocalTranslation(Vec3f {
        offset_position.x,
        offset_position.y,
        z_value
    });

    if (update_children) {
        ForEachChildUIObject([](const RC<UIObject> &child)
        {
            // Do not update children in the next call; ForEachChildUIObject runs for all descendants
            child->UpdatePosition(false);

            return UIObjectIterationResult::CONTINUE;
        });
    }

    node->LockTransform();

    UpdateComputedVisibility();
}

UIObjectSize UIObject::GetSize() const
{
    return m_size;
}

void UIObject::SetSize(UIObjectSize size)
{
    m_size = size;

    UpdateSize();
    UpdatePosition();
}

UIObjectSize UIObject::GetInnerSize() const
{
    return m_inner_size;
}

void UIObject::SetInnerSize(UIObjectSize size)
{
    m_inner_size = size;

    UpdateSize();
    UpdatePosition();
}

UIObjectSize UIObject::GetMaxSize() const
{
    return m_max_size;
}

void UIObject::SetMaxSize(UIObjectSize size)
{
    m_max_size = size;

    UpdateSize();
    UpdatePosition();
}

void UIObject::UpdateSize(bool update_children)
{
    if (!IsInit()) {
        return;
    }

    UpdateActualSizes(UpdateSizePhase::BEFORE_CHILDREN, UIObjectUpdateSizeFlags::DEFAULT);

    const NodeProxy &node = GetNode();

    if (!node.IsValid()) {
        return;
    }

    SetAABB(CalculateAABB());

    if (update_children) {
        ForEachChildUIObject([](const RC<UIObject> &child)
        {
            // Do not update children in the next call; ForEachChildUIObject runs for all descendants
            child->UpdateSize(false);

            return UIObjectIterationResult::CONTINUE;
        });
    }

    // auto needs recalculation
    const bool needs_update_after_children = (m_size.GetAllFlags() & (UIObjectSize::AUTO | UIObjectSize::FILL))
        || (m_inner_size.GetAllFlags() & (UIObjectSize::AUTO | UIObjectSize::FILL))
        || (m_max_size.GetAllFlags() &  (UIObjectSize::AUTO | UIObjectSize::FILL));

    if (needs_update_after_children) {
        UpdateActualSizes(UpdateSizePhase::AFTER_CHILDREN, UIObjectUpdateSizeFlags::DEFAULT);
        SetAABB(CalculateAABB());
    }

    UpdateMeshData();
}

Vec2i UIObject::GetScrollOffset() const
{
    return m_scroll_offset;
}

void UIObject::SetScrollOffset(Vec2i scroll_offset)
{
    // @TODO: Add UpdateScrollOffset() for when size changes

    scroll_offset.x = m_actual_inner_size.x > m_actual_size.x
        ? MathUtil::Clamp(scroll_offset.x, 0, m_actual_inner_size.x - m_actual_size.x)
        : 0;

    scroll_offset.y = m_actual_inner_size.y > m_actual_size.y
        ? MathUtil::Clamp(scroll_offset.y, 0, m_actual_inner_size.y - m_actual_size.y)
        : 0;

    if (scroll_offset == m_scroll_offset) {
        return;
    }

    HYP_LOG(UI, LogLevel::INFO, "Scroll offset changed for UI object {}: {}", GetName(), scroll_offset);
    HYP_LOG(UI, LogLevel::INFO, "Actual size: {}", m_actual_size);
    HYP_LOG(UI, LogLevel::INFO, "Actual inner size: {}", m_actual_inner_size);

    m_scroll_offset = scroll_offset;

    UpdatePosition();
}

void UIObject::SetFocusState(EnumFlags<UIObjectFocusState> focus_state)
{
    m_focus_state = focus_state;

    UpdateMaterial();
    UpdateMeshData();
}

int UIObject::GetComputedDepth() const
{
    if (m_depth != 0) {
        return m_depth;
    }

    if (const NodeProxy &node = GetNode()) {
        return MathUtil::Clamp(int(node->CalculateDepth()), UIStage::min_depth, UIStage::max_depth + 1);
    }

    return 0;
}

int UIObject::GetDepth() const
{
    return m_depth;
}

void UIObject::SetDepth(int depth)
{
    m_depth = MathUtil::Clamp(depth, UIStage::min_depth, UIStage::max_depth + 1);

    UpdatePosition();
}

void UIObject::SetAcceptsFocus(bool accepts_focus)
{
    m_accepts_focus = accepts_focus;

    if (!accepts_focus && HasFocus(true)) {
        Blur();
    }
}

void UIObject::Focus()
{
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
    if (GetFocusState() & UIObjectFocusState::FOCUSED) {
        SetFocusState(GetFocusState() & ~UIObjectFocusState::FOCUSED);
        OnLoseFocus(MouseEvent { });
    }

    if (blur_children) {
        ForEachChildUIObject([](const RC<UIObject> &child)
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
    m_border_radius = border_radius;

    UpdateMeshData();
}

void UIObject::SetBorderFlags(EnumFlags<UIObjectBorderFlags> border_flags)
{
    m_border_flags = border_flags;

    UpdateMeshData();
}

UIObjectAlignment UIObject::GetOriginAlignment() const
{
    return m_origin_alignment;
}

void UIObject::SetOriginAlignment(UIObjectAlignment alignment)
{
    m_origin_alignment = alignment;

    UpdatePosition();
}

UIObjectAlignment UIObject::GetParentAlignment() const
{
    return m_parent_alignment;
}

void UIObject::SetParentAlignment(UIObjectAlignment alignment)
{
    m_parent_alignment = alignment;

    UpdatePosition();
}

void UIObject::SetPadding(Vec2i padding)
{
    m_padding = padding;

    UpdateSize();
    UpdatePosition();
}

Color UIObject::GetBackgroundColor() const
{
    if (uint32(m_background_color) == 0) {
        if (const UIObject *parent = GetParentUIObject()) {
            return parent->GetBackgroundColor();
        }
    }

    return m_background_color;
}

void UIObject::SetBackgroundColor(const Color &background_color)
{
    m_background_color = background_color;

    UpdateMaterial();
}

Color UIObject::GetTextColor() const
{
    if (uint32(m_text_color) == 0) {
        if (const UIObject *parent = GetParentUIObject()) {
            return parent->GetTextColor();
        }
    }

    return m_text_color;
}

void UIObject::SetTextColor(const Color &text_color)
{
    m_text_color = text_color;

    UpdateMaterial();
}

bool UIObject::IsVisible() const
{
    return m_is_visible;
}

void UIObject::SetIsVisible(bool is_visible)
{
    m_is_visible = is_visible;
}

void UIObject::UpdateComputedVisibility()
{
    if (!IsVisible()) {
        m_computed_visibility = false;

        return;
    }

    if (RC<UIObject> parent_ui_object = GetParentUIObject()) {
        const Vec2i parent_size = parent_ui_object->GetActualSize();
        const Vec2f parent_position = parent_ui_object->GetAbsolutePosition();

        const Vec2i self_size = GetActualSize();
        const Vec2f self_position = GetAbsolutePosition();

        const BoundingBox parent_aabb { Vec3f { parent_position.x, parent_position.y, 0.0f }, Vec3f { parent_position.x + float(parent_size.x), parent_position.y + float(parent_size.y), 0.0f } };
        const BoundingBox self_aabb { Vec3f { self_position.x, self_position.y, 0.0f }, Vec3f { self_position.x + float(self_size.x), self_position.y + float(self_size.y), 0.0f } };

        m_computed_visibility = parent_aabb.Intersects(self_aabb);

        return;
    }

    m_computed_visibility = true;

    return;
}

bool UIObject::HasFocus(bool include_children) const
{
    if (GetFocusState() & UIObjectFocusState::FOCUSED) {
        return true;
    }

    if (!include_children) {
        return false;
    }

    bool has_focus = false;

    // check if any child has focus
    ForEachChildUIObject([&has_focus](const RC<UIObject> &child)
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
    if (!other) {
        return false;
    }

    const NodeProxy &this_node = GetNode();
    const NodeProxy &other_node = other->GetNode();

    if (!this_node.IsValid() || !other_node.IsValid()) {
        return false;
    }

    return this_node->IsOrHasParent(other_node.Get());
}

void UIObject::AddChildUIObject(UIObject *ui_object)
{
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

    ui_object->OnAttached_Internal(this);

    ui_object->UpdateSize();
    ui_object->UpdatePosition();
}

bool UIObject::RemoveChildUIObject(UIObject *ui_object)
{
    if (!ui_object) {
        return false;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return false;
    }

    if (NodeProxy child_node = ui_object->GetNode()) {
        if (child_node->IsOrHasParent(node.Get())) {
            bool removed = child_node->Remove();

            if (removed) {
                ui_object->OnDetached_Internal();

                ui_object->UpdateSize();
                ui_object->UpdatePosition();

                return true;
            }
        }
    }

    return false;
}

RC<UIObject> UIObject::FindChildUIObject(Name name) const
{
    RC<UIObject> found_object;

    ForEachChildUIObject([name, &found_object](const RC<UIObject> &child)
    {
        if (child->GetName() == name) {
            found_object = child;

            return UIObjectIterationResult::STOP;
        }

        return UIObjectIterationResult::CONTINUE;
    });

    return found_object;
}

RC<UIObject> UIObject::FindChildUIObject(const Proc<bool, const RC<UIObject> &> &predicate) const
{
    RC<UIObject> found_object;

    ForEachChildUIObject([&found_object, &predicate](const RC<UIObject> &child)
    {
        if (predicate(child)) {
            found_object = child;

            return UIObjectIterationResult::STOP;
        }

        return UIObjectIterationResult::CONTINUE;
    });

    return found_object;
}

const NodeProxy &UIObject::GetNode() const
{
    return m_node_proxy;
}

BoundingBox UIObject::GetWorldAABB() const
{
    if (const NodeProxy &node = GetNode()) {
        return node->GetWorldAABB();
    }

    return BoundingBox::Empty();
}

BoundingBox UIObject::GetLocalAABB() const
{
    if (const NodeProxy &node = GetNode()) {
        return node->GetLocalAABB();
    }

    return BoundingBox::Empty();
}

void UIObject::SetAABB(const BoundingBox &aabb)
{
    if (const NodeProxy &node = GetNode()) {
        const bool transform_locked = node->IsTransformLocked();

        if (transform_locked) {
            node->UnlockTransform();
        }

        node->SetEntityAABB(aabb);

        if (transform_locked) {
            node->LockTransform();
        }
    }

    // UpdateSize();
    // UpdatePosition();

    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    BoundingBoxComponent &bounding_box_component = scene->GetEntityManager()->GetComponent<BoundingBoxComponent>(GetEntity());
    bounding_box_component.local_aabb = aabb;

    UpdateComputedVisibility();
}

BoundingBox UIObject::CalculateAABB() const
{
    return { Vec3f::Zero(), Vec3f(float(m_actual_size.x), float(m_actual_size.y), 0.0f) };
}

BoundingBox UIObject::CalculateWorldAABBExcludingChildren() const
{
    if (const NodeProxy &node = GetNode()) {
        Transform transform = node->GetWorldTransform();
        transform.SetScale(Vec3f::One());
        return node->GetEntityAABB() * transform;
    }

    return BoundingBox::Empty();
}

Handle<Material> UIObject::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributeFlags::NONE
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f(GetBackgroundColor()) }
        }
    );
}

const Handle<Mesh> &UIObject::GetMesh() const
{
    const Scene *scene = GetScene();

    if (!GetEntity().IsValid() || !scene) {
        return Handle<Mesh>::empty;
    }

    if (const MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity())) {
        return mesh_component->mesh;
    }

    return Handle<Mesh>::empty;
}

RC<UIObject> UIObject::GetParentUIObject() const
{
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
                AssertThrow(ui_component->ui_object != nullptr);

                return ui_component->ui_object;
            }
        }

        parent_node = parent_node->GetParent();
    }

    return nullptr;
}

Vec2i UIObject::GetParentScrollOffset() const
{
    if (RC<UIObject> parent_ui_object = GetParentUIObject()) {
        return parent_ui_object->GetScrollOffset();
    }

    return Vec2i::Zero();
}

Scene *UIObject::GetScene() const
{
    if (const NodeProxy &node = GetNode()) {
        return node->GetScene();
    }

    return nullptr;
}

void UIObject::UpdateActualSizes(UpdateSizePhase phase, EnumFlags<UIObjectUpdateSizeFlags> flags)
{
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

    if (flags & UIObjectUpdateSizeFlags::CLAMP_OUTER_SIZE) {
        // clamp actual size to max size (if set for either x or y axis)
        m_actual_size.x = m_actual_max_size.x != 0 ? MathUtil::Min(m_actual_size.x, m_actual_max_size.x) : m_actual_size.x;
        m_actual_size.y = m_actual_max_size.y != 0 ? MathUtil::Min(m_actual_size.y, m_actual_max_size.y) : m_actual_size.y;
    }
}

void UIObject::ComputeActualSize(const UIObjectSize &in_size, Vec2i &out_actual_size, UpdateSizePhase phase, bool is_inner)
{
    Vec2i parent_size = { 0, 0 };
    Vec2i parent_padding = { 0, 0 };
    Vec2i self_padding = { 0, 0 };

    const RC<UIObject> parent_ui_object = GetParentUIObject();

    if (is_inner) {
        parent_size = GetActualSize();
        parent_padding = GetPadding();
    } else if (parent_ui_object) {
        parent_size = parent_ui_object->GetActualSize();
        parent_padding = parent_ui_object->GetPadding();
        self_padding = GetPadding();
    } else if (m_stage != nullptr) {
        parent_size = m_stage->GetSurfaceSize();
        self_padding = GetPadding();
    } else {
        return;
    }

    out_actual_size = in_size.GetValue();

    // percentage based size of parent ui object / surface
    if (in_size.GetFlagsX() & UIObjectSize::PERCENT) {
        out_actual_size.x = MathUtil::Floor(float(out_actual_size.x) * 0.01f * float(parent_size.x));

        // Reduce size due to parent object's padding
        out_actual_size.x -= parent_padding.x * 2;
    }

    if (in_size.GetFlagsY() & UIObjectSize::PERCENT) {
        out_actual_size.y = MathUtil::Floor(float(out_actual_size.y) * 0.01f * float(parent_size.y));

        // Reduce size due to parent object's padding
        out_actual_size.y -= parent_padding.y * 2;
    }

    if (phase == UpdateSizePhase::AFTER_CHILDREN && (in_size.GetAllFlags() & UIObjectSize::FILL)) {
        // @TODO
    }

    // Calculate "auto" sizing based on children. Must be executed after children have their sizes calculated.
    if (phase == UpdateSizePhase::AFTER_CHILDREN && (in_size.GetAllFlags() & UIObjectSize::AUTO)) {
        Vec2i dynamic_size;

        BoundingBox node_aabb = BoundingBox::Empty();

        if (const NodeProxy &node = GetNode()) {
            node_aabb = node->GetLocalAABB();
        }

        if (node_aabb.IsFinite() && node_aabb.IsValid()) {
            const Vec3f extent = node_aabb.GetExtent();

            Vec2f ratios;
            ratios.x = extent.x / MathUtil::Max(extent.y, MathUtil::epsilon_f);
            ratios.y = extent.y / MathUtil::Max(extent.x, MathUtil::epsilon_f);

            dynamic_size = Vec2i {
                MathUtil::Floor(float(out_actual_size.y) * ratios.x),
                MathUtil::Floor(float(out_actual_size.x) * ratios.y)
            };
        }

        if (in_size.GetFlagsX() & UIObjectSize::AUTO) {
            out_actual_size.x = dynamic_size.x;
            out_actual_size.x += self_padding.x * 2;
        }

        if (in_size.GetFlagsY() & UIObjectSize::AUTO) {
            out_actual_size.y = dynamic_size.y;
            out_actual_size.y += self_padding.y * 2;
        }
    }

    // make sure the actual size is at least 0
    out_actual_size = MathUtil::Max(out_actual_size, Vec2i { 0, 0 });

    if (is_inner) {
        HYP_LOG(UI, LogLevel::DEBUG, "inner for {}, in size: {}, {}\tout size: {}, {}", GetName(), in_size.GetValue().x, in_size.GetValue().y, out_actual_size.x, out_actual_size.y);
    }
}

void UIObject::ComputeOffsetPosition()
{
    Vec2f offset_position(m_position);

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
    if (const RC<UIObject> parent_ui_object = GetParentUIObject()) {
        const Vec2f parent_padding(parent_ui_object->GetPadding());
        const Vec2i parent_actual_size(parent_ui_object->GetActualSize());

        switch (m_parent_alignment) {
        case UIObjectAlignment::TOP_LEFT:
            offset_position += parent_padding;

            break;
        case UIObjectAlignment::TOP_RIGHT:
            offset_position += Vec2f(float(parent_actual_size.x) - parent_padding.x, parent_padding.y);

            break;
        case UIObjectAlignment::CENTER:
            offset_position += Vec2f(float(parent_actual_size.x) * 0.5f, float(parent_actual_size.y) * 0.5f);

            break;
        case UIObjectAlignment::BOTTOM_LEFT:
            offset_position += Vec2f(parent_padding.x, float(parent_actual_size.y) - parent_padding.y);

            break;
        case UIObjectAlignment::BOTTOM_RIGHT:
            offset_position += Vec2f(float(parent_actual_size.x) - parent_padding.x, float(parent_actual_size.y) - parent_padding.y);

            break;
        }
    }

    m_offset_position = offset_position;
}

void UIObject::UpdateMeshData()
{
    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity());

    if (!mesh_component) {
        return;
    }

    UIObjectMeshData ui_object_mesh_data { };
    ui_object_mesh_data.focus_state = m_focus_state;
    ui_object_mesh_data.width = m_actual_size.x;
    ui_object_mesh_data.height = m_actual_size.y;
    ui_object_mesh_data.additional_data = (m_border_radius & 0xFFu)
        | ((uint32(m_border_flags) & 0xFu) << 8u);

    mesh_component->user_data.Set(ui_object_mesh_data);
    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void UIObject::UpdateMaterial(bool update_children)
{
    if (update_children) {
        ForEachChildUIObject([](const RC<UIObject> &child)
        {
            // Do not update children in the next call; ForEachChildUIObject runs for all descendants
            child->UpdateMaterial(false);

            return UIObjectIterationResult::CONTINUE;
        });
    }

    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity());

    if (!mesh_component) {
        return;
    }

    Handle<Material> material = GetMaterial();

    if (mesh_component->material == material) {
        return;
    }

    g_safe_deleter->SafeRelease(std::move(mesh_component->material));

    mesh_component->material = std::move(material);
    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
}

bool UIObject::HasChildUIObjects() const
{
    const Scene *scene = GetScene();

    if (!scene) {
        return false;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return false;
    }

    for (const NodeProxy &descendent : node->GetDescendents()) {
        if (!descendent || !descendent->GetEntity()) {
            continue;
        }

        if (UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(descendent->GetEntity())) {
            if (ui_component->ui_object != nullptr) {
                return true;
            }
        }
    }

    return false;
}

void UIObject::SetNodeProxy(NodeProxy node_proxy)
{
    m_node_proxy = std::move(node_proxy);
}

void UIObject::CollectObjects(const Proc<void, const RC<UIObject> &> &proc, Array<RC<UIObject>> &out_deferred_child_objects) const
{
    AssertThrow(proc.IsValid());

    if (!m_node_proxy.IsValid()) {
        return;
    }
    
    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    const ID<Entity> entity = m_node_proxy->GetEntity();

    if (entity.IsValid()) {
        UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(entity);

        if (ui_component && ui_component->ui_object) {
            // Visibility affects all child nodes as well, so return from here.
            if (!ui_component->ui_object->GetComputedVisibility()) {
                return;
            }

            AssertThrow(ui_component->ui_object->GetNode().IsValid());

            proc(ui_component->ui_object);
        }
    }

    Array<Pair<NodeProxy, RC<UIObject>>> children;
    children.Reserve(m_node_proxy->GetChildren().Size());

    for (const auto &it : m_node_proxy->GetChildren()) {
        if (!it.IsValid()) {
            continue;
        }

        UIComponent *ui_component = it->GetEntity().IsValid()
            ? scene->GetEntityManager()->TryGetComponent<UIComponent>(it->GetEntity())
            : nullptr;

        if (!ui_component || !ui_component->ui_object) {
            continue;
        }

        children.PushBack({
            it,
            ui_component->ui_object
        });
    }

    if (IsContainer()) {
        for (const Pair<NodeProxy, RC<UIObject>> &it : children) {
            it.second->CollectObjects(proc, out_deferred_child_objects);
        }
    } else {
        for (Pair<NodeProxy, RC<UIObject>> &it : children) {
            out_deferred_child_objects.PushBack(std::move(it.second));
        }
    }
}

void UIObject::CollectObjects(const Proc<void, const RC<UIObject> &> &proc) const
{
    Array<RC<UIObject>> deferred_child_objects;
    CollectObjects(proc, deferred_child_objects);

    for (RC<UIObject> &child_object : deferred_child_objects) {
        child_object->CollectObjects(proc);
    }
}

void UIObject::CollectObjects(Array<RC<UIObject>> &out_objects) const
{
    CollectObjects([&out_objects](const RC<UIObject> &ui_object)
    {
        out_objects.PushBack(ui_object);
    });
}

Vec2f UIObject::TransformScreenCoordsToRelative(Vec2i coords) const
{
    const Vec2i actual_size = GetActualSize();
    const Vec2f absolute_position = GetAbsolutePosition();
    
    return (Vec2f(coords) - absolute_position) / Vec2f(actual_size);
}

template <class Lambda>
void UIObject::ForEachChildUIObject(Lambda &&lambda) const
{
    const Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
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
                }
            }

            queue.Push(child.Get());
        }
    }
}

void UIObject::SetAllChildUIObjectsStage(UIStage *stage)
{
    ForEachChildUIObject([stage](const RC<UIObject> &ui_object)
    {
        ui_object->m_stage = stage;

        return UIObjectIterationResult::CONTINUE;
    });
}

#pragma endregion UIObject

} // namespace hyperion
