#include <ui/UIObject.hpp>
#include <ui/UIScene.hpp>

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

#include <core/lib/Queue.hpp>

#include <system/Application.hpp>

#include <input/InputManager.hpp>

#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

struct UIObjectMeshData
{
    uint32 focus_state = UI_OBJECT_FOCUS_STATE_NONE;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;
};

static_assert(sizeof(UIObjectMeshData) == sizeof(MeshComponentUserData), "UIObjectMeshData size must match sizeof(MeshComponentUserData)");

// UIObject

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

UIObject::UIObject(ID<Entity> entity, UIScene *parent)
    : m_entity(entity),
      m_parent(parent),
      m_is_init(false),
      m_origin_alignment(UI_OBJECT_ALIGNMENT_TOP_LEFT),
      m_parent_alignment(UI_OBJECT_ALIGNMENT_TOP_LEFT),
      m_position(0, 0),
      m_size({ 100, 100 }),
      m_depth(0),
      m_focus_state(UI_OBJECT_FOCUS_STATE_NONE)
{
    AssertThrowMsg(entity.IsValid(), "Invalid Entity provided to UIObject!");
    AssertThrowMsg(parent != nullptr, "Invalid UIScene parent pointer provided to UIObject!");
}

UIObject::~UIObject()
{
}

