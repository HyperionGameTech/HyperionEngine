/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/animation/Bone.hpp>
#include <scene/animation/Skeleton.hpp>

namespace hyperion {

Bone::Bone()
    : Bone(Name::Invalid())
{
}

Bone::Bone(Name name)
    : Node(name, Transform()),
      m_skeleton(nullptr)
{
}

Bone::~Bone() = default;

Vector3 Bone::GetOffsetTranslation() const
{
    return m_localTransform.GetTranslation() - m_bindingTransform.GetTranslation();
}

Quaternion Bone::GetOffsetRotation() const
{
    return m_localTransform.GetRotation() * Quaternion(m_bindingTransform.GetRotation()).Invert();
}

void Bone::SetKeyframe(const Keyframe& keyframe)
{
    m_keyframe = keyframe;

    m_poseTransform = m_keyframe.transform;

    UpdateBoneTransform();
}

void Bone::ClearPose()
{
    m_poseTransform = Transform();

    UpdateBoneTransform();

    for (const Handle<Node>& child : m_childNodes)
    {
        if (!child)
        {
            continue;
        }

        if (!child->IsA<Bone>())
        {
            continue;
        }

        static_cast<Bone&>(*child).ClearPose(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::StoreBindingPose()
{
    m_invBindingTranslation = m_worldBoneTranslation * -1.0f;
    m_invBindingRotation = Quaternion(m_worldBoneRotation).Invert();

    for (const Handle<Node>& child : m_childNodes)
    {
        if (!child)
        {
            continue;
        }

        if (!child->IsA<Bone>())
        {
            continue;
        }

        static_cast<Bone&>(*child).StoreBindingPose(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::SetToBindingPose()
{
    m_localTransform = m_bindingTransform;
    m_poseTransform = m_bindingTransform;

    UpdateBoneTransform();

    for (const Handle<Node>& child : m_childNodes)
    {
        if (!child)
        {
            continue;
        }

        if (!child->IsA<Bone>())
        {
            continue;
        }

        static_cast<Bone&>(*child).SetToBindingPose(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::CalculateBoneTranslation()
{
    m_worldBoneTranslation = m_bindingTransform.GetTranslation();

    if (m_parentNode != nullptr && m_parentNode->IsA<Bone>())
    {
        const Bone* parentBone = static_cast<Bone*>(m_parentNode); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        m_worldBoneTranslation = parentBone->m_worldBoneRotation * m_bindingTransform.GetTranslation();
        m_worldBoneTranslation += parentBone->m_worldBoneTranslation;
    }

    for (const Handle<Node>& child : m_childNodes)
    {
        if (!child)
        {
            continue;
        }

        if (!child->IsA<Bone>())
        {
            continue;
        }

        static_cast<Bone&>(*child).CalculateBoneTranslation(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::CalculateBoneRotation()
{
    m_worldBoneRotation = m_bindingTransform.GetRotation();

    if (m_parentNode != nullptr && m_parentNode->IsA<Bone>())
    {
        Bone* parentBone = static_cast<Bone*>(m_parentNode); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        m_worldBoneRotation = parentBone->m_worldBoneRotation * m_bindingTransform.GetRotation();
    }

    for (const Handle<Node>& child : m_childNodes)
    {
        if (!child)
        {
            continue;
        }

        if (!child->IsA<Bone>())
        {
            continue;
        }

        static_cast<Bone&>(*child).CalculateBoneRotation(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::UpdateBoneTransform()
{
    m_boneMatrix = Matrix4::Translation(m_worldBoneTranslation * -1.0f);
    m_boneMatrix = Matrix4::Rotation(m_worldBoneRotation * m_poseTransform.GetRotation() * GetOffsetRotation() * m_invBindingRotation) * m_boneMatrix;
    m_boneMatrix = Matrix4::Translation(m_worldBoneTranslation + m_poseTransform.GetTranslation() + GetOffsetTranslation()) * m_boneMatrix;

    if (m_parentNode != nullptr)
    {
        if (m_parentNode->IsA<Bone>())
        {
            m_boneMatrix = static_cast<Bone&>(*m_parentNode).GetBoneMatrix() * m_boneMatrix; // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        }
    }

    if (m_skeleton != nullptr)
    {
        m_skeleton->SetNeedsRenderProxyUpdate();
    }
}

void Bone::SetSkeleton(Skeleton* skeleton)
{
    m_skeleton = skeleton;

    for (auto& child : m_childNodes)
    {
        if (!child)
        {
            continue;
        }

        if (!child->IsA<Bone>())
        {
            continue;
        }

        static_cast<Bone&>(*child).SetSkeleton(skeleton); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

} // namespace hyperion