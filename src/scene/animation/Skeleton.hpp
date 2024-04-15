/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_SKELETON_H
#define HYPERION_V2_SKELETON_H

#include <core/Base.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <rendering/ShaderDataState.hpp>
#include <rendering/Buffers.hpp>
#include <core/lib/DynArray.hpp>
#include <scene/animation/Animation.hpp>
#include <scene/NodeProxy.hpp>
#include <GameCounter.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Engine;
class Bone;

struct SkeletonBoneData
{
    using BoneMatricesPtr = RC<FixedArray<Matrix4, SkeletonShaderData::max_bones>>;

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

    void SetMatrix(uint index, const Matrix4 &matrix)
    {
        AssertThrow(matrices && index < matrices->Size());

        (*matrices)[index] = matrix;
    }

    const Matrix4 &GetMatrix(uint index)
    {
        AssertThrow(matrices && index < matrices->Size());

        return (*matrices)[index];
    }
};

class HYP_API Skeleton
    : public BasicObject<STUB_CLASS(Skeleton)>
{
public:
    Skeleton();
    Skeleton(const NodeProxy &root_bone);
    Skeleton(const Skeleton &other) = delete;
    Skeleton &operator=(const Skeleton &other) = delete;
    ~Skeleton();

    ShaderDataState GetShaderDataState() const
        { return m_shader_data_state; }

    void SetShaderDataState(ShaderDataState state)
        { m_shader_data_state = state; }

    /*! \brief Look up a bone with the given name/tag. If no root bone was set,
     * or the bone could not be found, nullptr is returned. Otherwise, the resulting bone
     * pointer is returned.
     */
    Bone *FindBone(const String &name);
    
    /*! \brief Look up a bone with the given name/tag. If no root bone was set,
     * or the bone could not be found, nullptr is returned. Otherwise, the resulting bone
     * pointer is returned.
     */
    const Bone *FindBone(const String &name) const
        { return const_cast<Skeleton *>(this)->FindBone(name); }

    /*! \brief Look up the index in the skeleton of a bone with the given name/tag. If no root bone was set,
     * or the bone could not be found, -1 is returned. Otherwise, the index is returned.
     */
    uint FindBoneIndex(const String &name) const;

    /*! \brief Get the root Bone of this skeleton, which all nested Bones fall under.
     * If no root bone was set on this Skeleton, nullptr is returned
     * @returns The root bone of this skeleton, or nullptr
     */
    Bone *GetRootBone();

    /*! \brief Get the root Bone of this skeleton, which all nested Bones fall under.
     * If no root bone was set on this Skeleton, nullptr is returned
     * @returns The root bone of this skeleton, or nullptr
     */
    const Bone *GetRootBone() const;

    void SetRootBone(const NodeProxy &root_bone);

    NodeProxy &GetRoot()
        { return m_root_bone; }

    const NodeProxy &GetRoot() const
        { return m_root_bone; }

    SizeType NumBones() const;
    
    Array<Animation> &GetAnimations()
        { return m_animations; }

    const Array<Animation> &GetAnimations() const
        { return m_animations; }

    SizeType NumAnimations() const
        { return m_animations.Size(); }

    void AddAnimation(Animation &&animation);

    Animation &GetAnimation(SizeType index)
        { return m_animations[index]; }

    const Animation &GetAnimation(SizeType index) const
        { return m_animations[index]; }

    const Animation *FindAnimation(const String &name, uint *out_index) const;
    
    void Init();
    void Update(GameCounter::TickUnit delta);

private:
    SkeletonBoneData        m_bone_data;
    
    NodeProxy               m_root_bone;
    Array<Animation>        m_animations;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif