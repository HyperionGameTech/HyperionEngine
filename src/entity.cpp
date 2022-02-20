#include "entity.h"
#include "util.h"

#include <algorithm>

namespace hyperion {

Entity::Entity(const std::string &name)
    : fbom::FBOMLoadable(fbom::FBOMObjectType("ENTITY")),
      m_name(name),
      m_aabb_affects_parent(true),
      m_flags(UPDATE_TRANSFORM | UPDATE_AABB),
      m_parent(nullptr),
      m_local_translation(Vector3::Zero()),
      m_local_scale(Vector3::One()),
      m_local_rotation(Quaternion::Identity())
{
}

Entity::~Entity()
{
    for (auto it = m_controls.rbegin(); it != m_controls.rend(); ++it) {
        (*it)->OnRemoved();
        (*it)->m_first_run = true;
        (*it)->parent = nullptr;
    }

    m_controls.clear();
    m_children.clear();
}

void Entity::SetGlobalTranslation(const Vector3 &translation)
{
    if (m_parent == nullptr) {
        SetLocalTranslation(translation);

        return;
    }

    m_local_translation = translation - m_parent->GetGlobalTransform().GetTranslation();

    SetTransformUpdateFlag();
    SetAABBUpdateFlag();
}

void Entity::SetGlobalRotation(const Quaternion &rotation)
{
    if (m_parent == nullptr) {
        SetLocalRotation(rotation);

        return;
    }

    Quaternion tmp = m_parent->GetGlobalTransform().GetRotation();
    tmp.Invert();

    m_local_rotation = rotation * tmp;

    SetTransformUpdateFlag();
    SetAABBUpdateFlag();
}

void Entity::SetGlobalScale(const Vector3 &scale)
{
    if (m_parent == nullptr) {
        SetLocalScale(scale);

        return;
    }

    m_local_scale = scale / m_parent->GetGlobalTransform().GetScale();

    SetTransformUpdateFlag();
    SetAABBUpdateFlag();
}

void Entity::UpdateTransform()
{
    if (m_parent != nullptr) {
        m_global_transform.SetTranslation(m_local_translation + m_parent->GetGlobalTransform().GetTranslation());
        m_global_transform.SetScale(m_local_scale * m_parent->GetGlobalTransform().GetScale());
        m_global_transform.SetRotation(m_local_rotation * m_parent->GetGlobalTransform().GetRotation());
    } else {
        m_global_transform = Transform(m_local_translation, m_local_scale, m_local_rotation);
    }
}

void Entity::UpdateAABB()
{
    // clear current aabb
    m_aabb.Clear();
    // retrieve renderable's aabb
    if (m_renderable != nullptr) {
        BoundingBox renderable_aabb = m_renderable->GetAABB();

        if (!renderable_aabb.Empty()) {
            BoundingBox renderable_aabb_transformed;
            // multiply by transform
            std::array<Vector3, 8> corners = renderable_aabb.GetCorners();

            for (Vector3 &corner : corners) {
                corner *= m_global_transform.GetMatrix();

                renderable_aabb_transformed.Extend(corner);
            }

            m_aabb.Extend(renderable_aabb_transformed);
        }
    }

    if (m_aabb_affects_parent && m_parent != nullptr) {
        // multiply parent's bounding box by this one
        m_parent->m_aabb.Extend(m_aabb);
    }
}

float Entity::CalculateCameraDistance(Camera *camera) const
{
    return m_global_transform.GetTranslation().Distance(camera->GetTranslation());
}

void Entity::AddChild(std::shared_ptr<Entity> entity)
{
    AssertThrow(entity != nullptr);

    if (entity->m_flags & PENDING_REMOVAL) {
        std::cout << entity->GetName() << " saved from death\n";
        entity->m_flags &= ~PENDING_REMOVAL;
        auto it = std::find(
            m_children_pending_removal.begin(),
            m_children_pending_removal.end(),
            entity
        );

        if (it != m_children_pending_removal.end()) {
            m_children_pending_removal.erase(it);
            entity->m_flags &= ~PENDING_REMOVAL;
        }
    }

    m_children.push_back(entity);
    entity->m_parent = this;
    entity->SetTransformUpdateFlag();

    if (entity->GetAABBAffectsParent()) {
        SetAABBUpdateFlag();
    }
}

void Entity::RemoveChild(const std::shared_ptr<Entity> &entity)
{
    AssertThrow(entity != nullptr);

    m_children_pending_removal.push_back(entity);
    m_children.erase(std::find(m_children.begin(), m_children.end(), entity));

    entity->m_parent = nullptr;
    entity->SetPendingRemovalFlag();
    entity->SetTransformUpdateFlag();

    if (entity->GetAABBAffectsParent()) {
        SetAABBUpdateFlag();
    }
}

std::shared_ptr<Entity> Entity::GetChild(size_t index) const
{
    return m_children.at(index);
}

std::shared_ptr<Entity> Entity::GetChild(const std::string &name) const
{
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [&](const std::shared_ptr<Entity> &elt)
    {
        return elt->GetName() == name;
    });

