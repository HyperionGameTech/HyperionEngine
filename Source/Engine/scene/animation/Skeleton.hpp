/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>
#include <asset/AssetReference.hpp>

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/containers/Array.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/math/Mat4f.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Bone;
class Animation;
class RenderProxySkeleton;

HYP_CLASS()
class HYP_API Skeleton final : public AssetObject
{
    HYP_OBJECT_BODY(Skeleton);

    friend class Bone;
    friend class AnimationSystem;

public:
    Skeleton();

    explicit Skeleton(const Handle<Bone>& rootBone);

    Skeleton(const Skeleton& other) = delete;
    Skeleton& operator=(const Skeleton& other) = delete;

    ~Skeleton() override;

    /*! \brief Look up a bone with the given name/tag. If no root bone was set,
     *  or the bone could not be found, nullptr is returned. Otherwise, the resulting bone
     *  pointer is returned.
     *  \param name The name of the bone to look up.
     *  \returns The bone with the given name, or nullptr if it could not be found.
     */
    Bone* FindBone(StringHash name) const;

    /*! \brief Look up the index in the skeleton of a bone with the given name/tag. If no root bone was set,
     *  or the bone could not be found, -1 is returned. Otherwise, the index is returned.
     *  \param name The name of the bone to look up.
     *  \returns The index of the bone with the given name, or -1 if it could not be found. */
    uint32 FindBoneIndex(StringHash name) const;

    /*! \brief Get the root Bone of this skeleton, which all nested Bones fall under.
     *  If no root bone was set on this Skeleton, nullptr is returned
     *  \returns The root bone of this skeleton, or nullptr */
    HYP_METHOD(Property = "RootBone")
    const Handle<Bone>& GetRootBone() const;

    /*! \brief Set the root Bone of this skeleton, which all nested Bones fall under.
     *  \param bone The root bone to set on this skeleton. */
    HYP_METHOD(Property = "RootBone")
    void SetRootBone(const Handle<Bone>& bone);

    HYP_FORCE_INLINE const Array<Handle<Animation>>& GetAnimations() const
    {
        return m_animations;
    }

    HYP_FORCE_INLINE void SetAnimations(const Array<Handle<Animation>>& animations)
    {
        m_animations = animations;
    }

    HYP_METHOD()
    const Handle<Animation>& GetAnimation(uint32 index) const
    {
        if (index >= m_animations.Size())
        {
            return Handle<Animation>::empty;
        }

        return m_animations[index];
    }

    void UpdateRenderProxy(RenderProxySkeleton* proxy);

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

private:
    void Init() override;

    HYP_FIELD(Property = "RootBone", SaveAsReference = false)
    Handle<Bone> m_rootBone;

    HYP_FIELD(Property = "Animations")
    Array<Handle<Animation>> m_animations;

    int m_renderProxyVersion;
};

} // namespace Hyperion
