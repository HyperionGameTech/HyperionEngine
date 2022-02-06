#ifndef RENDERABLE_H
#define RENDERABLE_H

#include "shader.h"
#include "../math/vertex.h"
#include "../math/ray.h"
#include "../math/bounding_box.h"

#include <memory>
#include <vector>

namespace hyperion {

class CoreEngine;
class Camera;
class Renderer;

using Vertices_t = std::vector<Vertex>;

class Renderable {
    friend class Renderer;
public:
    enum RenderBucket {
        RB_SKY = 0,
        RB_OPAQUE = 1,
        RB_TRANSPARENT = 2,
        RB_PARTICLE = 3,
        RB_SCREEN = 4,
        RB_DEBUG = 5,
        RB_BUFFER = 6
    };

    Renderable(RenderBucket bucket = RB_OPAQUE);
    virtual ~Renderable() = default;

    inline RenderBucket GetRenderBucket() const { return m_bucket; }
    inline void SetRenderBucket(RenderBucket bucket) { m_bucket = bucket; }
    inline std::shared_ptr<Shader> GetShader() { return m_shader; }
    inline void SetShader(const std::shared_ptr<Shader> &shader) { m_shader = shader; }
    inline const BoundingBox &GetAABB() const { return m_aabb; }

    virtual bool IntersectRay(const Ray &ray, const Transform &transform, RaytestHit &out) const;
    virtual bool IntersectRay(const Ray &ray, const Transform &transform, RaytestHitList_t &out) const;

    virtual void Render(Renderer *renderer, Camera *cam) = 0;

protected:
    RenderBucket m_bucket;
    std::shared_ptr<Shader> m_shader;
    BoundingBox m_aabb;
};

} // namespace hyperion

#endif
