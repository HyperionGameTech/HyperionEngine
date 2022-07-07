#ifndef HYPERION_V2_RENDERER_H
#define HYPERION_V2_RENDERER_H

#include "RenderList.hpp"

namespace hyperion::v2 {

class Renderer {
public:
    Renderer();
    Renderer(const Renderer &other) = delete;
    Renderer &operator=(const Renderer &other) = delete;
    ~Renderer();

protected:
};

} // namespace hyperion::v2

#endif