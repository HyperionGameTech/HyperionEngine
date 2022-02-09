#ifndef BOUNDING_BOX_RENDERER_H
#define BOUNDING_BOX_RENDERER_H

#include <cstddef>
#include <vector>

#include "../renderable.h"
#include "../../math/bounding_box.h"
#include "../mesh.h"

namespace hyperion {

class BoundingBoxRenderer : public Renderable {
    static const std::vector<MeshIndex> indices;
public:
    BoundingBoxRenderer();
    BoundingBoxRenderer(const BoundingBox &) = delete;
    virtual ~BoundingBoxRenderer();

    virtual void Render(Renderer *renderer, Camera *cam) override;

    inline void SetAABB(const BoundingBox &aabb) { m_aabb = aabb; }

protected:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

private:
    void UpdateVertices();

    Mesh *m_mesh;
    std::vector<Vertex> m_vertices;
};

} // namespace hyperion

#endif
