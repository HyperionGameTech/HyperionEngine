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

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>

#include <system/Application.hpp>

#include <input/InputManager.hpp>

#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

// UIObject

UIObject::UIObject(ID<Entity> entity, UIScene *parent)
    : m_entity(entity),
      m_parent(parent),
      m_alignment(UI_OBJECT_ALIGNMENT_TOP_LEFT),
      m_position(0, 0),
      m_size(100, 100)
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

    Handle<Mesh> mesh = MeshBuilder::Quad();
    InitObject(mesh);

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

    scene->GetEntityManager()->AddComponent(m_entity, TransformComponent {
        Transform { }
    });

    UpdatePosition();
    UpdateSize();

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

    OnMouseHover.Bind(ScriptedDelegate { this, "OnMouseHover" });
    OnMouseDrag.Bind(ScriptedDelegate { this, "OnMouseDrag" });
    OnMouseUp.Bind(ScriptedDelegate { this, "OnMouseUp" });
    OnMouseDown.Bind(ScriptedDelegate { this, "OnMouseDown" });
    OnClick.Bind(ScriptedDelegate { this, "OnClick" });
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
    if (!m_entity.IsValid() || !m_parent || !m_parent->GetScene().IsValid()) {
        // Invalid entity or parent
        return;
    }

    Vec2f offset_position(m_position);

    switch (m_alignment) {
    case UI_OBJECT_ALIGNMENT_TOP_LEFT:
        offset_position += Vec2f(float(m_size.x) * 0.5f, float(m_size.y) * 0.5f);

        break;
    case UI_OBJECT_ALIGNMENT_TOP_RIGHT:
        offset_position -= Vec2f(float(m_size.x) * 0.5f, float(m_size.y) * 0.5f);

        break;
    case UI_OBJECT_ALIGNMENT_CENTER:
        // No offset
        break;
    case UI_OBJECT_ALIGNMENT_BOTTOM_LEFT:
        offset_position += Vec2f(float(m_size.x) * 0.5f, -float(m_size.y) * 0.5f);

        break;
    case UI_OBJECT_ALIGNMENT_BOTTOM_RIGHT:
        offset_position -= Vec2f(float(m_size.x) * 0.5f, -float(m_size.y) * 0.5f);

        break;
    }

    if (TransformComponent *transform_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
        transform_component->transform.SetTranslation(Vec3f {
            offset_position.x,
            offset_position.y,
            0.0f
        });
    }
}

Vec2i UIObject::GetSize() const
{
    return m_size;
}

void UIObject::SetSize(Vec2i size)
{
    m_size = size;

    UpdateSize();
}

void UIObject::UpdateSize()
{
    if (!m_entity.IsValid() || !m_parent || !m_parent->GetScene().IsValid()) {
        // Invalid entity or parent
        return;
    }

    if (TransformComponent *transform_component = m_parent->GetScene()->GetEntityManager()->TryGetComponent<TransformComponent>(m_entity)) {
        transform_component->transform.SetScale(Vec3f {
            float(m_size.x),
            float(m_size.y),
            1.0f
        });
    }
}

UIObjectAlignment UIObject::GetAlignment() const
{
    return m_alignment;
}

void UIObject::SetAlignment(UIObjectAlignment alignment)
{
    m_alignment = alignment;

    UpdatePosition();
}

Handle<Material> UIObject::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_mode         = BlendMode::NORMAL,
            .cull_faces         = FaceCullMode::NONE,
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 0.0f, 0.005f, 0.015f, 0.95f } }
        }
    );
}

} // namespace hyperion::v2
