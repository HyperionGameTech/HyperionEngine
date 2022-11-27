
#include "system/SdlSystem.hpp"
#include "system/Debug.hpp"

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <core/lib/Proc.hpp>

#include <Engine.hpp>
#include <scene/Node.hpp>
#include <rendering/Atomics.hpp>
#include <scene/animation/Bone.hpp>
#include <rendering/rt/AccelerationStructureBuilder.hpp>
#include <rendering/rt/ProbeSystem.hpp>
#include <rendering/post_fx/FXAA.hpp>
#include <rendering/post_fx/Tonemap.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/AnimationController.hpp>
#include <scene/controllers/AabbDebugController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/paging/BasicPagingController.hpp>
#include <scene/controllers/ScriptedController.hpp>
#include <scene/controllers/physics/RigidBodyController.hpp>
#include <ui/controllers/UIButtonController.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>
#include <GameThread.hpp>
#include <Game.hpp>

#include <rendering/rt/BlurRadiance.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>

#include <ui/UIText.hpp>

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>
#include <asset/serialization/fbom/marshals/SceneMarshal.hpp>

#include <terrain/controllers/TerrainPagingController.hpp>

#include <rendering/vct/VoxelConeTracing.hpp>
#include <rendering/SparseVoxelOctree.hpp>

#include <util/fs/FsUtil.hpp>
#include <util/img/Bitmap.hpp>

#include <scene/NodeProxy.hpp>

#include <scene/camera/FirstPersonCamera.hpp>
#include <scene/camera/FollowCamera.hpp>

#include <util/MeshBuilder.hpp>

#include "util/Profile.hpp"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cmath>
#include <thread>
#include <list>

#include "rendering/RenderEnvironment.hpp"
#include "rendering/CubemapRenderer.hpp"
#include "rendering/UIRenderer.hpp"

#include <rendering/ParticleSystem.hpp>

#include <script/ScriptBindings.hpp>

#include <util/UTF8.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

using namespace hyperion;
using namespace hyperion::v2;

//#define HYP_TEST_VCT
//#define HYP_TEST_TERRAIN

namespace hyperion::v2 {
    
class MyGame : public Game
{

public:
    Handle<Light> m_sun;

    MyGame(RefCountedPtr<Application> application)
        : Game(application)
    {
    }

    virtual void InitRender() override
    {
        //Engine::Get()->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();
    }

