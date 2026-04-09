/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetLoader.hpp>

#include <Core/math/Quaternion.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

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

} // namespace Hyperion
