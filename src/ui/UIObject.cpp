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

#include <system/Application.hpp>

#include <input/InputManager.hpp>

#include <core/threading/Threads.hpp>
#include <core/containers/Queue.hpp>

#include <Engine.hpp>

namespace hyperion {

enum UIObjectFlags : uint32
{
    UI_OBJECT_FLAG_NONE                 = 0x0,
    UI_OBJECT_FLAG_BORDER_TOP_LEFT      = 0x1,
    UI_OBJECT_FLAG_BORDER_TOP_RIGHT     = 0x2,
    UI_OBJECT_FLAG_BORDER_BOTTOM_LEFT   = 0x4,
    UI_OBJECT_FLAG_BORDER_BOTTOM_RIGHT  = 0x8
};

struct UIObjectMeshData
{
    uint32 focus_state = UI_OBJECT_FOCUS_STATE_NONE;
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

UIObject::UIObject()
    : m_parent(nullptr),
      m_is_init(false),
      m_origin_alignment(UI_OBJECT_ALIGNMENT_TOP_LEFT),
      m_parent_alignment(UI_OBJECT_ALIGNMENT_TOP_LEFT),
      m_position(0, 0),
      m_size(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT })),
      m_depth(0),
      m_border_radius(5),
      m_border_flags(UI_OBJECT_BORDER_NONE),
      m_focus_state(UI_OBJECT_FOCUS_STATE_NONE),
      m_is_visible(true)
{
}

UIObject::UIObject(UIStage *parent, NodeProxy node_proxy)
    : UIObject()
{
    AssertThrowMsg(parent != nullptr, "Invalid UIStage parent pointer provided to UIObject!");
    AssertThrowMsg(node_proxy.IsValid(), "Invalid NodeProxy provided to UIObject!");
    
    m_parent = parent;
    m_node_proxy = std::move(node_proxy);
}

UIObject::~UIObject()
{
}