    virtual void InitGame() override
    {
        Game::InitGame();
        
        m_scene->SetCamera(
            CreateObject<Camera>(
                70.0f,
                1920, 1080,
                0.5f, 30000.0f
            )
        );

        m_scene->GetCamera()->SetCameraController(UniquePtr<FollowCameraController>::Construct(
            Vector3(0.0f), Vector3(0.0f, 150.0f, -35.0f)
        ));

#ifdef HYP_TEST_VCT
        { // voxel cone tracing for indirect light and reflections
            m_scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(
                VoxelConeTracing::Params {
                    BoundingBox(-128, 128)
                }
            );
        }
#endif
        Engine::Get()->GetWorld()->AddScene(Handle<Scene>(m_scene));

        auto batch = Engine::Get()->GetAssetManager().CreateBatch();
        batch.Add<Node>("zombie", "models/ogrexml/dragger_Body.mesh.xml");
        batch.Add<Node>("house", "models/house.obj");
        batch.Add<Node>("test_model", "models/sponza/sponza.obj"); //"San_Miguel/san-miguel-low-poly.obj");
        batch.Add<Node>("cube", "models/cube.obj");
        batch.Add<Node>("material", "models/material_sphere/material_sphere.obj");
        batch.Add<Node>("grass", "models/grass/grass.obj");
        
        // batch.Add<Node>("monkey_fbx", "models/monkey.fbx");
        batch.LoadAsync();
        auto obj_models = batch.AwaitResults();

        auto zombie = obj_models["zombie"].Get<Node>();
        auto test_model = obj_models["test_model"].Get<Node>();//Engine::Get()->GetAssetManager().Load<Node>("../data/dump2/sponza.fbom");
        auto cube_obj = obj_models["cube"].Get<Node>();
        auto material_test_obj = obj_models["material"].Get<Node>();

        test_model.Scale(0.35f);

        {
            int i = 0;

            for (auto &child : test_model.GetChildren()) {
                if (!child) {
                    continue;
                }

                if (auto ent = child.GetEntity()) {
                    InitObject(ent);
                    // ent->CreateBLAS();

                    if (!ent->GetMesh()) {
                        continue;
                    }

                    Array<Vector3> vertices;
                    vertices.Reserve(ent->GetMesh()->GetVertices().size());

                    for (auto &vertex : ent->GetMesh()->GetVertices()) {
                        vertices.PushBack(vertex.GetPosition());
                    }

                    ent->AddController<RigidBodyController>(
                        UniquePtr<physics::ConvexHullPhysicsShape>::Construct(vertices),
                        physics::PhysicsMaterial { .mass = 0.0f }
                    );
                    // ent->GetController<RigidBodyController>()->GetRigidBody()->SetIsKinematic(true);

                    i++;
                }
            }
        }

        if (false) {
            auto btn_node = GetUI().GetScene()->GetRoot().AddChild();
            btn_node.SetEntity(CreateObject<Entity>());
            btn_node.GetEntity()->AddController<UIButtonController>();

            if (UIButtonController *controller = btn_node.GetEntity()->GetController<UIButtonController>()) {
                controller->SetScript(Engine::Get()->GetAssetManager().Load<Script>("scripts/examples/ui_controller.hypscript"));
            }

            btn_node.Scale(0.01f);
        }

        auto cubemap = CreateObject<Texture>(TextureCube(
            Engine::Get()->GetAssetManager().LoadMany<Texture>(
                "textures/chapel/posx.jpg",
                "textures/chapel/negx.jpg",
                "textures/chapel/posy.jpg",
                "textures/chapel/negy.jpg",
                "textures/chapel/posz.jpg",
                "textures/chapel/negz.jpg"
            )
        ));
        cubemap->GetImage().SetIsSRGB(true);
        InitObject(cubemap);

        if (true) { // hardware skinning
            zombie.Scale(1.25f);
            zombie.Translate(Vector3(0, 0, -9));
            auto zombie_entity = zombie[0].GetEntity();
            zombie_entity->GetController<AnimationController>()->Play(1.0f, LoopMode::REPEAT);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Color(1.0f, 0.0f, 0.0f, 1.0f));
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.0f);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
            zombie_entity->RebuildRenderableAttributes();
            InitObject(zombie_entity);
            zombie_entity->CreateBLAS();
            m_scene->GetRoot().AddChild(zombie);
            
            auto zomb2 = CreateObject<Entity>();
            zomb2->SetMesh(zombie_entity->GetMesh());
            zomb2->SetTranslation(Vector3(0, 20, 0));
            zomb2->SetScale(Vector3(20.0f));
            zomb2->SetShader(zombie_entity->GetShader());
            zomb2->SetMaterial(CreateObject<Material>());//zombie_entity->GetMaterial());
            zomb2->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Color(0.0f, 1.0f, 0.0f, 1.0f));
            zomb2->SetName("FOOBAR ZOMBO");
            zomb2->SetSkeleton(zombie_entity->GetSkeleton());
            zomb2->RebuildRenderableAttributes();

            InitObject(zomb2);
            m_scene->AddEntity(zomb2);

            DebugLog(LogType::Debug, "FOOBAR ZOMBIE 1 ID : %u\n", zombie_entity->GetID().Value());
            DebugLog(LogType::Debug, "FOOBAR ZOMBO ID : %u\n", zomb2->GetID().Value());
        }
        
        { // adding lights to scene
            m_sun = CreateObject<Light>(DirectionalLight(
                Vector3(-0.5f, 1.0f, 0.1f).Normalize(),
                Color(1.0f, 1.0f, 1.0f),
                500000.0f
            ));

            m_scene->AddLight(m_sun);
        }

        //auto tex = Engine::Get()->GetAssetManager().Load<Texture>("textures/smoke.png");
        //AssertThrow(tex);

        if (true) { // particles test
            auto particle_spawner = CreateObject<ParticleSpawner>(ParticleSpawnerParams {
                .texture = Engine::Get()->GetAssetManager().Load<Texture>("textures/smoke.png"),
                .max_particles = 1024u,
                .origin = Vector3(0.0f, 8.0f, -17.0f),
                .lifespan = 8.0f
            });

            InitObject(particle_spawner);

            m_scene->GetEnvironment()->GetParticleSystem()->GetParticleSpawners().Add(std::move(particle_spawner));
        }

        if (false) { // adding cubemap rendering with a bounding box
            m_scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
                Extent2D { 512, 512 },
                test_model.GetWorldAABB(),//BoundingBox(Vector3(-128, -8, -128), Vector3(128, 25, 128)),
                FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
            );
        }

        { // allow ui rendering
            m_scene->GetEnvironment()->AddRenderComponent<UIRenderer>(Handle<Scene>(GetUI().GetScene()));
        }

        cube_obj.Scale(50.0f);

        auto skybox_material = CreateObject<Material>();
        skybox_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));
        skybox_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, std::move(cubemap));
        skybox_material->SetBucket(BUCKET_SKYBOX);
        skybox_material->SetIsDepthWriteEnabled(false);
        skybox_material->SetIsDepthTestEnabled(false);
        skybox_material->SetFaceCullMode(FaceCullMode::FRONT);

        auto skybox_spatial = cube_obj[0].GetEntity();
        skybox_spatial->SetMaterial(std::move(skybox_material));
        skybox_spatial->SetShader(Handle<Shader>(Engine::Get()->shader_manager.GetShader(ShaderManager::Key::BASIC_SKYBOX)));
        skybox_spatial->RebuildRenderableAttributes();
        m_scene->AddEntity(std::move(skybox_spatial));
        
        for (auto &child : test_model.GetChildren()) {
            if (const Handle<Entity> &entity = child.GetEntity()) {
                Handle<Entity> ent = entity;

                if (InitObject(ent)) {
                    entity->CreateBLAS();
                }
            }
        }

        // add sponza model
        m_scene->GetRoot().AddChild(test_model);

        m_scene->GetFogParams().end_distance = 40000.0f;

