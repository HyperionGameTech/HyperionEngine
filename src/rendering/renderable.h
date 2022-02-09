#ifndef RENDERABLE_H
#define RENDERABLE_H

#include "shader.h"
#include "../math/vertex.h"
#include "../math/ray.h"
#include "../math/bounding_box.h"
#include "../asset/fbom/fbom.h"

#include <memory>
#include <vector>

namespace hyperion {

class Renderer;
class CoreEngine;
class Camera;
class Renderer;

using Vertices_t = std::vector<Vertex>;

class Renderable : public fbom::FBOMLoadable {
    friend class Renderer;
public:
    enum RenderBucket {
        RB_SKY,
        RB_OPAQUE,
        RB_TRANSPARENT,
        RB_PARTICLE,
        RB_SCREEN,
        RB_DEBUG,
        RB_BUFFER,
        // ...
        RB_MAX
    };

    Renderable(const fbom::FBOMType &loadable_type,
        RenderBucket bucket = RB_OPAQUE);
    Renderable(const Renderable &other) = delete;
    Renderable &operator=(const Renderable &other) = delete;
    virtual ~Renderable() = default;

    inline RenderBucket GetRenderBucket() const { return m_bucket; }
    inline void SetRenderBucket(RenderBucket bucket) { m_bucket = bucket; }
    inline std::shared_ptr<Shader> GetShader() { return m_shader; }
    inline void SetShader(const std::shared_ptr<Shader> &shader) { m_shader = shader; }
    inline const BoundingBox &GetAABB() const { return m_aabb; }

    virtual bool IntersectRay(const Ray &ray, const Transform &transform, RaytestHit &out) const;
    virtual bool IntersectRay(const Ray &ray, const Transform &transform, RaytestHitList_t &out) const;

    virtual void Render(Renderer *renderer, Camera *cam) = 0;

    virtual std::shared_ptr<Loadable> Clone() override;

protected:
    RenderBucket m_bucket;
    std::shared_ptr<Shader> m_shader;
    BoundingBox m_aabb;

    virtual std::shared_ptr<Renderable> CloneImpl() = 0;
};

} // namespace hyperion

#endif
