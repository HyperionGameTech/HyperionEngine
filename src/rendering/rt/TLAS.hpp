#ifndef HYPERION_V2_TLAS_H
#define HYPERION_V2_TLAS_H

#include <rendering/Base.hpp>
#include "BLAS.hpp"

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

namespace hyperion::v2 {

using renderer::TopLevelAccelerationStructure;

class TLAS : public EngineComponent<TopLevelAccelerationStructure> {
public:
    TLAS();
    TLAS(const TLAS &other) = delete;
    TLAS &operator=(const TLAS &other) = delete;
    ~TLAS();

    void AddBottomLevelAccelerationStructure(Ref<BLAS> &&blas);
    auto &GetBottomLevelAccelerationStructures()             { return m_blas; }
    const auto &GetBottomLevelAccelerationStructures() const { return m_blas; }

    void Init(Engine *engine);

    /*! \brief Perform any pending updates to the structure*/
    void Update(Engine *engine);

private:
    std::vector<Ref<BLAS>> m_blas;
};

} // namespace hyperion::v2

#endif