#ifdef HYP_TEST_TERRAIN
        { // paged procedural terrain
            if (auto terrain_node = m_scene->GetRoot().AddChild()) {
                terrain_node.SetEntity(CreateObject<Entity>());
                terrain_node.GetEntity()->AddController<TerrainPagingController>(0xBEEF, Extent3D { 256 } , Vector3(16.0f, 16.0f, 16.0f), 2.0f);
            }
        }
#endif

        { // adding shadow maps
            m_scene->GetEnvironment()->AddRenderComponent<ShadowRenderer>(
                Handle<Light>(m_sun),
                test_model.GetWorldAABB()
            );
        }

        if (auto monkey = Engine::Get()->GetAssetManager().Load<Node>("models/monkey/monkey.obj")) {
            monkey.SetName("monkey");
            auto monkey_entity = monkey[0].GetEntity();
            monkey_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED, false);
            // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.0f);
            // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
            // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, Handle<Texture>());
            // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Handle<Texture>());
            // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
            // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
           //monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_TRANSMISSION, 0.95f);
            // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
           // monkey_entity->GetMaterial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            //monkey_entity->GetMaterial()->SetIsAlphaBlended(true);
            monkey_entity->RebuildRenderableAttributes();
            monkey.Translate(Vector3(0, 250.5f, 0));
            monkey.Scale(12.0f);
            InitObject(monkey_entity);

            monkey_entity->AddController<ScriptedController>(
                Engine::Get()->GetAssetManager().Load<Script>("scripts/examples/controller.hypscript")
            );

            monkey_entity->CreateBLAS();
            m_scene->GetRoot().AddChild(monkey);

            monkey[0].GetEntity()->AddController<RigidBodyController>(
                UniquePtr<physics::BoxPhysicsShape>::Construct(BoundingBox(-1, 1)),
                physics::PhysicsMaterial { .mass = 1.0f }
            );
        }

        auto mh = Engine::Get()->GetAssetManager().Load<Node>("models/mh/mh1.obj");
        mh.SetName("mh_model");
        mh.Scale(5.0f);
        for (auto &mh_child : mh.GetChildren()) {
            //mh_child.SetEntity(Handle<Entity>::empty);

            if (auto entity = mh_child.GetEntity()) {
                entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
                entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
                entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.0f, 4.0f, 0.0f, 1.0f));
                entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.01f);
                entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
            }
        }
        GetScene()->GetRoot().AddChild(mh);


        NodeProxy tree = Engine::Get()->GetAssetManager().Load<Node>("models/conifer/Conifer_Low.obj");
        tree.SetName("tree");
        tree.Scale(5.0f);
        if (auto needles = tree.Select("Needles")) {
            if (needles.GetEntity() && needles.GetEntity()->GetMaterial()) {
                needles.GetEntity()->GetMaterial()->SetFaceCullMode(FaceCullMode::NONE);
                //needles.GetEntity()->GetMaterial()->SetBucket(BUCKET_TRANSLUCENT);
            }
        }

        for (auto &child : tree.GetChildren()) {
            if (child.GetName() == "BlueSpruceBark") {
                continue;
            }

            if (child.GetEntity()) {
                //child.GetEntity()->SetShader(Engine::Get()->shader_manager.GetShader(ShaderManager::Key::BASIC_VEGETATION));
            }
        }

        GetScene()->GetRoot().AddChild(tree);


        if (false) {
            // add a plane physics shape
            auto plane = CreateObject<Entity>();
            plane->SetName("Plane entity");
            plane->SetTranslation(Vector3(0, 12, 8));
            plane->SetMesh(MeshBuilder::Quad());
            plane->GetMesh()->SetVertexAttributes(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes);
            plane->SetScale(250.0f);
            plane->SetMaterial(CreateObject<Material>());
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.0f, 0.8f, 1.0f, 1.0f));
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.075f);
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_UV_SCALE, Vector2(2.0f));
            plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, Engine::Get()->GetAssetManager().Load<Texture>("textures/water.jpg"));
            // plane->GetMaterial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            plane->SetRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(-90.0f)));
            plane->SetShader(Handle<Shader>(Engine::Get()->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD)));
            plane->RebuildRenderableAttributes();
            GetScene()->AddEntity(Handle<Entity>(plane));
            plane->CreateBLAS();
            plane->AddController<RigidBodyController>(
                UniquePtr<physics::PlanePhysicsShape>::Construct(Vector4(0, 1, 0, 1)),
                physics::PhysicsMaterial { .mass = 0.0f }
            );
            plane->GetController<RigidBodyController>()->GetRigidBody()->SetIsKinematic(false);
        }
    }

    virtual void Teardown() override
    {
        Game::Teardown();
    }

    bool svo_ready_to_build = false;

    virtual void OnFrameBegin(Frame *) override
    {
        Engine::Get()->render_state.BindScene(m_scene.Get());
    }

    virtual void OnFrameEnd(Frame *) override
    {
        Engine::Get()->render_state.UnbindScene();
    }

    virtual void Logic(GameCounter::TickUnit delta) override
    {
        timer += delta;

        m_ui.Update(delta);

        HandleCameraMovement(delta);

        GetScene()->GetCamera()->SetTarget(GetScene()->GetRoot().Select("mh_model").GetWorldTranslation());

        m_sun->SetPosition(Vector3(MathUtil::Sin(timer * 0.002f), MathUtil::Cos(timer * 0.002f), -MathUtil::Sin(timer * 0.002f)).Normalize());

        if (auto house = GetScene()->GetRoot().Select("house")) {
            //house.Rotate(Quaternion(Vector3(0, 1, 0), 0.1f * delta));
        }

        #if 0 // bad performance on large meshes. need bvh
        if (GetInputManager()->IsButtonDown(MOUSE_BUTTON_LEFT) && ray_cast_timer > 1.0f) {
            ray_cast_timer = 0.0f;
            const auto &mouse_position = GetInputManager()->GetMousePosition();

            const Int mouse_x = mouse_position.GetX();
            const Int mouse_y = mouse_position.GetY();

            const auto mouse_world = m_scene->GetCamera()->TransformScreenToWorld(
                Vector2(
                    mouse_x / Float(GetInputManager()->GetWindow()->GetExtent().width),
                    mouse_y / Float(GetInputManager()->GetWindow()->GetExtent().height)
                )
            );

            auto ray_direction = mouse_world.Normalized() * -1.0f;

            // std::cout << "ray direction: " << ray_direction << "\n";

            Ray ray { m_scene->GetCamera()->GetTranslation(), Vector3(ray_direction) };
            RayTestResults results;

            if (Engine::Get()->GetWorld()->GetOctree().TestRay(ray, results)) {
                // std::cout << "hit with aabb : " << results.Front().hitpoint << "\n";
                RayTestResults triangle_mesh_results;

                for (const auto &hit : results) {
                    // now ray test each result as triangle mesh to find exact hit point
                    Handle<Entity> entity(Entity::ID { hit.id });

                    if (entity) {
                        entity->AddController<AABBDebugController>();

                        if (auto &mesh = entity->GetMesh()) {
                            ray.TestTriangleList(
                                mesh->GetVertices(),
                                mesh->GetIndices(),
                                entity->GetTransform(),
                                entity->GetID().value,
                                triangle_mesh_results
                            );
                        }
                    }
                }

                if (!triangle_mesh_results.Empty()) {
                    const auto &mesh_hit = triangle_mesh_results.Front();

                    if (auto target = m_scene->GetRoot().Select("monkey")) {
                        target.SetLocalTranslation(mesh_hit.hitpoint);
                        target.SetLocalRotation(Quaternion::LookAt((m_scene->GetCamera()->GetTranslation() - mesh_hit.hitpoint).Normalized(), Vector3::UnitY()));
                    }
                }
            }
        } else {
            ray_cast_timer += delta;
        }
        #endif
    }

    virtual void OnInputEvent(const SystemEvent &event) override
    {
        Game::OnInputEvent(event);

        if (event.GetType() == SystemEventType::EVENT_FILE_DROP) {
            if (const FilePath *path = event.GetEventData().TryGet<FilePath>()) {
                if (auto reader = path->Open()) {

                    auto batch = Engine::Get()->GetAssetManager().CreateBatch();
                    batch.Add<Node>("dropped_object", *path);
                    batch.LoadAsync();

                    auto results = batch.AwaitResults();

                    if (results.Any()) {
                        for (auto &it : results) {
                            GetScene()->GetRoot().AddChild(it.second.Get<Node>());
                        }
                    }

                    reader.Close();
                }
            }
        }
    }

    // not overloading; just creating a method to handle camera movement
    void HandleCameraMovement(GameCounter::TickUnit delta)
    {
        if (auto mh_model = GetScene()->GetRoot().Select("mh_model")) {
            constexpr Float speed = 0.75f;

            mh_model.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetDirection(), GetScene()->GetCamera()->GetUpVector()));

            if (m_input_manager->IsKeyDown(KEY_W)) {
                mh_model.Translate(GetScene()->GetCamera()->GetDirection() * delta * speed);
            }

            if (m_input_manager->IsKeyDown(KEY_S)) {
                mh_model.Translate(GetScene()->GetCamera()->GetDirection() * -1.0f * delta * speed);
            }

            if (m_input_manager->IsKeyDown(KEY_A)) {
                mh_model.Translate((GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector())) * -1.0f * delta * speed);
            }

            if (m_input_manager->IsKeyDown(KEY_D)) {
                mh_model.Translate((GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector())) * delta * speed);
            }
        }
    }

    std::unique_ptr<Node> zombie;
    GameCounter::TickUnit timer{};
    GameCounter::TickUnit ray_cast_timer{};

};
} // namespace hyperion::v2



