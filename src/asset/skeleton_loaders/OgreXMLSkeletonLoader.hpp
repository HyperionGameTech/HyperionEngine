#ifndef HYPERION_V2_OGRE_XML_SKELETON_LOADER_H
#define HYPERION_V2_OGRE_XML_SKELETON_LOADER_H

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <math/Quaternion.hpp>
#include <core/Containers.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class OgreXMLSkeletonLoader : public AssetLoader
{
public:
    struct OgreXMLSkeleton
    {
        struct BoneData
        {
            String      name;
            UInt        id;

            String      parent_name;
            Vector3     binding_translation;
            Quaternion  binding_rotation;
        };

        struct KeyframeData
        {
            Float       time;
            Vector3     translation;
            Quaternion  rotation;
        };

        struct AnimationTrackData
        {
            String              bone_name;
            Array<KeyframeData> keyframes;
        };

        struct AnimationData
        {
            String                      name;
            Array<AnimationTrackData>   tracks;
        };

        Array<BoneData> bones;
        Array<AnimationData> animations;
    };

    virtual ~OgreXMLSkeletonLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif