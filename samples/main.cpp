
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
#include <rendering/ProbeSystem.hpp>
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

    virtual void InitRender(Engine *engine) override
    {
        engine->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();
    }

    virtual void InitGame(Engine *engine) override
    {
        Game::InitGame(engine);
        
        m_scene->SetCamera(
            engine->CreateHandle<Camera>(new FirstPersonCamera(
                1920, 1080,
                70.0f,
                0.5f, 30000.0f
            ))
        );

#ifdef HYP_TEST_VCT
        { // voxel cone tracing for indirect light and reflections
            m_scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(
                VoxelConeTracing::Params {
                    BoundingBox(-128, 128)
                }
            );
        }
#endif

        engine->GetWorld()->AddScene(Handle<Scene>(m_scene));

        auto batch = engine->GetAssetManager().CreateBatch();
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
        auto test_model = obj_models["test_model"].Get<Node>();//engine->GetAssetManager().Load<Node>("../data/dump2/sponza.fbom");
        auto cube_obj = obj_models["cube"].Get<Node>();
        auto material_test_obj = obj_models["material"].Get<Node>();

#if 0
        if (auto house = GetScene()->GetRoot().AddChild(obj_models["house"].Get<Node>())) {
            house.Scale(10.0f);
            house.SetName("house");

            int i = 0;

            for (auto &child : house.GetChildren()) {
                if (!child) {
                    continue;
                }

                if (auto ent = child.GetEntity()) {
                    //if (i > 6) {
                    //    std::cout << "entity name = " << ent->GetName() << std::endl;
                    //    break;
                   // }
                    engine->InitObject(ent);
                    ent->CreateBLAS();

                    i++;
                }
            }
        }
#endif

        test_model.Scale(0.35f);

        if (false) {
            auto btn_node = GetUI().GetScene()->GetRoot().AddChild();
            btn_node.SetEntity(engine->CreateHandle<Entity>());
            btn_node.GetEntity()->AddController<UIButtonController>();
            btn_node.Scale(0.01f);
        }

        auto cubemap = engine->CreateHandle<Texture>(new TextureCube(
            engine->GetAssetManager().LoadMany<Texture>(
                "textures/chapel/posx.jpg",
                "textures/chapel/negx.jpg",
                "textures/chapel/posy.jpg",
                "textures/chapel/negy.jpg",
                "textures/chapel/posz.jpg",
                "textures/chapel/negz.jpg"
            )
        ));
        cubemap->GetImage().SetIsSRGB(true);
        engine->InitObject(cubemap);

        if (false) { // hardware skinning



            zombie.Scale(1.25f);
            zombie.Translate(Vector3(0, 0, -9));
            auto zombie_entity = zombie[0].GetEntity();
            zombie_entity->GetController<AnimationController>()->Play(1.0f, LoopMode::REPEAT);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f));
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.0f);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
            zombie_entity->RebuildRenderableAttributes();
            engine->InitObject(zombie_entity);
            zombie_entity->CreateBLAS();
            m_scene->GetRoot().AddChild(zombie);
        }
        
        { // adding lights to scene
            m_sun = engine->CreateHandle<Light>(new DirectionalLight(
                Vector3(-0.5f, 0.75f, 0.0f).Normalize(),
                Color(1.0f, 1.0f, 1.0f),
                150000.0f
            ));
            m_scene->AddLight(m_sun);

            // m_scene->AddLight(engine->CreateHandle<Light>(new PointLight(
            //      Vector3(0.0f, 4.0f, 0.0f),
            //      Color(1.0f, 0.0f, 0.0f),
            //      100000.0f,
            //      30.0f
            // )));

            // m_scene->AddLight(engine->CreateHandle<Light>(new PointLight(
            //      Vector3(5.0f, 4.0f, 12.0f),
            //      Color(0.0f, 0.0f, 1.0f),
            //      50000.0f,
            //      30.0f
            // )));
        }

        //auto tex = engine->GetAssetManager().Load<Texture>("textures/smoke.png");
        //AssertThrow(tex);

        if (false) { // particles test
            auto particle_spawner = engine->CreateHandle<ParticleSpawner>(ParticleSpawnerParams {
                .texture = engine->GetAssetManager().Load<Texture>("textures/smoke.png"),
                .max_particles = 1024u,
                .origin = Vector3(0.0f, 8.0f, -17.0f),
                .lifespan = 8.0f
            });
            engine->InitObject(particle_spawner);

            m_scene->GetEnvironment()->GetParticleSystem()->GetParticleSpawners().Add(std::move(particle_spawner));
        }

        { // adding cubemap rendering with a bounding box
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

        auto skybox_material = engine->CreateHandle<Material>();
        skybox_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));
        skybox_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, std::move(cubemap));
        skybox_material->SetBucket(BUCKET_SKYBOX);
        skybox_material->SetIsDepthWriteEnabled(false);
        skybox_material->SetIsDepthTestEnabled(false);
        skybox_material->SetFaceCullMode(FaceCullMode::FRONT);

        auto skybox_spatial = cube_obj[0].GetEntity();
        skybox_spatial->SetMaterial(std::move(skybox_material));
        skybox_spatial->SetShader(Handle<Shader>(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_SKYBOX)));
        skybox_spatial->RebuildRenderableAttributes();
        m_scene->AddEntity(std::move(skybox_spatial));
        
        for (auto &child : test_model.GetChildren()) {
            if (auto &entity = child.GetEntity()) {
                auto ent = Handle(entity);
                if (engine->InitObject(ent)) {
                    entity->CreateBLAS();
                }
            }
        }

        // add sponza model
        m_scene->GetRoot().AddChild(test_model);

        m_scene->GetFogParams().end_distance = 30000.0f;

