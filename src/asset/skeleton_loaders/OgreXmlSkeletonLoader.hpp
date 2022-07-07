#ifndef HYPERION_V2_OGRE_XML_SKELETON_LOADER_H
#define HYPERION_V2_OGRE_XML_SKELETON_LOADER_H

#include <asset/LoaderObject.hpp>
#include <asset/Loader.hpp>
#include <animation/Skeleton.hpp>

#include <math/Quaternion.hpp>

namespace hyperion::v2 {

template <>
struct LoaderObject<Skeleton, LoaderFormat::OGRE_XML_SKELETON> {
    class Loader : public LoaderBase<Skeleton, LoaderFormat::OGRE_XML_SKELETON> {
        static LoaderResult LoadFn(LoaderState *state, Object &);
        static std::unique_ptr<Skeleton> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };

    struct Bone {
        std::string name;
        uint32_t id;

        std::string parent_name;
        Vector3 binding_translation;
        Quaternion binding_rotation;
    };

    struct Keyframe {
        float time;
        Vector3 translation;
        Quaternion rotation;
    };

    struct AnimationTrack {
        std::string bone_name;
        std::vector<Keyframe> keyframes;
    };

    struct Animation {
        std::string name;
        std::vector<AnimationTrack> tracks;
    };

    std::vector<Bone> bones;
    std::vector<Animation> animations;
};

} // namespace hyperion::v2

#endif