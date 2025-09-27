/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <core/math/Quaternion.hpp>

#include <core/Types.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class OgreXMLSkeletonLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(OgreXMLSkeletonLoader);

public:
    struct OgreXMLSkeleton
    {
        struct BoneData
        {
            String name;
            uint32 id;

            String parentName;
            Vector3 bindingTranslation;
            Quaternion bindingRotation;
        };

        struct KeyframeData
        {
            float time;
            Vector3 translation;
            Quaternion rotation;
        };

        struct AnimationTrackData
        {
            String boneName;
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

