#ifndef MESH_ARRAY_H
#define MESH_ARRAY_H

#include "../mesh.h"
#include "../material.h"
#include "../../math/transform.h"

#include <vector>
#include <memory>

namespace hyperion {
struct Submesh {
    std::shared_ptr<Mesh> mesh;
    Transform transform;
};

class MeshArray : public Renderable {
public:
    MeshArray();
    virtual ~MeshArray() = default;

    virtual void Render(Renderer *renderer, Camera *cam) override;

    // merges all submeshes into one
    void Optimize();

    void AddSubmesh(const Submesh &submesh);
    void ClearSubmeshes();

protected:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

    void UpdateSubmeshes();
    void ApplyTransforms();

    std::vector<Submesh> m_submeshes;
};
} // namespace hyperion

#endif
