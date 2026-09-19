/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/Node.hpp>
#include <Scene/Animation/Keyframe.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Math/Transform.hpp>

namespace Hyperion {

class Skeleton;

HYP_CLASS()
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

    // Snaps this bone (and all descendants) to their binding transform; call before StoreBindingPose.
    void SetToBindingPose();

    // Caches the current world transform (set via SetToBindingPose) as the inverse bind matrix used
    // for skinning; call after SetToBindingPose, before any pose is applied.
    void StoreBindingPose();

protected:
    virtual void OnTransformUpdated() override;

private:
    void SetSkeleton(Skeleton* skeleton);

    Skeleton* GetSkeleton() const
    {
        return m_skeleton;
    }

    void UpdateBoneTransform();

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

    HYP_FIELD(Transient)
    Mat4f m_inverseBindMatrix;
};

} // namespace Hyperion