    return it != m_children.end() ? *it : nullptr;
}

std::shared_ptr<Entity> Entity::GetChildPendingRemoval(size_t index) const
{
    return m_children_pending_removal.at(index);
}

void Entity::AddControl(std::shared_ptr<EntityControl> control)
{
    // AssertThrow(control->parent == nullptr);

    m_controls.push_back(control);
    control->parent = this;
    control->OnAdded();
}

void Entity::RemoveControl(const std::shared_ptr<EntityControl> &control)
{
    AssertSoft(control != nullptr);

    m_controls.erase(std::find(m_controls.begin(), m_controls.end(), control));
    control->OnRemoved();
    control->parent = nullptr;
}

void Entity::Update(double dt)
{
    if (m_flags & UPDATE_TRANSFORM) {
        UpdateTransform();
        m_flags &= ~UPDATE_TRANSFORM;
    }
    if (m_flags & UPDATE_AABB) {
        UpdateAABB();
        m_flags &= ~UPDATE_AABB;
    }

    UpdateControls(dt);

    for (auto &child : m_children) {
        child->Update(dt);
    }
}

void Entity::UpdateControls(double dt)
{
    for (auto &control : m_controls) {
        if (control->m_first_run) {
            control->OnFirstRun(dt);
            control->m_first_run = false;
        }

        control->tick += dt * 1000;
        if ((control->tick / 1000 * control->tps) >= 1) {
            control->OnUpdate(dt);
            control->tick = 0;
        }
    }

    // for (auto &child : m_children) {
    //     child->UpdateControls(dt);
    // }
}

void Entity::SetTransformUpdateFlag()
{
    m_flags |= UPDATE_TRANSFORM;
    for (auto &child : m_children) {
        child->SetTransformUpdateFlag();
    }
}

void Entity::SetAABBUpdateFlag()
{
    m_flags |= UPDATE_AABB;
    for (auto &child : m_children) {
        child->SetAABBUpdateFlag();
    }
}

void Entity::SetPendingRemovalFlag()
{
    m_flags |= PENDING_REMOVAL;
    for (auto &child : m_children) {
        child->SetPendingRemovalFlag();
    }
}

std::shared_ptr<Loadable> Entity::Clone()
{
    return CloneImpl();
}

std::shared_ptr<Entity> Entity::CloneImpl()
{
    auto new_entity = std::make_shared<Entity>(m_name + "_clone");

    new_entity->m_flags = m_flags;

    new_entity->m_material = m_material;

    // reference copy
    new_entity->m_renderable = m_renderable;

    new_entity->m_local_translation = m_local_translation;
    new_entity->m_local_scale = m_local_scale;
    new_entity->m_local_rotation = m_local_rotation;

    // clone all child entities
    for (auto &child : m_children) {
        new_entity->AddChild(child->CloneImpl());
    }

    return new_entity;
}

} // namespace hyperion
