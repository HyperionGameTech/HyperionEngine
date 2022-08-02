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

    m_blas.push_back(std::move(blas));
}

void TLAS::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ACCELERATION_STRUCTURES, [this, engine](...) {
        std::vector<BottomLevelAccelerationStructure *> blas(m_blas.size());

        // extract backend objects
        for (size_t i = 0; i < m_blas.size(); i++) {
            AssertThrow(m_blas[i] != nullptr);

            m_blas[i].Init();
            blas[i] = &m_blas[i]->Get();
        }

        EngineComponent::Create(
            engine,
            engine->GetInstance(),
            std::move(blas)
        );

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ACCELERATION_STRUCTURES, [this](...) {
            m_blas.clear();

            EngineComponent::Destroy(GetEngine());
        }));
    }));
}

void TLAS::Update(Engine *engine)
{
    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(engine->GetInstance()));
}


} // namespace hyperion::v2