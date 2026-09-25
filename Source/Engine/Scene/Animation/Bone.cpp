/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Bone.generated.inl>

namespace Hyperion {

// Welcome to the BONE ZONE

Bone::Bone()
    : Bone(Name::Invalid())
{
}

Bone::Bone(Name name)
    : Node(name, Transform()),
      m_boneName(name),
      m_skeleton(nullptr)
{
}

Bone::~Bone() = default;

void Bone::SetKeyframe(const Keyframe& keyframe)
{
    m_keyframe = keyframe;

    SetLocalTransform(m_keyframe.transform);
}

void Bone::ClearPose()
{
    m_keyframe = Keyframe(0.0f, m_bindingTransform);

    SetLocalTransform(m_bindingTransform);

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
    // cache inverse for skinning and recompute the bone matrix immediately,
    // since ClearPose()/SetLocalTransform() may no-op below if nothing has changed yet.
    m_inverseBindMatrix = GetWorldMatrix().Inverse();

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

        static_cast<Bone&>(*child).StoreBindingPose(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
}

void Bone::SetToBindingPose()
{
    m_keyframe = Keyframe(0.0f, m_bindingTransform);

    SetLocalTransform(m_bindingTransform);

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

void Bone::OnTransformUpdated()
{
    UpdateBoneTransform();

    Node::OnTransformUpdated();
}

void Bone::UpdateBoneTransform()
{
    m_boneMatrix = GetWorldMatrix() * m_inverseBindMatrix;

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

} // namespace Hyperion