int main()
{
    using namespace hyperion::renderer;

    RefCountedPtr<Application> application(new SDLApplication("My Application"));
    application->SetCurrentWindow(application->CreateSystemWindow("Hyperion Engine", 1920, 1080));
    
    SystemEvent event;

    auto *engine = Engine::Get();
    auto *my_game = new MyGame(application);

    Engine::Get()->Initialize(application);

    Engine::Get()->shader_manager.SetShader(
        ShaderKey::BASIC_VEGETATION,
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("Vegetation", ShaderProps { }))
    );

    Engine::Get()->shader_manager.SetShader(
        ShaderKey::BASIC_UI,
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("UIObject", ShaderProps { }))
    );

    Engine::Get()->shader_manager.SetShader(
        ShaderKey::DEBUG_AABB,
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("DebugAABB", ShaderProps { }))
    );

    Engine::Get()->shader_manager.SetShader(
        ShaderKey::BASIC_FORWARD,
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("Forward", ShaderProps(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes)))
    );

    Engine::Get()->shader_manager.SetShader(
        ShaderKey::BASIC_FORWARD_SKINNED,
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("Forward", ShaderProps(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes, { "SKINNING" })))
    );

    Engine::Get()->shader_manager.SetShader(
        ShaderKey::TERRAIN,
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("Terrain", ShaderProps(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes)))
    );

    Engine::Get()->shader_manager.SetShader(
        ShaderManager::Key::BASIC_SKYBOX,
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("Skybox", ShaderProps { }))
    );

    my_game->Init();

    Engine::Get()->Compile();
    
    Engine::Get()->game_thread.Start(my_game);

    UInt num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    while (Engine::Get()->IsRenderLoopActive()) {
        // input manager stuff
        while (application->PollEvent(event)) {
            my_game->HandleEvent(std::move(event));
        }

        counter.NextTick();
        delta_time_accum += counter.delta;
        num_frames++;

        if (num_frames >= 250) {
            DebugLog(
                LogType::Debug,
                "Render FPS: %f\n",
                1.0f / (delta_time_accum / static_cast<float>(num_frames))
            );

            delta_time_accum = 0.0f;
            num_frames = 0;
        }
        
        Engine::Get()->RenderNextFrame(my_game);
    }

    delete my_game;
    delete Engine::Get();

    return 0;
}
