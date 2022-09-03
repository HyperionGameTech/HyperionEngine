#include "TLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

TLAS::TLAS()
    : EngineComponent()
{
}

TLAS::~TLAS()
{
    Teardown();
}

void TLAS::AddBottomLevelAccelerationStructure(Ref<BLAS> &&blas)
{
    if (blas == nullptr) {
        return;
    }

    if (IsInitCalled()) {
        blas.Init();
    }

    std::lock_guard guard(m_update_blas_mutex);

    m_blas_pending_addition.PushBack(std::move(blas));

    m_has_blas_updates.store(true);
}

void TLAS::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ACCELERATION_STRUCTURES, [this, engine](...) {
        if (m_has_blas_updates.load()) {
            m_blas.Concat(std::move(m_blas_pending_addition));
            m_blas_pending_addition = {};

            m_has_blas_updates.store(false);
        }

        for (auto &blas : m_blas) {
            AssertThrow(blas != nullptr);
            blas.Init();
        }

        engine->render_scheduler.Enqueue([this, engine](...) {
            std::vector<BottomLevelAccelerationStructure *> blas(m_blas.Size());

            // extract backend objects
            for (size_t i = 0; i < m_blas.Size(); i++) {
                AssertThrow(m_blas[i] != nullptr);

                m_blas[i].Init();
                blas[i] = &m_blas[i]->Get();
            }

            EngineComponent::Create(
                engine,
                engine->GetInstance(),
                std::move(blas)
            );

            SetReady(true);

            HYPERION_RETURN_OK;
        });

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ACCELERATION_STRUCTURES, [this](...) {
            SetReady(false);

            if (m_has_blas_updates.load()) {
                std::lock_guard guard(m_update_blas_mutex);

                m_blas_pending_addition.Clear();

                m_has_blas_updates.store(false);
            }

            GetEngine()->render_scheduler.Enqueue([this](...) {
                m_blas.Clear();

                EngineComponent::Destroy(GetEngine());

                HYPERION_RETURN_OK;
            });

            HYP_FLUSH_RENDER_QUEUE(GetEngine());
        }));
    }));
}

void TLAS::Update(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (m_has_blas_updates.load()) {
        std::lock_guard guard(m_update_blas_mutex);

        for (auto it = m_blas_pending_addition.Begin(); it != m_blas_pending_addition.End();) {
            if ((*it)->IsReady()) {
                m_wrapped.AddBottomLevelAccelerationStructure(&(*it)->Get());
                m_blas.PushBack(std::move(*it));

                it = m_blas_pending_addition.Erase(it);
            } else {
                ++it;
            }
        }

        m_has_blas_updates.store(!m_blas_pending_addition.Empty());
    }

    //DebugLog(LogType::Debug, "v2:: Update TLAS %llu    Pending: %llu\n", m_blas.Size(), m_blas_pending_addition.Size());

    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(engine->GetInstance()));
}


} // namespace hyperion::v2