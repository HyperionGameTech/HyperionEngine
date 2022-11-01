#ifndef HYPERION_V2_TLAS_H
#define HYPERION_V2_TLAS_H

#include <core/Base.hpp>
#include "BLAS.hpp"

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <mutex>
#include <atomic>

namespace hyperion::v2 {

using renderer::TopLevelAccelerationStructure;
using renderer::RTUpdateStateFlags;

class TLAS : public EngineComponentBase<STUB_CLASS(TLAS)>
{
public:
    TLAS();
    TLAS(const TLAS &other) = delete;
    TLAS &operator=(const TLAS &other) = delete;
    ~TLAS();

    TopLevelAccelerationStructure &GetInternalTLAS() { return m_tlas; }
    const TopLevelAccelerationStructure &GetInternalTLAS() const { return m_tlas; }

    void AddBLAS(Handle<BLAS> &&blas);

    void Init(Engine *engine);

    /*! \brief Update the TLAS on the RENDER thread, performing any
     * updates to the structure. This is also called for each BLAS underneath this.
     */
    void UpdateRender(
        Engine *engine,
        Frame *frame,
        RTUpdateStateFlags &out_flags
    );

private:
    void PerformBLASUpdates();

    TopLevelAccelerationStructure m_tlas;

    Array<Handle<BLAS>> m_blas;
    Array<Handle<BLAS>> m_blas_pending_addition;

    std::atomic_bool m_has_blas_updates { false };
    std::mutex m_blas_updates_mutex;
};

} // namespace hyperion::v2

#endif