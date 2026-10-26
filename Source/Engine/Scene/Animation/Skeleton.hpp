/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetObject.hpp>
#include <Asset/AssetImportInfo.hpp>
#include <Asset/AssetReference.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Bone;
class Animation;
struct RenderProxySkeleton;

struct SkeletonBindData
{
    static constexpr uint32 InvalidSlot = ~0u;

    Array<Name> boneNames;
    Array<uint32> parentSlots;
    Array<Transform> restLocals;
    Array<Mat4f> inverseBindMatrices;
    Map<Name, uint32> slotsByBoneName;

    HYP_FORCE_INLINE uint32 NumSlots() const
    {
        return uint32(boneNames.Size());
    }

    HYP_FORCE_INLINE uint32 FindSlot(Name boneName) const
    {
        auto it = slotsByBoneName.Find(boneName);

        return it != slotsByBoneName.End() ? it->second : InvalidSlot;
    }

    bool IsSlotWithin(uint32 slot, Name ancestorBoneName) const;
};

// each slot's model matrix from a pose of local transforms, and the skinning matrix that takes the bind pose to it
ENGINE_API void ComputeSkeletonPoseMatrices(const SkeletonBindData& bindData, Span<const Transform> pose, Span<Mat4f> outModelMatrices, Span<Mat4f> outSkinningMatrices);

HYP_STRUCT()
struct SkeletonSocket
{
    HYP_STRUCT_BODY(SkeletonSocket);

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name name;

    HYP_FIELD(Property = "BoneName", Serialize = true, Editor = true, EditorType = "Bone")
    Name boneName;

    HYP_FIELD(Property = "Offset", Serialize = true, Editor = true)
    Transform offset;

    HYP_FORCE_INLINE bool operator==(const SkeletonSocket& other) const
    {
        return name == other.name && boneName == other.boneName && offset == other.offset;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(boneName);
        hc.Add(offset);

        return hc;
    }
};

HYP_CLASS(AssetBucket = "Skeletons")
class ENGINE_API Skeleton final : public AssetObject
{
    HYP_OBJECT_BODY(Skeleton);

    friend class Bone;

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

    HYP_METHOD()
    const Handle<Animation>& FindAnimation(Name name) const;

    HYP_METHOD()
    Array<Name> GetAnimationNames() const;

    HYP_METHOD()
    Array<Name> GetBoneNames() const;

    // tracks of the clip that drive a bone of this skeleton; by bone name, unless a plugin that retargets counts them
    HYP_METHOD()
    uint32 CountBoundTracks(const Handle<Animation>& animation) const;

    using BoundTrackCounter = uint32 (*)(const Animation& animation, const Skeleton& skeleton);

    static void SetBoundTrackCounter(BoundTrackCounter counter);

    HYP_METHOD(Property = "Sockets", NoScriptBindings)
    const Array<SkeletonSocket>& GetSockets() const
    {
        return m_sockets;
    }

    HYP_METHOD(Property = "Sockets", NoScriptBindings)
    void SetSockets(const Array<SkeletonSocket>& sockets)
    {
        m_sockets = sockets;
    }

    // the humanoid mapping of a skeleton saved before it became an extension, kept as HMF until the extension takes it
    HYP_METHOD(Property = "Humanoid", NoScriptBindings, Editor = false, OmitNull)
    const BoxedValue& GetLegacyHumanoid() const
    {
        return m_legacyHumanoid;
    }

    HYP_METHOD(Property = "Humanoid", NoScriptBindings, Editor = false, OmitNull)
    void SetLegacyHumanoid(const BoxedValue& legacyHumanoid);

    BoxedValue TakeLegacyHumanoid() const;

    const SkeletonSocket* FindSocket(Name name) const;

    HYP_METHOD()
    Array<Name> GetSocketNames() const;

    const SkeletonBindData& GetBindData() const;

    HYP_FORCE_INLINE const AssetImportInfo& GetImportInfo() const
    {
        return m_importInfo;
    }

    HYP_FORCE_INLINE void SetImportInfo(const AssetImportInfo& importInfo)
    {
        m_importInfo = importInfo;
    }

    // bumped whenever the bone tree is replaced, so pose instances know to rebuild
    HYP_FORCE_INLINE uint32 GetRevision() const
    {
        return m_revision;
    }

    Handle<Skeleton> CreateInstance() const;

    HYP_FORCE_INLINE const Skeleton* GetInstanceSource() const
    {
        return m_instanceSource;
    }

    // false once the source's bones changed since this instance was made from it
    bool IsInstanceCurrent() const;

    HYP_FORCE_INLINE Span<const Mat4f> GetPoseModelMatrices() const
    {
        return m_poseModelMatrices.ToSpan();
    }

    HYP_FORCE_INLINE Span<const Transform> GetPose() const
    {
        return m_pose.ToSpan();
    }

    // an instance's pose, local transforms by bind data slot, from a pose writer (eg the animation system); the skeleton works
    // out the model and skinning matrices from it. An empty pose shows the bind pose
    void SetPose(Span<const Transform> localPose);

    void UpdateRenderProxy(RenderProxySkeleton* proxy);

    const int* GetRenderProxyVersionPtr() const
    {
        return &m_renderProxyVersion;
    }

    void SetNeedsRenderProxyUpdate()
    {
        ++m_renderProxyVersion;
    }

protected:
    // marks the clips this skeleton lists as authored for it, for clips saved before they recorded their skeleton
    void OnLoaded() override;

    // clips bound to this skeleton choose between name binding and retargeting by its humanoid mapping
    void OnExtensionChanged(AssetExtension& extension) override;

private:
    HYP_FIELD(Property = "RootBone")
    Handle<Bone> m_rootBone;

    HYP_FIELD(Property = "Animations")
    Array<Handle<Animation>> m_animations;

    HYP_FIELD(Property = "Sockets")
    Array<SkeletonSocket> m_sockets;

    mutable BoxedValue m_legacyHumanoid;

    HYP_FIELD(Property = "ImportInfo", Serialize, Editor = false)
    AssetImportInfo m_importInfo;

    uint32 m_revision = 0;

    int m_renderProxyVersion;

    const Skeleton* m_instanceSource = nullptr;
    uint32 m_instanceSourceRevision = 0;

    mutable SkeletonBindData m_bindData;
    mutable bool m_bindDataBuilt = false;

    Array<Transform> m_pose;
    Array<Mat4f> m_poseModelMatrices;
    Array<Mat4f> m_poseSkinningMatrices;
};

} // namespace Hyperion
