#ifndef HYPERION_V2_TLAS_H
#define HYPERION_V2_TLAS_H

#include <core/Base.hpp>
#include "BLAS.hpp"

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

namespace hyperion::v2 {

using renderer::TopLevelAccelerationStructure;

class Tlas : public EngineComponentWrapper<STUB_CLASS(Tlas), TopLevelAccelerationStructure>
{
public:
    Tlas();
    Tlas(const Tlas &other) = delete;
    Tlas &operator=(const Tlas &other) = delete;
    ~Tlas();

    void AddBlas(Handle<Blas> &&blas);

    void Init(Engine *engine);
    void Update(Engine *engine);
    void UpdateRender(Engine *engine, Frame *frame);

private:
    std::vector<Handle<Blas>> m_blas;
};

} // namespace hyperion::v2

#endif