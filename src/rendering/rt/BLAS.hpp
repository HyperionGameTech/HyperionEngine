#ifndef HYPERION_V2_BLAS_H
#define HYPERION_V2_BLAS_H

#include <rendering/Base.hpp>
#include <rendering/Mesh.hpp>

#include <math/Transform.hpp>

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

namespace hyperion::v2 {

using renderer::BottomLevelAccelerationStructure;
using renderer::AccelerationStructureFlags;

class Blas : public EngineComponentWrapper<STUB_CLASS(Blas), BottomLevelAccelerationStructure>
{
public:
    Blas(Ref<Mesh> &&mesh, const Transform &transform);
    Blas(const Blas &other) = delete;
    Blas &operator=(const Blas &other) = delete;
    ~Blas();

    Mesh *GetMesh() const { return m_mesh.ptr; }
    void SetMesh(Ref<Mesh> &&mesh);

    const Transform &GetTransform() const         { return m_transform; }
    void SetTransform(const Transform &transform);

    void Init(Engine *engine);
    void Update(Engine *engine);

private:
    void SetNeedsUpdate()
        { m_wrapped.SetFlag(AccelerationStructureFlags::ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING); }

    bool NeedsUpdate() const
        { return m_wrapped.GetFlags() & AccelerationStructureFlags::ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING; }

    Ref<Mesh> m_mesh;
    Transform m_transform;
};

} // namespace hyperion::v2

#endif