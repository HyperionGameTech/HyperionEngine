#ifndef HYPERION_V2_DUMMY_DATA_H
#define HYPERION_V2_DUMMY_DATA_H

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_image_view.h>
#include <rendering/backend/renderer_sampler.h>

#include <memory>

namespace hyperion::v2 {

class DummyData {
    struct DummyImage {
        std::unique_ptr<Image>     image;
        std::unique_ptr<ImageView> image_view;
        std::unique_ptr<Sampler>   sampler;
    } image_data;
};

} // namespace hyperion::v2

#endif