void UIObject::Init()
{
    Handle<Mesh> mesh = GetQuadMesh();

    Scene *scene = GetScene();
    AssertThrow(scene != nullptr);

    scene->GetEntityManager()->AddComponent(GetEntity(), MeshComponent {
        mesh,
        GetMaterial()
    });

    scene->GetEntityManager()->AddComponent(GetEntity(), VisibilityStateComponent {
        // VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
    });

    scene->GetEntityManager()->AddComponent(GetEntity(), BoundingBoxComponent {
        mesh->GetAABB()
    });

    struct ScriptedDelegate
    {
        ID<Entity>  entity;
        Scene       *scene;
        String      method_name;

        ScriptedDelegate(UIObject *ui_object, const String &method_name)
            : entity(ui_object->GetEntity()),
              scene(ui_object->GetScene()),
              method_name(method_name)
        {
        }

        bool operator()(const UIMouseEventData &)
        {
            if (!entity.IsValid() || !scene) {
                return false;
            }

            ScriptComponent *script_component = scene->GetEntityManager()->TryGetComponent<ScriptComponent>(entity);

            if (!script_component || !script_component->object) {
                return false;
            }
            
            if (dotnet::Class *class_ptr = script_component->object->GetClass()) {
                if (auto *method_ptr = class_ptr->GetMethod(method_name)) {
                    // Stubbed method, do not call
                    if (method_ptr->HasAttribute("Hyperion.ScriptMethodStub")) {
                        return false;
                    }

                    return script_component->object->InvokeMethod<bool>(method_ptr);
                }
            }

            return false;
        }
    };

    OnMouseHover.Bind(ScriptedDelegate { this, "OnMouseHover" }).Detach();
    OnMouseLeave.Bind(ScriptedDelegate { this, "OnMouseLeave" }).Detach();
    OnGainFocus.Bind(ScriptedDelegate { this, "OnGainFocus" }).Detach();
    OnLoseFocus.Bind(ScriptedDelegate { this, "OnLoseFocus" }).Detach();
    OnMouseDrag.Bind(ScriptedDelegate { this, "OnMouseDrag" }).Detach();
    OnMouseUp.Bind(ScriptedDelegate { this, "OnMouseUp" }).Detach();
    OnMouseDown.Bind(ScriptedDelegate { this, "OnMouseDown" }).Detach();
    OnClick.Bind(ScriptedDelegate { this, "OnClick" }).Detach();

    // set `m_is_init` to true before calling `UpdatePosition` and `UpdateSize` to allow them to run
    m_is_init = true;

    UpdateSize();
    UpdatePosition();
    UpdateMeshData();
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

void UIObject::UpdatePosition()
{
    if (!IsInit()) {
        return;
    }

    const NodeProxy &node = GetNode();

    if (!node) {
        return;
    }

    Vec2f offset_position(m_position);

    switch (m_origin_alignment) {
    case UI_OBJECT_ALIGNMENT_TOP_LEFT:
        // no offset
        break;
    case UI_OBJECT_ALIGNMENT_TOP_RIGHT:
        offset_position -= Vec2f(float(m_actual_size.x), 0.0f);

        break;
    case UI_OBJECT_ALIGNMENT_CENTER:
        offset_position -= Vec2f(float(m_actual_size.x) * 0.5f, float(m_actual_size.y) * 0.5f);

        break;
    case UI_OBJECT_ALIGNMENT_BOTTOM_LEFT:
        offset_position -= Vec2f(0.0f, float(m_actual_size.y));

        break;
    case UI_OBJECT_ALIGNMENT_BOTTOM_RIGHT:
        offset_position -= Vec2f(float(m_actual_size.x), float(m_actual_size.y));

        break;
    }

    // where to position the object relative to its parent
    if (const UIObject *parent_ui_object = GetParentUIObject()) {
        const Vec2f parent_padding(parent_ui_object->GetPadding());
        const Vec2i parent_actual_size(parent_ui_object->GetActualSize());

        switch (m_parent_alignment) {
        case UI_OBJECT_ALIGNMENT_TOP_LEFT:
            offset_position += parent_padding;

            break;
        case UI_OBJECT_ALIGNMENT_TOP_RIGHT:
            offset_position += Vec2f(float(parent_actual_size.x) - parent_padding.x, parent_padding.y);

            break;
        case UI_OBJECT_ALIGNMENT_CENTER:
            offset_position += Vec2f(float(parent_actual_size.x) * 0.5f, float(parent_actual_size.y) * 0.5f);

            break;
        case UI_OBJECT_ALIGNMENT_BOTTOM_LEFT:
            offset_position += Vec2f(parent_padding.x, float(parent_actual_size.y) - parent_padding.y);

            break;
        case UI_OBJECT_ALIGNMENT_BOTTOM_RIGHT:
            offset_position += Vec2f(float(parent_actual_size.x) - parent_padding.x, float(parent_actual_size.y) - parent_padding.y);

            break;
        }
    }

    // float z_value = 0.0f;

    // if (m_depth != 0) {
    //     z_value = float(m_depth);
    // }

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

    node->LockTransform();

    ForEachChildUIObject([](UIObject *child)
    {
        child->UpdatePosition();
    });
}

UIObjectSize UIObject::GetSize() const
{
    return m_size;
}

void UIObject::SetSize(UIObjectSize size)
{
    m_size = size;

    UpdateSize();
    UpdateMeshData();
}

int UIObject::GetMaxWidth() const
{
    return m_actual_max_size.x;
}

void UIObject::SetMaxWidth(int max_width, UIObjectSize::Flags flags)
{
    m_max_size = UIObjectSize({ max_width, flags }, { m_max_size.GetValue().y, m_max_size.GetFlagsY() });

    UpdateSize();
}

int UIObject::GetMaxHeight() const
{
    return m_actual_max_size.y;
}

void UIObject::SetMaxHeight(int max_height, UIObjectSize::Flags flags)
{
    m_max_size = UIObjectSize({ m_max_size.GetValue().x, m_max_size.GetFlagsX() }, { max_height, flags });

    UpdateSize();
}

void UIObject::UpdateSize()
{
    if (!IsInit()) {
        return;
    }

    UpdateActualSizes();

    const NodeProxy &node = GetNode();

    if (!node) {
        return;
    }

    node->UnlockTransform();

    BoundingBox aabb = node->GetLocalAABB();

    // If we have a mesh, set the AABB to the mesh's AABB
    if (!aabb.IsValid() || !aabb.IsFinite()) {
        if (const Handle<Mesh> &mesh = GetMesh()) {
            aabb = mesh->GetAABB();

            SetLocalAABB(aabb);
        }
    }

    if (!aabb.IsFinite() || !aabb.IsValid()) {
        DebugLog(
            LogType::Warn, "AABB is invalid or not finite for UI object: %s\tBounding box: [%f, %f, %f], [%f, %f, %f]\n",
            m_name.LookupString(),
            aabb.min.x, aabb.min.y, aabb.min.z,
            aabb.max.x, aabb.max.y, aabb.max.z
        );

        return;
    }

    const Vec3f local_aabb_extent = aabb.GetExtent();

    node->SetWorldScale(Vec3f {
        float(m_actual_size.x) / MathUtil::Max(local_aabb_extent.x, MathUtil::epsilon_f),
        float(m_actual_size.y) / MathUtil::Max(local_aabb_extent.y, MathUtil::epsilon_f),
        1.0f
    });

    node->LockTransform();

    ForEachChildUIObject([](UIObject *child)
    {
        child->UpdateSize();
    });
}

void UIObject::SetFocusState(UIObjectFocusState focus_state)
{
    m_focus_state = focus_state;

    UpdateMeshData();
}

DrawableLayer UIObject::GetDrawableLayer() const
{
    return m_drawable_layer;
}

void UIObject::SetDrawableLayer(DrawableLayer layer)
{
    if (m_drawable_layer == layer) {
        return;
    }

    m_drawable_layer = layer;

    UpdateMaterial(false);
}

int UIObject::GetDepth() const
{
    if (m_depth != 0) {
        return m_depth;
    }

    if (const NodeProxy &node = GetNode()) {
        return MathUtil::Clamp(int(node->CalculateDepth()), UIStage::min_depth, UIStage::max_depth + 1);
    }

    return 0;
}

void UIObject::SetDepth(int depth)
{
    m_depth = MathUtil::Clamp(depth, UIStage::min_depth, UIStage::max_depth + 1);

    UpdatePosition();
}

void UIObject::Focus()
{
    if (!AcceptsFocus()) {
        return;
    }

    if (GetFocusState() & UI_OBJECT_FOCUS_STATE_FOCUSED) {
        return;
    }

    if (!m_parent) {
        return;
    }

    m_parent->SetFocusedObject(RefCountedPtrFromThis());

    SetFocusState(GetFocusState() | UI_OBJECT_FOCUS_STATE_FOCUSED);
    OnGainFocus(UIMouseEventData { });
}

void UIObject::Blur()
{
    if (!AcceptsFocus()) {
        return;
    }

    if (!(GetFocusState() & UI_OBJECT_FOCUS_STATE_FOCUSED)) {
        return;
    }

    if (!m_parent) {
        return;
    }

    SetFocusState(GetFocusState() & ~UI_OBJECT_FOCUS_STATE_FOCUSED);
    OnLoseFocus(UIMouseEventData { });

    ForEachChildUIObject([](UIObject *child)
    {
        child->Blur();
    });

    if (m_parent->GetFocusedObject() != this) {
        return;
    }

    m_parent->SetFocusedObject(nullptr);
}

void UIObject::SetBorderRadius(uint32 border_radius)
{
    m_border_radius = border_radius;

    UpdateMeshData();
}

void UIObject::SetBorderFlags(uint32 border_flags)
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

bool UIObject::IsVisible() const
{
    return m_is_visible;
}

void UIObject::SetIsVisible(bool is_visible)
{
    m_is_visible = is_visible;
}

bool UIObject::HasFocus(bool include_children) const
{
    if (GetFocusState() & UI_OBJECT_FOCUS_STATE_FOCUSED) {
        return true;
    }

    if (!include_children) {
        return false;
    }

    bool has_focus = false;

    // check if any child has focus
    ForEachChildUIObject([&has_focus](UIObject *child)
    {
        if (child->HasFocus(true)) {
            has_focus = true;
        }
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

    const NodeProxy &node = GetNode();

    if (!node) {
        DebugLog(LogType::Error, "Parent UI object has no attachable node: %s\n", *GetName());

        return;
    }

    if (NodeProxy child_node = ui_object->GetNode()) {
        if (child_node->GetParent() != nullptr && !child_node->Remove()) {
            DebugLog(LogType::Error, "Failed to remove child node '%s' from current parent\n", child_node->GetName().Data(), child_node->GetParent()->GetName().Data());

            return;
        }

        node->AddChild(child_node);
    } else {
        DebugLog(LogType::Error, "Child UI object '%s' has no attachable node\n", *ui_object->GetName());

        return;
    }

    ui_object->UpdateSize();
    ui_object->UpdatePosition();
    ui_object->UpdateMaterial(); // for z-layer
}

bool UIObject::RemoveChildUIObject(UIObject *ui_object)
{
    if (!ui_object) {
        return false;
    }

    Scene *scene = GetScene();

    if (!scene) {
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
                ui_object->UpdateSize();
                ui_object->UpdatePosition();
                ui_object->UpdateMaterial(); // for z-layer

                return true;
            }
        }
    }

    return false;
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

void UIObject::SetLocalAABB(const BoundingBox &aabb)
{
    if (const NodeProxy &node = GetNode()) {
        node->SetLocalAABB(aabb);
    }

    Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    BoundingBoxComponent &bounding_box_component = scene->GetEntityManager()->GetComponent<BoundingBoxComponent>(GetEntity());
    bounding_box_component.local_aabb = aabb;
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
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE,
            .layer              = GetDrawableLayer()
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 0.0f, 0.005f, 0.015f, 0.95f } }
        }/*,
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, g_asset_manager->Load<Texture>("textures/dummy.jpg") }
        }*/
    );
}

