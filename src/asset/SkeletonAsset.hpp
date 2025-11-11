/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>

#include <core/reflection/ObjectMacros.hpp>

#include <core/containers/Array.hpp>

#include <core/math/Transform.hpp>
#include <core/Name.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Animation;

/*! \brief Describes a single bone in the skeleton hierarchy - serializable data only */
HYP_STRUCT()
struct BoneDesc
{
    HYP_STRUCT_BODY(BoneDesc);

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    Transform bindingTransform;

    HYP_FIELD()
    Name parentName;
};

/*! \brief Describes the skeleton structure and associated animations */
HYP_STRUCT()
struct SkeletonDesc
{
    HYP_STRUCT_BODY(SkeletonDesc);

    HYP_FIELD()
    Array<BoneDesc> bones;

    HYP_FIELD()
    Array<Name> animationNames;

    bool GetAnimationIndex(StringHash name, uint32* outIndex) const
    {
        const auto it = animationNames.FindAs(name);

        if (it != animationNames.End())
        {
            if (outIndex != nullptr)
            {
                *outIndex = uint32(it - animationNames.Begin());
            }

            return true;
        }

        return false;
    }
};

/*! \brief Runtime skeleton data - animations that can be shared between skeleton instances */
HYP_STRUCT()
struct SkeletonData
{
    HYP_STRUCT_BODY(SkeletonData);

    HYP_FIELD()
    Array<Handle<Animation>> animations;

    const Handle<Animation>& GetAnimation(uint32 index) const
    {
        if (index >= animations.Size())
        {
            return Handle<Animation>::empty;
        }

        return animations[index];
    }
};

/*! \brief Asset representing shared skeleton data  */
HYP_CLASS()
class SkeletonAsset : public AssetObject
{
    HYP_OBJECT_BODY(SkeletonAsset);

public:
    SkeletonAsset()
        : AssetObject(),
          m_skeletonDesc()
    {
        AssetObject::SetData(SkeletonData());
    }

    SkeletonAsset(Name name, const SkeletonDesc& desc)
        : AssetObject(name),
          m_skeletonDesc(desc)
    {
        AssetObject::SetData(SkeletonData());
    }

    SkeletonAsset(Name name, const SkeletonDesc& desc, const SkeletonData& skeletonData)
        : AssetObject(name, skeletonData),
          m_skeletonDesc(desc)
    {
    }

    SkeletonAsset(Name name, const SkeletonDesc& desc, SkeletonData&& skeletonData)
        : AssetObject(name, std::move(skeletonData)),
          m_skeletonDesc(desc)
    {
    }

    SkeletonAsset(const SkeletonAsset& other) = delete;
    SkeletonAsset& operator=(const SkeletonAsset& other) = delete;

    SkeletonAsset(SkeletonAsset&& other) noexcept = delete;
    SkeletonAsset& operator=(SkeletonAsset&& other) noexcept = delete;

    ~SkeletonAsset() = default;

    HYP_FORCE_INLINE const SkeletonDesc& GetSkeletonDesc() const
    {
        return m_skeletonDesc;
    }

    HYP_FORCE_INLINE SkeletonData* GetSkeletonData() const
    {
        return GetResourceData<SkeletonData>();
    }

private:
    HYP_FIELD(Serialize)
    SkeletonDesc m_skeletonDesc;
};

} // namespace hyperion