void UIObject::Init()
{
    AssertThrow(m_entity.IsValid());
    AssertThrow(m_parent != nullptr);

    const Handle<Scene> &scene = m_parent->GetScene();
    AssertThrow(scene.IsValid());

    Handle<Mesh> mesh = GetQuadMesh();

    scene->GetEntityManager()->AddComponent(m_entity, MeshComponent {
        mesh,
        GetMaterial()
    });

    scene->GetEntityManager()->AddComponent(m_entity, VisibilityStateComponent {
        // VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
    });

    scene->GetEntityManager()->AddComponent(m_entity, BoundingBoxComponent {
        mesh->GetAABB()
    });

    struct ScriptedDelegate
    {
        ID<Entity>  entity;
        UIScene     *parent;
        String      method_name;

        ScriptedDelegate(UIObject *ui_object, const String &method_name)
            : entity(ui_object->GetEntity()),
              parent(ui_object->GetParent()),
              method_name(method_name)
        {
        }

        bool operator()(const UIMouseEventData &)
        {
            if (!entity.IsValid() || !parent) {
                return false;
            }

            ScriptComponent *script_component = parent->GetScene()->GetEntityManager()->TryGetComponent<ScriptComponent>(entity);

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
}

void UIObject::UpdatePosition()
{
    if (!m_is_init) {
        return;
    }

    NodeProxy node = GetNode();

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
    if (!m_is_init) {
        return;
    }

    UpdateActualSizes();

    NodeProxy node = GetNode();

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

int UIObject::GetDepth() const
{
    if (m_depth != 0) {
        return m_depth;
    }

    if (NodeProxy node = GetNode()) {
        return MathUtil::Clamp(int(node->CalculateDepth()), UIScene::min_depth, UIScene::max_depth);
    }

    return 0;
}

void UIObject::SetDepth(int depth)
{
    m_depth = MathUtil::Clamp(depth, UIScene::min_depth, UIScene::max_depth);

    UpdatePosition();
    UpdateMaterial(); // Update material to change z-layer
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

void UIObject::AddChildUIObject(UIObject *ui_object)
{
    if (!ui_object) {
        return;
    }

    if (!m_parent || !m_parent->GetScene().IsValid()) {
        return;
    }

    NodeProxy node = GetNode();

    if (!node) {
        DebugLog(LogType::Error, "Parent UI object has no attachable node: %s\n", GetName().LookupString());

        return;
    }

    if (NodeProxy child_node = ui_object->GetNode()) {
        if (!child_node->Remove()) {
            DebugLog(LogType::Error, "Failed to remove child node '%s' from parent: '%s'\n", ui_object->GetName().LookupString(), GetName().LookupString());

            return;
        }

        node->AddChild(child_node);
    } else {
        DebugLog(LogType::Error, "Child UI object '%s' has no attachable node\n", ui_object->GetName().LookupString());

        return;
    }

    ui_object->UpdateSize();
    ui_object->UpdatePosition();
}

bool UIObject::RemoveChildUIObject(UIObject *ui_object)
{
    if (!ui_object) {
        return false;
    }

    if (!m_parent || !m_parent->GetScene().IsValid()) {
        return false;
    }

    NodeProxy node = GetNode();

    if (!node) {
        return false;
    }

    if (NodeProxy child_node = ui_object->GetNode()) {
        if (child_node->IsOrHasParent(node.Get())) {
            bool removed = child_node->Remove();

            if (removed) {
                ui_object->UpdateSize();
                ui_object->UpdatePosition();

                return true;
            }
        }
    }

    return false;
}

NodeProxy UIObject::GetNode() const
{
    if (!m_entity.IsValid() || !m_parent || !m_parent->GetScene().IsValid()) {
        // Invalid entity or parent
        return NodeProxy::empty;
    }

    if (NodeLinkComponent *node_link_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<NodeLinkComponent>(m_entity)) {
        return NodeProxy(node_link_component->node.Lock());
    }

    return NodeProxy::empty;
}

BoundingBox UIObject::GetWorldAABB() const
{
    if (NodeProxy node = GetNode()) {
        return node->GetWorldAABB();
    }

    return BoundingBox::Empty();
}

BoundingBox UIObject::GetLocalAABB() const
{
    if (NodeProxy node = GetNode()) {
        return node->GetLocalAABB();
    }

    return BoundingBox::Empty();
}

void UIObject::SetLocalAABB(const BoundingBox &aabb)
{
    if (NodeProxy node = GetNode()) {
        node->SetLocalAABB(aabb);
    }

    if (!m_parent || !m_parent->GetScene().IsValid()) {
        return;
    }

    BoundingBoxComponent &bounding_box_component = m_parent->GetScene()->GetEntityManager()->GetComponent<BoundingBoxComponent>(m_entity);
    bounding_box_component.local_aabb = aabb;
}

Handle<Material> UIObject::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_mode         = BlendMode::NORMAL,
            .cull_faces         = FaceCullMode::NONE,
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE,
            .z_layer            = GetDepth()
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
    if (!m_entity.IsValid() || !m_parent || !m_parent->GetScene().IsValid()) {
        return Handle<Mesh>::empty;
    }

    if (const MeshComponent *mesh_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(m_entity)) {
        return mesh_component->mesh;
    }

    return Handle<Mesh>::empty;
}

const UIObject *UIObject::GetParentUIObject() const
{
    if (!m_parent || !m_parent->GetScene().IsValid()) {
        return nullptr;
    }

    NodeProxy node = GetNode();

    if (!node) {
        return nullptr;
    }

    Node *parent_node = node->GetParent();

    while (parent_node) {
        if (parent_node->GetEntity().IsValid()) {
            if (UIComponent *ui_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<UIComponent>(parent_node->GetEntity())) {
                if (ui_component->ui_object != nullptr) {
                    return ui_component->ui_object.Get();
                }
            }
        }

        parent_node = parent_node->GetParent();
    }

    return nullptr;
}

void UIObject::UpdateActualSizes()
{
    if (m_max_size.GetValue().x != 0 || m_max_size.GetValue().y != 0) {
        ComputeActualSize(m_max_size, m_actual_max_size);
    } else {
        m_actual_max_size = m_actual_size;
    }

    ComputeActualSize(m_size, m_actual_size, true);

    // clamp actual size to max size (if set for either x or y axis)
    m_actual_size.x = m_actual_max_size.x != 0 ? MathUtil::Min(m_actual_size.x, m_actual_max_size.x) : m_actual_size.x;
    m_actual_size.y = m_actual_max_size.y != 0 ? MathUtil::Min(m_actual_size.y, m_actual_max_size.y) : m_actual_size.y;
}

void UIObject::ComputeActualSize(const UIObjectSize &in_size, Vec2i &out_actual_size, bool clamp)
{
    const UIObject *parent_ui_object = GetParentUIObject();

    // if we have a parent ui object, use that as the parent size - otherwise use the actual surface size.
    const Vec2i parent_size = parent_ui_object != nullptr
        ? parent_ui_object->GetActualSize()
        : m_parent->GetSurfaceSize();

    const Vec2i parent_padding = parent_ui_object != nullptr
        ? parent_ui_object->GetPadding()
        : Vec2i { 0, 0 };

    out_actual_size = in_size.GetValue();

    // percentage based size of parent ui object / surface
    if (in_size.GetFlagsX() & UIObjectSize::PERCENT) {
        out_actual_size.x = MathUtil::Floor(float(out_actual_size.x) * 0.01f * float(parent_size.x));
    }

    if (in_size.GetFlagsY() & UIObjectSize::PERCENT) {
        out_actual_size.y = MathUtil::Floor(float(out_actual_size.y) * 0.01f * float(parent_size.y));
    }

    if (in_size.GetAllFlags() & UIObjectSize::GROW) {
        Vec2i dynamic_size;

        if (NodeProxy node = GetNode(); node->GetLocalAABB().IsFinite() && node->GetLocalAABB().IsValid()) {
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
        }

        if (in_size.GetFlagsY() & UIObjectSize::GROW) {
            out_actual_size.y = dynamic_size.y;
        }
    }

    // Reduce size due to parent object's padding
    out_actual_size.x -= parent_padding.x * 2;
    out_actual_size.y -= parent_padding.y * 2;

    // make sure the actual size is at least 0
    out_actual_size = MathUtil::Max(out_actual_size, Vec2i { 0, 0 });
}

void UIObject::UpdateMeshData()
{
    if (!m_parent || !m_parent->GetScene().IsValid()) {
        return;
    }

    MeshComponent *mesh_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(m_entity);

    if (!mesh_component) {
        return;
    }

    UIObjectMeshData ui_object_mesh_data { };
    ui_object_mesh_data.focus_state = m_focus_state;

    mesh_component->user_data.Set(ui_object_mesh_data);
    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void UIObject::UpdateMaterial()
{
    if (!m_parent || !m_parent->GetScene().IsValid()) {
        return;
    }

    MeshComponent *mesh_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<MeshComponent>(m_entity);

    if (!mesh_component) {
        return;
    }

    mesh_component->material = GetMaterial();
    mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
}

template <class Lambda>
void UIObject::ForEachChildUIObject(Lambda &&lambda)
{
    if (!m_parent || !m_parent->GetScene().IsValid()) {
        return;
    }

    NodeProxy node = GetNode();

    if (!node) {
        return;
    }

    // breadth-first search for all UIComponents in nested children
    Queue<const Node *> queue;
    queue.Push(node.Get());

    while (queue.Any()) {
        const Node *parent = queue.Pop();

        for (const NodeProxy &child : parent->GetChildren()) {
            if (!child || !child.Get()->GetEntity()) {
                continue;
            }

            if (UIComponent *ui_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<UIComponent>(child.GetEntity())) {
                if (ui_component->ui_object != nullptr) {
                    lambda(ui_component->ui_object.Get());
                }
            }

            queue.Push(child.Get());
        }
    }
}

} // namespace hyperion::v2
