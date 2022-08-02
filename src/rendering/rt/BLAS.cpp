#include "BLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

BLAS::BLAS(Ref<Mesh> &&mesh, const Transform &transform)
    : EngineComponent(),
      m_mesh(std::move(mesh)),
      m_transform(transform)
{
}

BLAS::~BLAS()
{
    Teardown();
}

void BLAS::SetMesh(Ref<Mesh> &&mesh)
{
    m_mesh = std::move(mesh);

    if (!m_wrapped.GetGeometries().empty()) {
        auto size = static_cast<int64_t>(m_wrapped.GetGeometries().size());

        while (size) {
            m_wrapped.RemoveGeometry(size--);
        }
    }

    if (m_mesh != nullptr && IsInitCalled()) {
        m_mesh.Init();
        
        m_wrapped.AddGeometry(std::make_unique<AccelerationGeometry>(
            m_mesh->BuildPackedVertices(),
            m_mesh->BuildPackedIndices()
        ));
    }
}

void BLAS::SetTransform(const Transform &transform)
{
    m_transform = transform;

    if (IsInitCalled()) {
        m_wrapped.SetTransform(m_transform.GetMatrix());
    }
}

void BLAS::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ACCELERATION_STRUCTURES, [this, engine](...) {
        if (m_mesh != nullptr) {
            m_mesh.Init();

            m_wrapped.SetTransform(m_transform.GetMatrix());
            m_wrapped.AddGeometry(std::make_unique<AccelerationGeometry>(
                m_mesh->BuildPackedVertices(),
                m_mesh->BuildPackedIndices()
            ));
        }

        EngineComponent::Create(
            engine,
            engine->GetInstance()
        );

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ACCELERATION_STRUCTURES, [this](...) {
            EngineComponent::Destroy(GetEngine());

            m_mesh.Reset();
        }));
    }));
}

void BLAS::Update(Engine *engine)
{
    if (!IsInitCalled()) {
        return;
    }

    if (!NeedsUpdate()) {
        return;
    }

    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(engine->GetInstance()));
}

} // namespace hyperion::v2