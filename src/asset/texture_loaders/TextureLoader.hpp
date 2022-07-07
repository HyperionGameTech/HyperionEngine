#ifndef HYPERION_V2_TEXTURE_LOADER_H
#define HYPERION_V2_TEXTURE_LOADER_H

#include <asset/LoaderObject.hpp>
#include <asset/Loader.hpp>
#include <rendering/Texture.hpp>

namespace hyperion::v2 {

template <>
struct LoaderObject<Texture, LoaderFormat::TEXTURE_2D> {
    class Loader : public LoaderBase<Texture, LoaderFormat::TEXTURE_2D> {
        static LoaderResult LoadFn(LoaderState *state, Object &);
        static std::unique_ptr<Texture> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };

    std::vector<unsigned char> data;
    int width;
    int height;
    int num_components;
    Image::InternalFormat format;
};

} // namespace hyperion::v2

#endif