const Handle<Mesh> &UIObject::GetMesh() const
{
    Scene *scene = GetScene();

    if (!GetEntity().IsValid() || !scene) {
        return Handle<Mesh>::empty;
    }

    if (const MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity())) {
        return mesh_component->mesh;
    }

    return Handle<Mesh>::empty;
}

const UIObject *UIObject::GetParentUIObject() const
{
    Scene *scene = GetScene();

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
                    // TEMP
                    if (ui_component->ui_object.Is<UIStage>()) {
                        return nullptr;
                    }
                    return ui_component->ui_object.Get();
                }
            }
        }

        parent_node = parent_node->GetParent();
    }

    return nullptr;
}

Scene *UIObject::GetScene() const
{
    if (const NodeProxy &node = GetNode()) {
        return node->GetScene();
    }

    return nullptr;
}

void UIObject::UpdateActualSizes()
{
    if (m_max_size.GetValue().x != 0 || m_max_size.GetValue().y != 0) {
        ComputeActualSize(m_max_size, m_actual_max_size);
    }

    ComputeActualSize(m_size, m_actual_size);

    // clamp actual size to max size (if set for either x or y axis)
    m_actual_size.x = m_actual_max_size.x != 0 ? MathUtil::Min(m_actual_size.x, m_actual_max_size.x) : m_actual_size.x;
    m_actual_size.y = m_actual_max_size.y != 0 ? MathUtil::Min(m_actual_size.y, m_actual_max_size.y) : m_actual_size.y;
}

