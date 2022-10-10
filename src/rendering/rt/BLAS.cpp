#include "BLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

BLAS::BLAS(
    IDBase entity_id,
    Handle<Mesh> &&mesh,
    Handle<Material> &&material,
    const Transform &transform
) : EngineComponentBase(),
    m_entity_id(entity_id),
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

    if (!m_blas.GetGeometries().empty()) {
        auto size = static_cast<UInt>(m_blas.GetGeometries().size());

        while (size) {
            m_blas.RemoveGeometry(--size);
        }
    }

    if (m_mesh && IsInitCalled()) {
        GetEngine()->InitObject(m_mesh);

        auto material_id = m_material
            ? m_material->GetID()
            : Material::empty_id;
        
        m_blas.AddGeometry(std::make_unique<AccelerationGeometry>(
            m_mesh->BuildPackedVertices(),
            m_mesh->BuildPackedIndices(),
            m_entity_id.ToIndex(),
            material_id.ToIndex()
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

        if (!m_blas.GetGeometries().empty()) {
            for (auto &geometry : m_blas.GetGeometries()) {
                if (!geometry) {
                    continue;
                }

                geometry->SetMaterialIndex(material_index);
            }

            m_blas.SetFlag(AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
        }
    }
}

void BLAS::SetTransform(const Transform &transform)
{
    // TODO: thread safety

    m_transform = transform;

    if (IsInitCalled()) {
        m_blas.SetTransform(m_transform.GetMatrix());
    }
}

void BLAS::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    UInt material_index = 0u;

    if (engine->InitObject(m_material)) {
        material_index = m_material->GetID().value - 1;
    }

    AssertThrow(engine->InitObject(m_mesh));
    
    m_blas.SetTransform(m_transform.GetMatrix());
    m_blas.AddGeometry(std::make_unique<AccelerationGeometry>(
        m_mesh->BuildPackedVertices(),
        m_mesh->BuildPackedIndices(),
        m_entity_id.ToIndex(),
        material_index
    ));

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        return m_blas.Create(engine->GetDevice(), engine->GetInstance());
    });

    HYP_FLUSH_RENDER_QUEUE(engine);

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);
        
        GetEngine()->GetRenderScheduler().Enqueue([this](...) {
            return m_blas.Destroy(GetEngine()->GetDevice());
        });

        m_mesh.Reset();

        HYP_FLUSH_RENDER_QUEUE(GetEngine());
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