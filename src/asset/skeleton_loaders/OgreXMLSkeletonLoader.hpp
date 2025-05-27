/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OGRE_XML_SKELETON_LOADER_HPP
#define HYPERION_OGRE_XML_SKELETON_LOADER_HPP

#include <asset/Assets.hpp>

#include <core/math/Quaternion.hpp>

#include <Types.hpp>

namespace hyperion {

class OgreXMLSkeletonLoader : public AssetLoaderBase
{
public:
    struct OgreXMLSkeleton
    {
        struct BoneData
        {
            String name;
            uint32 id;

            String parent_name;
            Vector3 binding_translation;
            Quaternion binding_rotation;
        };

        struct KeyframeData
        {
            float time;
            Vector3 translation;
            Quaternion rotation;
        };

        struct AnimationTrackData
        {
            String bone_name;
            Array<KeyframeData> keyframes;
        };

        struct AnimationData
        {
            String name;
            Array<AnimationTrackData> tracks;
        };

        Array<BoneData> bones;
        Array<AnimationData> animations;
    };

    virtual ~OgreXMLSkeletonLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion

#endif