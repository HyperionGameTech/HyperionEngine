#include "TLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(CreateTLAS) : RenderCommandBase2
{
    renderer::TopLevelAccelerationStructure *tlas;
    Array<renderer::BottomLevelAccelerationStructure *> blases;

    RENDER_COMMAND(CreateTLAS)(renderer::TopLevelAccelerationStructure *tlas, Array<renderer::BottomLevelAccelerationStructure *> &&blases)
        : tlas(tlas),
          blases(std::move(blases))
    {
    }

    virtual Result operator()(Engine *engine)
    {
        return tlas->Create(
            engine->GetDevice(),
            engine->GetInstance(),
            std::vector<renderer::BottomLevelAccelerationStructure *>(blases.Begin(), blases.End())
        );
    }
};

struct RENDER_COMMAND(DestroyTLAS) : RenderCommandBase2
{
    renderer::TopLevelAccelerationStructure *tlas;

    RENDER_COMMAND(DestroyTLAS)(renderer::TopLevelAccelerationStructure *tlas)
        : tlas(tlas),
    {
    }

    virtual Result operator()(Engine *engine)
    {
        return tlas->Destroy(engine->GetDevice());
    }
};

TLAS::TLAS()
    : EngineComponentBase()
{
}

TLAS::~TLAS()
{
    Teardown();
}

void TLAS::AddBLAS(Handle<BLAS> &&blas)
{
    if (!blas) {
        return;
    }

    if (IsInitCalled()) {
        if (!GetEngine()->InitObject(blas)) {
            // the blas could not be initialzied. not valid?
            return;
        }
    }

    std::lock_guard guard(m_blas_updates_mutex);

    m_blas_pending_addition.PushBack(std::move(blas));
    m_has_blas_updates.store(true);
}

void TLAS::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);
    
    // add all pending to be added to the list
    if (m_has_blas_updates.load()) {
        std::lock_guard guard(m_blas_updates_mutex);

        m_blas.Concat(std::move(m_blas_pending_addition));

        m_has_blas_updates.store(false);
    }

    for (SizeType i = 0; i < m_blas.Size(); i++) {
        AssertThrow(m_blas[i] != nullptr);
        AssertThrow(engine->InitObject(m_blas[i]));
    }

    Array<BottomLevelAccelerationStructure *> internal_blases;
    internal_blases.Resize(m_blas.Size());

    for (SizeType i = 0; i < m_blas.Size(); i++) {
        internal_blases[i] = &m_blas[i]->GetInternalBLAS();
    }

    RenderCommands::Push<RENDER_COMMAND(CreateTLAS)>(&m_tlas, std::move(internal_blases));

    HYP_FLUSH_RENDER_QUEUE(engine);

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        m_blas.Clear();

        if (m_has_blas_updates.load()) {
            m_blas_updates_mutex.lock();
            m_blas_pending_addition.Clear();
            m_blas_updates_mutex.unlock();
            m_has_blas_updates.store(false);
        }

        RenderCommands::Push<RENDER_COMMAND(DestroyTLAS)>(&m_tlas);

        HYP_FLUSH_RENDER_QUEUE(GetEngine());
    });
}

void TLAS::PerformBLASUpdates()
{
    std::lock_guard guard(m_blas_updates_mutex);

    while (m_blas_pending_addition.Any()) {
        auto blas = m_blas_pending_addition.PopFront();
        AssertThrow(blas);

        m_tlas.AddBLAS(&blas->GetInternalBLAS());

        m_blas.PushBack(std::move(blas));
    }

    m_has_blas_updates.store(false);
}

void TLAS::UpdateRender(
    Engine *engine,
    Frame *frame,
    RTUpdateStateFlags &out_update_state_flags
)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    out_update_state_flags = renderer::RT_UPDATE_STATE_FLAGS_NONE;

    if (m_has_blas_updates.load()) {
        PerformBLASUpdates();
    }
    
    for (auto &blas : m_blas) {
        AssertThrow(blas != nullptr);
        
        AssertThrow(!blas->GetInternalBLAS().GetGeometries().empty());
        AssertThrow(blas->GetInternalBLAS().GetGeometries()[0]->GetPackedIndexStorageBuffer() != nullptr);
        AssertThrow(blas->GetInternalBLAS().GetGeometries()[0]->GetPackedVertexStorageBuffer() != nullptr);

        //bool was_blas_rebuilt = false;
        //blas->UpdateRender(engine, frame, was_blas_rebuilt);
    }
    
    HYPERION_ASSERT_RESULT(m_tlas.UpdateStructure(engine->GetInstance(), out_update_state_flags));
}

} // namespace hyperion::v2