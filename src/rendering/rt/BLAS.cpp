#include "BLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(CreateBLAS) : renderer::RenderCommand
{
    renderer::BottomLevelAccelerationStructure *blas;

    RENDER_COMMAND(CreateBLAS)(renderer::BottomLevelAccelerationStructure *blas)
        : blas(blas)
    {
    }

    virtual Result operator()()
    {
        return blas->Create(g_engine->GetGPUDevice(), g_engine->GetGPUInstance());
    }
};

struct RENDER_COMMAND(DestroyBLAS) : renderer::RenderCommand
{
    renderer::BottomLevelAccelerationStructure *blas;

    RENDER_COMMAND(DestroyBLAS)(renderer::BottomLevelAccelerationStructure *blas)
        : blas(blas)
    {
    }

    virtual Result operator()()
    {
        return blas->Destroy(g_engine->GetGPUDevice());
    }
};

BLAS::BLAS(
    IDBase entity_id,
    Handle<Mesh> &&mesh,
    Handle<Material> &&material,
    const Transform &transform
) : BasicObject(),
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

    if (m_mesh) {
        InitObject(m_mesh);

        ID<Material> material_id = m_material
            ? m_material->GetID()
            : ID<Material> { };
        
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
        
        const auto material_index = material_id.ToIndex();

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

void BLAS::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    UInt material_index = 0;

    if (InitObject(m_material)) {
        material_index = m_material->GetID().ToIndex();
    }

    AssertThrow(InitObject(m_mesh));
    
    m_blas.SetTransform(m_transform.GetMatrix());
    m_blas.AddGeometry(std::make_unique<AccelerationGeometry>(
        m_mesh->BuildPackedVertices(),
        m_mesh->BuildPackedIndices(),
        m_entity_id.ToIndex(),
        material_index
    ));

    PUSH_RENDER_COMMAND(CreateBLAS, &m_blas);

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        PUSH_RENDER_COMMAND(DestroyBLAS, &m_blas);

        HYP_SYNC_RENDER();
    });
}

void BLAS::Update()
{
    // no-op
}

void BLAS::UpdateRender(
    
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
    
    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(g_engine->GetGPUInstance(), out_was_rebuilt));
#endif
}

} // namespace hyperion::v2