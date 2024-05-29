/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/BLAS.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(SetBLASMaterial) : renderer::RenderCommand
{
    BLASRef         blas;
    ID<Material>    material_id;

    RENDER_COMMAND(SetBLASMaterial)(BLASRef blas, ID<Material> material_id)
        : blas(std::move(blas)),
          material_id(material_id)
    {
    }

    virtual ~RENDER_COMMAND(SetBLASMaterial)() override = default;

    virtual Result operator()() override
    {
        if (!blas.IsValid()) {
            return { Result::RENDERER_ERR, "Invalid BLAS" };
        }

        const uint material_index = material_id.ToIndex();

        if (!blas->GetGeometries().Empty()) {
            for (AccelerationGeometryRef &geometry : blas->GetGeometries()) {
                if (!geometry) {
                    continue;
                }

                geometry->SetMaterialIndex(material_index);
            }

            blas->SetFlag(AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
        }

        return { };
    }
};

struct RENDER_COMMAND(SetBLASTransform) : renderer::RenderCommand
{
    BLASRef blas;
    Matrix4 transform;

    RENDER_COMMAND(SetBLASTransform)(BLASRef blas, const Matrix4 &transform)
        : blas(std::move(blas)),
          transform(transform)
    {
    }

    virtual ~RENDER_COMMAND(SetBLASTransform)() override = default;

    virtual Result operator()() override
    {
        if (!blas.IsValid()) {
            return { Result::RENDERER_ERR, "Invalid BLAS" };
        }
        
        blas->SetTransform(transform);

        return { };
    }
};

struct RENDER_COMMAND(SetBLASMesh) : renderer::RenderCommand
{
    BLASRef         blas;
    Handle<Mesh>    mesh;
    ID<Entity>      entity_id;
    ID<Material>    material_id;

    RENDER_COMMAND(SetBLASMesh)(BLASRef blas, Handle<Mesh> mesh, ID<Entity> entity_id, ID<Material> material_id)
        : blas(std::move(blas)),
          mesh(std::move(mesh)),
          entity_id(entity_id),
          material_id(material_id)
    {
    }

    virtual ~RENDER_COMMAND(SetBLASMesh)() override = default;

    virtual Result operator()() override
    {
        if (!blas.IsValid()) {
            return { Result::RENDERER_ERR, "Invalid BLAS" };
        }
        

        if (!blas->GetGeometries().Empty()) {
            uint size = uint(blas->GetGeometries().Size());

            while (size) {
                blas->RemoveGeometry(--size);
            }
        }

        if (mesh) {
            AccelerationGeometryRef geometry = MakeRenderObject<AccelerationGeometry>(
                mesh->BuildPackedVertices(),
                mesh->BuildPackedIndices(),
                entity_id.ToIndex(),
                material_id.ToIndex()
            );

            Result create_geometry_result = geometry->Create(g_engine->GetGPUDevice(), g_engine->GetGPUInstance());

            if (!create_geometry_result) {
                return create_geometry_result;
            }

            blas->AddGeometry(std::move(geometry));
        }

        return { };
    }
};

#pragma endregion Render commands

BLAS::BLAS(
    ID<Entity> entity_id,
    Handle<Mesh> mesh,
    Handle<Material> material,
    const Transform &transform
) : BasicObject(),
    m_entity_id(entity_id),
    m_mesh(std::move(mesh)),
    m_material(std::move(material)),
    m_transform(transform),
    m_blas(MakeRenderObject<renderer::BottomLevelAccelerationStructure>())
{
}

BLAS::~BLAS()
{
    SafeRelease(std::move(m_blas));
}

void BLAS::SetMesh(Handle<Mesh> mesh)
{
    m_mesh = std::move(mesh);
    InitObject(m_mesh);

    PUSH_RENDER_COMMAND(
        SetBLASMesh,
        m_blas,
        m_mesh,
        m_entity_id,
        m_material.GetID()
    );
}

void BLAS::SetMaterial(Handle<Material> material)
{
    m_material = std::move(material);
    InitObject(m_material);

    if (IsInitCalled()) {
        PUSH_RENDER_COMMAND(
            SetBLASMaterial,
            m_blas,
            m_material.GetID()
        );
    }
}

void BLAS::SetTransform(const Transform &transform)
{
    m_transform = transform;

    if (IsInitCalled()) {
        PUSH_RENDER_COMMAND(
           SetBLASTransform,
           m_blas,
           m_transform.GetMatrix()
        );
    }
}

void BLAS::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    InitObject(m_material);
    AssertThrow(InitObject(m_mesh));
    
    m_blas->SetTransform(m_transform.GetMatrix());
    m_blas->AddGeometry(MakeRenderObject<AccelerationGeometry>(
        m_mesh->BuildPackedVertices(),
        m_mesh->BuildPackedIndices(),
        m_entity_id.ToIndex(),
        m_material.GetID().ToIndex()
    ));

    DeferCreate(m_blas, g_engine->GetGPUDevice(), g_engine->GetGPUInstance());

    // @TODO: Refactor so this sync is not needed
    HYP_SYNC_RENDER();

    SetReady(true);
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
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    if (!NeedsUpdate()) {
        return;
    }
    
    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(g_engine->GetGPUInstance(), out_was_rebuilt));
#endif
}

} // namespace hyperion