#ifdef HYP_TEST_TERRAIN
        { // paged procedural terrain
            if (auto terrain_node = m_scene->GetRoot().AddChild()) {
                terrain_node.SetEntity(engine->CreateHandle<Entity>());
                terrain_node.GetEntity()->AddController<TerrainPagingController>(0xBEEF, Extent3D { 256 } , Vector3(16.0f, 16.0f, 16.0f), 2.0f);
            }
        }
#endif

        { // adding shadow maps
            m_scene->GetEnvironment()->AddRenderComponent<ShadowRenderer>(
                Handle<Light>(m_sun),
                BoundingBox(Vector3(-500, -10, -500), Vector3(500, 300, 500))//test_model.GetWorldAABB()
            );
        }

        if (auto monkey = engine->GetAssetManager().Load<Node>("models/monkey/monkey.obj")) {
            monkey.SetName("monkey");
            auto monkey_entity = monkey[0].GetEntity();
            monkey_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED, false);
            monkey_entity->AddController<ScriptedController>(
                engine->GetAssetManager().Load<Script>("scripts/examples/controller.hypscript")
            );
            monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.0f);
            monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
            monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, Handle<Texture>());
            monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Handle<Texture>());
            monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
            monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
           //monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_TRANSMISSION, 0.95f);
            monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
           // monkey_entity->GetMaterial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            //monkey_entity->GetMaterial()->SetIsAlphaBlended(true);
            monkey_entity->RebuildRenderableAttributes();
            monkey.Translate(Vector3(40, 250.5f, 0));
            monkey.Scale(12.0f);
            engine->InitObject(monkey_entity);
            monkey_entity->CreateBLAS();
            m_scene->GetRoot().AddChild(monkey);

            monkey[0].GetEntity()->AddController<RigidBodyController>(
                UniquePtr<physics::BoxPhysicsShape>::Construct(BoundingBox(-1, 1)),
                physics::PhysicsMaterial { .mass = 1.0f }
            );
        }

        // add a plane physics shape
        auto plane = engine->CreateHandle<Entity>();
        plane->SetName("Plane entity");
        plane->SetTranslation(Vector3(0, 15, 8));
        // plane->SetMesh(engine->CreateHandle<Mesh>(MeshBuilder::Quad()));
        plane->SetScale(20.0f);
        plane->SetMaterial(engine->CreateHandle<Material>());
        plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.0f);
        plane->SetRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(-90.0f)));
        // plane->SetShader(Handle<Shader>(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD)));
        GetScene()->AddEntity(Handle<Entity>(plane));
        plane->CreateBLAS();
        plane->AddController<RigidBodyController>(
            UniquePtr<physics::PlanePhysicsShape>::Construct(Vector4(0, 1, 0, 1)),
            physics::PhysicsMaterial { .mass = 0.0f }
        );
        plane->GetController<RigidBodyController>()->GetRigidBody()->SetIsKinematic(false);
    }

    virtual void Teardown(Engine *engine) override
    {
        Game::Teardown(engine);
    }

    bool svo_ready_to_build = false;

    virtual void OnFrameBegin(Engine *engine, Frame *frame) override
    {
        engine->render_state.BindScene(m_scene.Get());
    }

    virtual void OnFrameEnd(Engine *engine, Frame *) override
    {
        engine->render_state.UnbindScene();
    }

    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) override
    {
        timer += delta;

        m_ui.Update(engine, delta);

        HandleCameraMovement();

        if (auto house = GetScene()->GetRoot().Select("house")) {
            //house.Rotate(Quaternion(Vector3(0, 1, 0), 0.1f * delta));
        }

        #if 0 // bad performance on large meshes. need bvh
        //if (input_manager->IsButtonDown(MOUSE_BUTTON_LEFT) && ray_cast_timer > 1.0f) {
        //    ray_cast_timer = 0.0f;
            const auto &mouse_position = GetInputManager()->GetMousePosition();

            const auto mouse_x = mouse_position.x.load();
            const auto mouse_y = mouse_position.y.load();

            const auto mouse_world = m_scene->GetCamera()->TransformScreenToWorld(
                Vector2(
                    mouse_x / static_cast<float>(GetInputManager()->GetWindow()->width),
                    mouse_y / static_cast<float>(GetInputManager()->GetWindow()->height)
                )
            );

            auto ray_direction = mouse_world.Normalized() * -1.0f;

            // std::cout << "ray direction: " << ray_direction << "\n";

            Ray ray { m_scene->GetCamera()->GetTranslation(), Vector3(ray_direction) };
            RayTestResults results;

            if (engine->GetWorld()->GetOctree().TestRay(ray, results)) {
                // std::cout << "hit with aabb : " << results.Front().hitpoint << "\n";
                RayTestResults triangle_mesh_results;

                for (const auto &hit : results) {
                    // now ray test each result as triangle mesh to find exact hit point
                    if (auto lookup_result = engine->GetObjectSystem().Lookup<Entity>(Entity::ID { TypeID::ForType<Entity>(), hit.id })) {
                        if (auto &mesh = lookup_result->GetMesh()) {
                            ray.TestTriangleList(
                                mesh->GetVertices(),
                                mesh->GetIndices(),
                                lookup_result->GetTransform(),
                                lookup_result->GetID().value,
                                triangle_mesh_results
                            );
                        }
                    }
                }

                if (!triangle_mesh_results.Empty()) {
                    const auto &mesh_hit = triangle_mesh_results.Front();

                    if (auto sphere = m_scene->GetRoot().Select("monkey")) {
                        sphere.SetLocalTranslation(mesh_hit.hitpoint);
                        sphere.SetLocalRotation(Quaternion::LookAt((m_scene->GetCamera()->GetTranslation() - mesh_hit.hitpoint).Normalized(), Vector3::UnitY()));
                    }
                }
            }
        #endif
    }

    virtual void OnInputEvent(Engine *engine, const SystemEvent &event) override
    {
        Game::OnInputEvent(engine, event);

        if (event.GetType() == SystemEventType::EVENT_FILE_DROP) {
            if (const FilePath *path = event.GetEventData().TryGet<FilePath>()) {
                if (auto reader = path->Open()) {

                    auto batch = engine->GetAssetManager().CreateBatch();
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
    void HandleCameraMovement()
    {
        if (m_input_manager->IsKeyDown(KEY_W)) {
            if (m_scene && m_scene->GetCamera()) {
                m_scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_FORWARD,
                        .amount = 1.0f
                    }
                });
            }
        }

        if (m_input_manager->IsKeyDown(KEY_S)) {
            if (m_scene && m_scene->GetCamera()) {
                m_scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_BACKWARD,
                        .amount = 1.0f
                    }
                });
            }
        }

        if (m_input_manager->IsKeyDown(KEY_A)) {
            if (m_scene && m_scene->GetCamera()) {
                m_scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_LEFT,
                        .amount = 1.0f
                    }
                });
            }
        }

        if (m_input_manager->IsKeyDown(KEY_D)) {
            if (m_scene && m_scene->GetCamera()) {
                m_scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_RIGHT,
                        .amount = 1.0f
                    }
                });
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

    RefCountedPtr<Application> application(new SDLApplication);
    application->SetCurrentWindow(application->CreateSystemWindow("Hyperion Engine", 1920, 1080));
    
    SystemEvent event;

    auto *engine = new Engine(application, "My app");
    auto *my_game = new MyGame(application);

    engine->Initialize();

    engine->shader_manager.SetShader(
        ShaderKey::BASIC_VEGETATION,
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("Vegetation", ShaderProps { }))
    );

    engine->shader_manager.SetShader(
        ShaderKey::BASIC_UI,
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("UIObject", ShaderProps { }))
    );

    engine->shader_manager.SetShader(
        ShaderKey::DEBUG_AABB,
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("DebugAABB", ShaderProps { }))
    );

    engine->shader_manager.SetShader(
        ShaderKey::BASIC_FORWARD,
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("Forward", ShaderProps { }))
    );

    engine->shader_manager.SetShader(
        ShaderKey::TERRAIN,
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("Terrain", ShaderProps { }))
    );

    engine->shader_manager.SetShader(
        ShaderManager::Key::BASIC_SKYBOX,
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("Skybox", ShaderProps { }))
    );

    my_game->Init(engine);

    engine->Compile();
    
    engine->game_thread.Start(engine, my_game);

    UInt num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    while (engine->IsRenderLoopActive()) {
        // input manager stuff
        while (application->PollEvent(event)) {
            my_game->HandleEvent(engine, std::move(event));
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
        
        engine->RenderNextFrame(my_game);
    }

    delete my_game;
    delete engine;

    return 0;
}
