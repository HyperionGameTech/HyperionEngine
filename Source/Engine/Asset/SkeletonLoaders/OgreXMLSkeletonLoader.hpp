/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetLoader.hpp>

#include <Core/Math/Quat4f.hpp>

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
            Name name;
            uint32 id;

            String parentName;
            Vec3f bindingTranslation;
            Quat4f bindingRotation;
        };

        struct KeyframeData
        {
            float time;
            Vec3f translation;
            Quat4f rotation;
        };

        struct AnimationTrackData
        {
            Name boneName;
            Array<KeyframeData> keyframes;
        };

        struct AnimationData
        {
            Name name;
            Array<AnimationTrackData> tracks;
        };

        Array<BoneData> bones;
        Array<AnimationData> animations;
    };

    virtual ~OgreXMLSkeletonLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
