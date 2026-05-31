/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Node.hpp>
#include <Scene/animation/Keyframe.hpp>

#include <Core/containers/String.hpp>

#include <Core/math/Transform.hpp>

namespace Hyperion {

class Skeleton;

HYP_CLASS(AssetBucket = "Bones")
class ENGINE_API Bone : public Node
{
    friend class Skeleton;

    HYP_OBJECT_BODY(Bone);

public:
    Bone();
    explicit Bone(Name name);

    Bone(const Bone& other) = delete;
    Bone& operator=(const Bone& other) = delete;

    virtual ~Bone() override;

    Vec3f GetOffsetTranslation() const;
    Quat4f GetOffsetRotation() const;

    const Keyframe& GetKeyframe() const
    {
        return m_keyframe;
    }

    void SetKeyframe(const Keyframe& keyframe);

    void ClearPose();

    HYP_FORCE_INLINE Name GetBoneName() const
    {
        return m_boneName;
    }

    HYP_FORCE_INLINE void SetBoneName(Name boneName)
    {
        m_boneName = boneName;
    }

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
    Name m_boneName;

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
    Quat4f m_worldBoneRotation;

    HYP_FIELD()
    Quat4f m_invBindingRotation;
};

} // namespace Hyperion
