#include "DDGI.hpp"

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <camera/OrthoCamera.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <builders/MeshBuilder.hpp>

namespace hyperion::v2 {

DDGI::DDGI(Params &&params)
    : EngineComponentBase(),
      RenderComponent(),
      m_params(std::move(params)),
      m_probe_grid({ .aabb = m_params.aabb })
{
}

DDGI::~DDGI()
{
    Teardown();
}

void DDGI::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](...) {
        auto *engine = GetEngine();

        m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(nullptr));

        //engine->render_scheduler.Enqueue([this, engine](...) {
            CreateDescriptors(engine);

            // add an empty mesh if no bottom level acceleration structures are present
            //if (m_tlas.GetBottomLevelAccelerationStructures().empty()) {
                auto test_mesh = GetEngine()->resources.meshes.Add(MeshBuilder::Cube());
                test_mesh.Init();

                m_tlas.AddBottomLevelAccelerationStructure(GetEngine()->resources.blas.Add(std::make_unique<BLAS>(
                    test_mesh.IncRef(),
                    Transform()
                )));
            //}

            m_tlas.Init(engine);
            m_probe_grid.Init(engine);
            
            SetReady(true);

        //    HYPERION_RETURN_OK;
        //});

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](...) {
            auto *engine = GetEngine();
            
            SetReady(false);
            
            m_renderer_instance.Reset();
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        }));
    }));
}

// called from game thread
void DDGI::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    // add all entities from environment scene
    AssertThrow(GetParent()->GetScene() != nullptr);

    for (auto &it : GetParent()->GetScene()->GetEntities()) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        //if (entity->GetBucket() != BUCKET_TRANSLUCENT) {
        //    continue;
       // }

        if (BucketHasGlobalIllumination(entity->GetBucket())) {
            if (entity->GetMesh() != nullptr) {
                // TODO: use thread-safe pattern
                m_tlas.AddBottomLevelAccelerationStructure(engine->resources.blas.Add(std::make_unique<BLAS>(
                    entity->GetMesh().IncRef(),
                    entity->GetTransform()
                )));
            }
        }
    }
}

void DDGI::OnEntityAdded(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    //if (entity->GetBucket() != BUCKET_TRANSLUCENT) {
    //    return;
    //}

    if (BucketHasGlobalIllumination(entity->GetBucket())) {
        if (entity->GetMesh() != nullptr) {
            m_tlas.AddBottomLevelAccelerationStructure(GetEngine()->resources.blas.Add(std::make_unique<BLAS>(
                entity->GetMesh().IncRef(),
                entity->GetTransform()
            )));
        }
    }
}

void DDGI::OnEntityRemoved(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    // TODO!
}

void DDGI::OnEntityRenderableAttributesChanged(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (BucketHasGlobalIllumination(entity->GetBucket())) {
        if (entity->GetMesh() != nullptr) {
            m_tlas.AddBottomLevelAccelerationStructure(GetEngine()->resources.blas.Add(std::make_unique<BLAS>(
                entity->GetMesh().IncRef(),
                entity->GetTransform()
            )));
        }
    } else {
        // TODO!
    }
}

void DDGI::OnUpdate(Engine *engine, GameCounter::TickUnit delta)
{
    // Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    // TODO: Update transforms of all bottom level acceleration structures?
}

void DDGI::OnRender(Engine *engine, Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    m_tlas.Update(engine);
    
    m_probe_grid.RenderProbes(engine, frame);
    m_probe_grid.ComputeIrradiance(engine, frame);

    // Copy probe borders
}

void DDGI::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

void DDGI::CreateDescriptors(Engine *engine)
{
    DebugLog(LogType::Debug, "Add DDGI descriptors\n");

    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING);
    descriptor_set
        ->GetOrAddDescriptor<renderer::TLASDescriptor>(0)
        ->SetSubDescriptor({
            .element_index = 0u,
            .acceleration_structure = &m_tlas.Get()
        });
}

} // namespace hyperion::v2
