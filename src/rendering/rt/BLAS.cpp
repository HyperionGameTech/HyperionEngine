#include "BLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

BLAS::BLAS(
    Handle<Mesh> &&mesh,
    Handle<Material> &&material,
    const Transform &transform
) : EngineComponentWrapper(),
    m_mesh(std::move(mesh)),
    m_material(std::move(material)),
    m_transform(transform)
{
}

BLAS::~BLAS()
{
    Teardown();
}

void BLAS::SetMesh(Handle<Mesh> &&mesh)
{
    // TODO: thread safety

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

void BLAS::SetMaterial(Handle<Material> &&material)
{
    // TODO: thread safety

    m_material = std::move(material);

    if (IsInitCalled()) {
        auto material_id = m_material
            ? m_material->GetID()
            : Material::empty_id;
        
        const auto material_index = material_id
            ? material_id.value - 1
            : 0u;

        if (!m_wrapped.GetGeometries().empty()) {
            for (auto &geometry : m_wrapped.GetGeometries()) {
                if (!geometry) {
                    continue;
                }

                geometry->SetMaterialIndex(material_index);
            }

            m_wrapped.SetFlag(AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
        }
    }
}

void BLAS::SetTransform(const Transform &transform)
{
    // TODO: thread safety

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

void BLAS::Update(Engine *engine)
{
    // no-op
}

void BLAS::UpdateRender(
    Engine *engine,
    Frame *frame,
    bool &out_was_rebuilt
)
{
#if 0 // TopLevelAccelerationStructure does this work here.
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (!NeedsUpdate()) {
        return;
    }
    
    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(engine->GetInstance(), out_was_rebuilt));
#endif
}

} // namespace hyperion::v2