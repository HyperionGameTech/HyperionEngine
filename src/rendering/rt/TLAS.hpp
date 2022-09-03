#ifndef HYPERION_V2_TLAS_H
#define HYPERION_V2_TLAS_H

#include <rendering/Base.hpp>
#include "BLAS.hpp"

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <core/lib/DynArray.hpp>

#include <mutex>
#include <atomic>

namespace hyperion::v2 {

using renderer::TopLevelAccelerationStructure;

class TLAS : public EngineComponent<TopLevelAccelerationStructure> {
public:
    TLAS();
    TLAS(const TLAS &other) = delete;
    TLAS &operator=(const TLAS &other) = delete;
    ~TLAS();

    /*! \brief Add/enqueue add a BLAS to the structure. This method is thread-safe, so may be called not just from the
     * render thread but from any thread.
     */
    void AddBottomLevelAccelerationStructure(Ref<BLAS> &&blas);

    void Init(Engine *engine);

    /*! \brief Perform any pending updates to the structure*/
    void Update(Engine *engine);

private:
    DynArray<Ref<BLAS>> m_blas;

    DynArray<Ref<BLAS>> m_blas_pending_addition;
    std::mutex          m_update_blas_mutex;
    std::atomic_bool    m_has_blas_updates{false};
};

} // namespace hyperion::v2

#endif