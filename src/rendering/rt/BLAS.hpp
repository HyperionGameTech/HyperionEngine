#ifndef HYPERION_V2_BLAS_H
#define HYPERION_V2_BLAS_H

#include <core/Base.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <math/Transform.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

namespace hyperion::v2 {

using renderer::BottomLevelAccelerationStructure;
using renderer::AccelerationStructureFlagBits;
using renderer::Frame;

class BLAS : public EngineComponentWrapper<STUB_CLASS(BLAS), BottomLevelAccelerationStructure>
{
public:
    BLAS(
        Handle<Mesh> &&mesh,
        Handle<Material> &&material,
        const Transform &transform
    );
    BLAS(const BLAS &other) = delete;
    BLAS &operator=(const BLAS &other) = delete;
    ~BLAS();
    
    Handle<Mesh> &GetMesh() { return m_mesh; }
    const Handle<Mesh> &GetMesh() const { return m_mesh; }
    void SetMesh(Handle<Mesh> &&mesh);

    Handle<Material> &GetMaterial() { return m_material; }
    const Handle<Material> &GetMaterial() const { return m_material; }
    void SetMaterial(Handle<Material> &&material);

    const Transform &GetTransform() const { return m_transform; }
    void SetTransform(const Transform &transform);

    void Init(Engine *engine);
    void Update(Engine *engine);
    void UpdateRender(
        Engine *engine,
        Frame *frame,
        bool &out_was_rebuilt
    );

private:
    void SetNeedsUpdate()
        { m_wrapped.SetFlag(AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING); }

    bool NeedsUpdate() const
        { return bool(m_wrapped.GetFlags()); }

    Handle<Mesh> m_mesh;
    Handle<Material> m_material;
    Transform m_transform;
};

} // namespace hyperion::v2

#endif