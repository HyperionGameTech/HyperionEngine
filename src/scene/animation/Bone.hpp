/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Node.hpp>
#include <scene/animation/Keyframe.hpp>

#include <core/containers/String.hpp>

#include <core/reflection/HypObject.hpp>

#include <core/math/Transform.hpp>

namespace hyperion {

class Skeleton;

HYP_CLASS()
class HYP_API Bone : public Node
{
    friend class Skeleton;

    HYP_OBJECT_BODY(Bone);

public:
    Bone();
    Bone(Name name);
    Bone(const Bone& other) = delete;
    Bone& operator=(const Bone& other) = delete;
    virtual ~Bone() override;

    Vec3f GetOffsetTranslation() const;
    Quaternion GetOffsetRotation() const;

    const Keyframe& GetKeyframe() const
    {
        return m_keyframe;
    }

    void SetKeyframe(const Keyframe& keyframe);

    void ClearPose();

    HYP_FORCE_INLINE const Mat4f& GetBoneMatrix() const
    {
        return m_boneMatrix;
    }

    HYP_FORCE_INLINE void SetBindingTransform(const Transform& transform)
    {
        m_bindingTransform = transform;
    }

    HYP_FORCE_INLINE const Transform& GetBindingTransform() const
    {
        return m_bindingTransform;
    }

    void SetToBindingPose();
    void StoreBindingPose();

    void CalculateBoneTranslation();
    void CalculateBoneRotation();

    void UpdateBoneTransform();

private:
    void SetSkeleton(Skeleton* skeleton);

    Skeleton* GetSkeleton() const
    {
        return m_skeleton;
    }

    HYP_FIELD(Transient)
    Skeleton* m_skeleton;

    HYP_FIELD()
    Mat4f m_boneMatrix;

    HYP_FIELD(Transient)
    Keyframe m_keyframe;

    HYP_FIELD()
    Transform m_bindingTransform;

    HYP_FIELD()
    Transform m_poseTransform;

    HYP_FIELD()
    Vec3f m_worldBoneTranslation;

    HYP_FIELD()
    Quaternion m_worldBoneRotation;

    HYP_FIELD()
    Quaternion m_invBindingRotation;
};

} // namespace hyperion
