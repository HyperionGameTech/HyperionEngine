/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>
#include <core/reflection/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/Array.hpp>

#include <core/debug/Debug.hpp>

#include <core/math/Mat4f.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetReference.hpp>
#include <asset/SkeletonAsset.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Bone;
class Animation;
class RenderProxySkeleton;

/*! \brief Runtime skeleton instance that references shared SkeletonAsset data.
 *  Contains the actual bone node hierarchy that can be manipulated at runtime.
 *  Multiple Skeleton instances can share the same SkeletonAsset. */
HYP_CLASS()
class HYP_API Skeleton final : public AssetObject
{
    HYP_OBJECT_BODY(Skeleton);

    friend class Bone;
    friend class AnimationSystem;

public:
    Skeleton();

    explicit Skeleton(const Handle<Bone>& rootBone);
    explicit Skeleton(const Handle<SkeletonAsset>& asset);

    Skeleton(const Skeleton& other) = delete;
    Skeleton& operator=(const Skeleton& other) = delete;

    ~Skeleton() override;

    /*! \brief Look up a bone with the given name/tag. If no root bone was set,
     *  or the bone could not be found, nullptr is returned. Otherwise, the resulting bone
     *  pointer is returned.
     *  \param name The name of the bone to look up.
     *  \returns The bone with the given name, or nullptr if it could not be found.
     */
    Bone* FindBone(WeakName name) const;

    /*! \brief Look up the index in the skeleton of a bone with the given name/tag. If no root bone was set,
     *  or the bone could not be found, -1 is returned. Otherwise, the index is returned.
     *  \param name The name of the bone to look up.
     *  \returns The index of the bone with the given name, or -1 if it could not be found. */
    uint32 FindBoneIndex(WeakName name) const;

    /*! \brief Get the root Bone of this skeleton, which all nested Bones fall under.
     *  If no root bone was set on this Skeleton, nullptr is returned
     *  \returns The root bone of this skeleton, or nullptr */
    HYP_METHOD(Property = "RootBone")
    const Handle<Bone>& GetRootBone() const;

    /*! \brief Set the root Bone of this skeleton, which all nested Bones fall under.
     *  \param bone The root bone to set on this skeleton. */
    HYP_METHOD(Property = "RootBone")
    void SetRootBone(const Handle<Bone>& bone);

    /*! \brief Get the SkeletonAsset that this skeleton references.
     *  \returns The skeleton asset, or invalid handle if none is set. */
    HYP_FORCE_INLINE const Handle<SkeletonAsset>& GetAsset() const
    {
        return m_skeletonAsset.Resolve();
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

    /*! \internal Serialization only */
    HYP_METHOD(Property = "SkeletonAsset")
    const AssetReference& GetSkeletonAsset() const
    {
        return m_skeletonAsset;
    }

    /*! \internal Serialization only */
    HYP_METHOD(Property = "SkeletonAsset")
    void SetSkeletonAsset(const AssetReference& assetReference);

    HYP_FIELD(Property = "RootBone")
    Handle<Bone> m_rootBone;

    TAssetReference<SkeletonAsset> m_skeletonAsset;

    int m_renderProxyVersion;
};

} // namespace hyperion