void UIObject::ComputeActualSize(const UIObjectSize &in_size, Vec2i &out_actual_size)
{
    Vec2i parent_size = { 0, 0 };
    Vec2i parent_padding = { 0, 0 };

    const UIObject *parent_ui_object = GetParentUIObject();

    if (parent_ui_object) {
        parent_size = parent_ui_object->GetActualSize();
        parent_padding = parent_ui_object->GetPadding();
    } else if (m_parent) {
        parent_size = m_parent->GetSurfaceSize();
    } else /*if (Scene *scene = GetScene(); scene->GetCamera().IsValid()) {
        // In the case that this UIObject is actually a UIStage
        parent_size = {
            GetScene()->GetCamera()->GetWidth(),
            GetScene()->GetCamera()->GetHeight()
        };
    } else*/ {
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

    if (in_size.GetAllFlags() & UIObjectSize::GROW) {
        Vec2i dynamic_size;

        if (const NodeProxy &node = GetNode(); node->GetLocalAABB().IsFinite() && node->GetLocalAABB().IsValid()) {
            const Vec3f extent = node->GetLocalAABB().GetExtent();

            Vec2f ratios;
            ratios.x = extent.x / MathUtil::Max(extent.y, MathUtil::epsilon_f);
            ratios.y = extent.y / MathUtil::Max(extent.x, MathUtil::epsilon_f);

            dynamic_size = Vec2i {
                MathUtil::Floor(float(out_actual_size.y) * ratios.x),
                MathUtil::Floor(float(out_actual_size.x) * ratios.y)
            };
        }

        if (in_size.GetFlagsX() & UIObjectSize::GROW) {
            out_actual_size.x = dynamic_size.x;
            out_actual_size.x += m_padding.x * 2;
        }

        if (in_size.GetFlagsY() & UIObjectSize::GROW) {
            out_actual_size.y = dynamic_size.y;
            out_actual_size.y += m_padding.y * 2;
        }
    }

    // make sure the actual size is at least 0
    out_actual_size = MathUtil::Max(out_actual_size, Vec2i { 0, 0 });
}

void UIObject::UpdateMeshData()
{
    Scene *scene = GetScene();

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
        | ((m_border_flags & 0xFu) << 8u);

    mesh_component->user_data.Set(ui_object_mesh_data);
    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void UIObject::UpdateMaterial(bool update_children)
{
    Scene *scene = GetScene();

    if (!scene) {
        return;
    }

    if (update_children) {
        ForEachChildUIObject([](UIObject *child)
        {
            child->UpdateMaterial();
        });
    }

    MeshComponent *mesh_component = scene->GetEntityManager()->TryGetComponent<MeshComponent>(GetEntity());

    if (!mesh_component) {
        return;
    }

    Handle<Material> material = GetMaterial();

    if (mesh_component->material == material) {
        return;
    }

    mesh_component->material = std::move(material);
    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
}

bool UIObject::HasChildUIObjects() const
{
    Scene *scene = GetScene();

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

template <class Lambda>
void UIObject::ForEachChildUIObject(Lambda &&lambda) const
{
    Scene *scene = GetScene();

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
                    lambda(ui_component->ui_object.Get());
                }
            }

            queue.Push(child.Get());
        }
    }
}

#pragma endregion UIObject

} // namespace hyperion
