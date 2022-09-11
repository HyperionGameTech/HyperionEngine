#include "BLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

Blas::Blas(
    Handle<Mesh> &&mesh,
    Handle<Material> &&material,
    const Transform &transform
) : EngineComponentWrapper(),
    m_mesh(std::move(mesh)),
    m_material(std::move(material)),
    m_transform(transform)
{
}

Blas::~Blas()
{
    Teardown();
}

void Blas::SetMesh(Handle<Mesh> &&mesh)
{
    m_mesh = std::move(mesh);

    if (!m_wrapped.GetGeometries().empty()) {
        auto size = static_cast<UInt>(m_wrapped.GetGeometries().size());

        while (size) {
            m_wrapped.RemoveGeometry(--size);
        }
    }

    if (m_mesh && IsInitCalled()) {
        GetEngine()->InitObject(m_mesh);

        auto material_id = m_material
            ? m_material->GetID()
            : Material::empty_id;
        
        m_wrapped.AddGeometry(std::make_unique<AccelerationGeometry>(
            m_mesh->BuildPackedVertices(),
            m_mesh->BuildPackedIndices(),
            material_id
                ? material_id.value - 1
                : 0u
        ));
    }
}

void Blas::SetTransform(const Transform &transform)
{
    m_transform = transform;

    if (IsInitCalled()) {
        m_wrapped.SetTransform(m_transform.GetMatrix());
    }
}

void Blas::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    /*if (engine->InitObject(m_material)) {
        for (auto &geometry : m_wrapped.GetGeometries()) {
            AssertThrow(geometry != nullptr);

            geometry->SetMaterialIndex(m_material->GetID().value - 1);
        }
    }*/

    UInt material_index = 0u;

    if (engine->InitObject(m_material)) {
        material_index = m_material->GetID().value - 1;
    }

    if (engine->InitObject(m_mesh)) {
        m_wrapped.SetTransform(m_transform.GetMatrix());
        m_wrapped.AddGeometry(std::make_unique<AccelerationGeometry>(
            m_mesh->BuildPackedVertices(),
            m_mesh->BuildPackedIndices(),
            material_index
        ));
    }

    EngineComponentWrapper::Create(
        engine,
        engine->GetInstance()
    );

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        EngineComponentWrapper::Destroy(GetEngine());

        m_mesh.Reset();
    });
}

void Blas::Update(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    // TODO: Should these be removed?
    // Entity will already Update Mesh and Material, as well as BLAS.
    if (m_material) {
        m_material->Update(engine);
    }
}

void Blas::UpdateRender(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (!NeedsUpdate()) {
        return;
    }

    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(engine->GetInstance()));
}

} // namespace hyperion::v2