/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/animation/Bone.hpp>
#include <scene/animation/Skeleton.hpp>

namespace hyperion {

Bone::Bone(const String &name)
    : Node(Type::BONE, name, Handle<Entity>::empty, Transform()),
      m_skeleton(nullptr)
{
}

Bone::~Bone() = default;

Vector3 Bone::GetOffsetTranslation() const
{
    return m_local_transform.GetTranslation() - m_binding_transform.GetTranslation();
}

Quaternion Bone::GetOffsetRotation() const
{
    return m_local_transform.GetRotation() * Quaternion(m_binding_transform.GetRotation()).Invert();
}

void Bone::SetKeyframe(const Keyframe &keyframe)
{
    m_keyframe = keyframe;

    m_pose_transform = m_keyframe.GetTransform();

    UpdateBoneTransform();
}

void Bone::ClearPose()
{
    m_pose_transform = Transform();

    UpdateBoneTransform();

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        if (child.Get()->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.Get())->ClearPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::StoreBindingPose()
{
    m_inv_binding_translation = m_world_bone_translation * -1.0f;
    m_inv_binding_rotation = Quaternion(m_world_bone_rotation).Invert();

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        if (child.Get()->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.Get())->StoreBindingPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::SetToBindingPose()
{
    m_local_transform = m_binding_transform;
    m_pose_transform = m_binding_transform;

    UpdateBoneTransform();

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        if (child.Get()->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.Get())->SetToBindingPose();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::CalculateBoneTranslation()
{
    m_world_bone_translation = m_binding_transform.GetTranslation();

    if (m_parent_node != nullptr && m_parent_node->GetType() == Type::BONE) {
        const auto *parent_bone = static_cast<Bone *>(m_parent_node);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        m_world_bone_translation = parent_bone->m_world_bone_rotation * m_binding_transform.GetTranslation();
        m_world_bone_translation += parent_bone->m_world_bone_translation;
    }

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        if (child.Get()->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.Get())->CalculateBoneTranslation();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::CalculateBoneRotation()
{
    m_world_bone_rotation = m_binding_transform.GetRotation();

    if (m_parent_node != nullptr && m_parent_node->GetType() == Type::BONE) {
        const auto *parent_bone = static_cast<Bone *>(m_parent_node);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        m_world_bone_rotation = parent_bone->m_world_bone_rotation * m_binding_transform.GetRotation();
    }

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        if (child.Get()->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.Get())->CalculateBoneRotation();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::UpdateBoneTransform()
{
    m_bone_matrix = Matrix4::Translation(m_world_bone_translation * -1.0f);
    m_bone_matrix = Matrix4::Rotation(m_world_bone_rotation * m_pose_transform.GetRotation() * GetOffsetRotation() * m_inv_binding_rotation) * m_bone_matrix;
    m_bone_matrix = Matrix4::Translation(m_world_bone_translation + m_pose_transform.GetTranslation() + GetOffsetTranslation()) * m_bone_matrix;

    if (m_parent_node != nullptr) {
        if (m_parent_node->GetType() == Type::BONE) {
            m_bone_matrix = static_cast<Bone *>(m_parent_node)->GetBoneMatrix() * m_bone_matrix;  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }
    }

    if (m_skeleton != nullptr) {
        m_skeleton->SetMutationState(DataMutationState::DIRTY);
    }
}

void Bone::SetSkeleton(Skeleton *skeleton)
{
    m_skeleton = skeleton;
    
    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }
        
        if (child.Get()->GetType() != Type::BONE) {
            continue;
        }

        static_cast<Bone *>(child.Get())->SetSkeleton(skeleton);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

} // namespace hyperion