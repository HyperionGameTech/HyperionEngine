/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SKELETON_HPP
#define HYPERION_SKELETON_HPP

#include <core/Base.hpp>
#include <core/utilities/DataMutationState.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>

#include <scene/animation/Animation.hpp>
#include <scene/NodeProxy.hpp>

#include <core/system/Debug.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

class Engine;
class Bone;

struct SkeletonBoneData
{
    using BoneMatricesPtr = RC<FixedArray<Matrix4, 128>>;

    BoneMatricesPtr matrices;

    SkeletonBoneData()
        : matrices(BoneMatricesPtr::Construct())
    {
    }

    SkeletonBoneData(const SkeletonBoneData &other)                 = default;
    SkeletonBoneData &operator=(const SkeletonBoneData &other)      = default;
    SkeletonBoneData(SkeletonBoneData &&other) noexcept             = default;
    SkeletonBoneData &operator=(SkeletonBoneData &&other) noexcept  = default;
    ~SkeletonBoneData()                                             = default;

    void SetMatrix(uint32 index, const Matrix4 &matrix)
    {
        AssertThrow(matrices && index < matrices->Size());

        (*matrices)[index] = matrix;
    }

    const Matrix4 &GetMatrix(uint32 index)
    {
        AssertThrow(matrices && index < matrices->Size());

        return (*matrices)[index];
    }
};

HYP_CLASS()
class HYP_API Skeleton : public BasicObject<Skeleton>
{
    HYP_OBJECT_BODY(Skeleton);

public:
    Skeleton();
    Skeleton(const RC<Bone> &root_bone);
    Skeleton(const Skeleton &other)             = delete;
    Skeleton &operator=(const Skeleton &other)  = delete;
    ~Skeleton();

    /*! \brief Get the mutation state of this skeleton.
     *  \returns The mutation state of this skeleton. */
    DataMutationState GetMutationState() const
        { return m_mutation_state; }

    /*! \brief Set the mutation state of this skeleton. To be called by the Bone class.
     *  \note This is not intended to be called by the user. */
    void SetMutationState(DataMutationState state)
        { m_mutation_state = state; }

    /*! \brief Look up a bone with the given name/tag. If no root bone was set,
     *  or the bone could not be found, nullptr is returned. Otherwise, the resulting bone
     *  pointer is returned.
     *  \param name The name of the bone to look up.
     *  \returns The bone with the given name, or nullptr if it could not be found.
     */
    Bone *FindBone(UTF8StringView name) const;

    /*! \brief Look up the index in the skeleton of a bone with the given name/tag. If no root bone was set,
     *  or the bone could not be found, -1 is returned. Otherwise, the index is returned.
     *  \param name The name of the bone to look up.
     *  \returns The index of the bone with the given name, or -1 if it could not be found. */
    uint32 FindBoneIndex(UTF8StringView name) const;

    /*! \brief Get the root Bone of this skeleton, which all nested Bones fall under.
     *  If no root bone was set on this Skeleton, nullptr is returned
     *  \returns The root bone of this skeleton, or nullptr */
    HYP_METHOD(Serialize, Property="RootBone")
    const RC<Bone> &GetRootBone() const;

    /*! \brief Set the root Bone of this skeleton, which all nested Bones fall under.
     *  \param bone The root bone to set on this skeleton. */
    HYP_METHOD(Serialize, Property="RootBone")
    void SetRootBone(const RC<Bone> &bone);

    /*! \brief Returns the number of bones in this skeleton.
     *  \returns The number of bones in this skeleton. */
    SizeType NumBones() const;

    /*! \brief Get the array of animations that are associated with this skeleton.
     *  \returns The array of animations associated with this skeleton. */
    HYP_METHOD(Serialize, Property="Animations")
    const Array<Animation> &GetAnimations() const
        { return m_animations; }

    /*! \brief Set the array of animations that are associated with this skeleton.
     *  \param animations The array of animations to set on this skeleton. */
    HYP_METHOD(Serialize, Property="Animations")
    void SetAnimations(const Array<Animation> &animations)
        { m_animations = animations; }

    /*! \brief Get the number of animations that are associated with this skeleton.
     *  \returns The number of animations associated with this skeleton.
     */
    SizeType NumAnimations() const
        { return m_animations.Size(); }

    /*! \brief Add an animation to this skeleton.
     *  \param animation The animation to add to this skeleton.
     */
    void AddAnimation(Animation &&animation);

    /*! \brief Get the animation at the given index. Ensure the index is valid before calling this.
     *  \param index The index of the animation to get.
     *  \returns The animation at the given index.
     */
    Animation &GetAnimation(SizeType index)
        { return m_animations[index]; }

    /*! \brief Get the animation at the given index. Ensure the index is valid before calling this.
     *  \param index The index of the animation to get.
     *  \returns The animation at the given index.
     */
    const Animation &GetAnimation(SizeType index) const
        { return m_animations[index]; }

    /*! \brief Find an animation with the given name. If the animation could not be found, nullptr is returned.
     *  \param name The name of the animation to find.
     *  \param out_index The index of the animation, if it was found.
     *  \returns The animation with the given name, or nullptr if it could not be found.
     */
    const Animation *FindAnimation(const String &name, uint32 *out_index) const;
    
    void Init();
    void Update(GameCounter::TickUnit delta);

private:
    SkeletonBoneData            m_bone_data;
    
    RC<Bone>                    m_root_bone;
    Array<Animation>            m_animations;

    mutable DataMutationState   m_mutation_state;
};

} // namespace hyperion

#endif