
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
#include <rendering/EnvGrid.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/AnimationController.hpp>
#include <scene/controllers/AabbDebugController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/paging/BasicPagingController.hpp>
#include <scene/controllers/ScriptedController.hpp>
#include <scene/controllers/physics/RigidBodyController.hpp>
#include <ui/controllers/UIButtonController.hpp>
#include <ui/controllers/UIContainerController.hpp>
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

#include <scene/terrain/controllers/TerrainPagingController.hpp>
#include <scene/skydome/controllers/SkydomeController.hpp>

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

namespace hyperion::v2 {
    
class MyGame : public Game
{

public:
    Handle<Light> m_sun;
    Array<Handle<Light>> m_point_lights;

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

        if (Engine::Get()->GetConfig().Get(CONFIG_VOXEL_GI)) { // voxel cone tracing for indirect light and reflections
            m_scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(VoxelConeTracing::Params {
                BoundingBox(-256.0f, 256.0f)
            });
        }
        
        m_scene->SetCamera(
            CreateObject<Camera>(
                70.0f,
                1280, 768,
                0.5f, 30000.0f
            )
        );

        m_scene->GetCamera()->SetCameraController(UniquePtr<FollowCameraController>::Construct(
            Vector3(0.0f), Vector3(0.0f, 150.0f, -15.0f)
        ));

        // m_scene->GetCamera()->SetCameraController(UniquePtr<FirstPersonCameraController>::Construct());
        

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

        /*material_test_obj.Scale(5.0f);
        material_test_obj.Translate(Vector3(15.0f, 20.0f, 15.0f));
        GetScene()->GetRoot().AddChild(material_test_obj);*/

        test_model.Scale(0.2f);

        if (false) {
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

        if (true) {
            //auto container_node = GetUI().GetScene()->GetRoot().AddChild();
            //container_node.SetEntity(CreateObject<Entity>());
            //container_node.GetEntity()->SetTranslation(Vector3(0.4f, 0.4f, 0.0f));
            //container_node.GetEntity()->AddController<UIContainerController>();

            //container_node.Scale(0.2f);

            auto btn_node = GetUI().GetScene()->GetRoot().AddChild();
            btn_node.SetEntity(CreateObject<Entity>());
            btn_node.GetEntity()->SetTranslation(Vector3(0.0f, 0.85f, 0.0f));
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
            zombie.Scale(4.25f);
            zombie.Translate(Vector3(0, 0, -9));
            auto zombie_entity = zombie[0].GetEntity();
            zombie_entity->GetController<AnimationController>()->Play(1.0f, LoopMode::REPEAT);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.21f);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 1.0f);
            zombie_entity->RebuildRenderableAttributes();
            InitObject(zombie_entity);
            zombie_entity->CreateBLAS();
            zombie.SetName("zombie");
            m_scene->GetRoot().AddChild(zombie);
            
            auto zomb2 = CreateObject<Entity>();
            zomb2->SetMesh(zombie_entity->GetMesh());
            zomb2->SetTranslation(Vector3(0, 20, 0));
            zomb2->SetScale(Vector3(2.0f));
            zomb2->SetShader(zombie_entity->GetShader());
            zomb2->SetMaterial(CreateObject<Material>());//zombie_entity->GetMaterial());
            zomb2->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Color(1.0f, 1.0f, 1.0f, 0.8f));
            //zomb2->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_TRANSMISSION, 0.95f);
            //zomb2->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.025f);
            //zomb2->GetMaterial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            //zomb2->GetMaterial()->SetIsAlphaBlended(true);
            zomb2->SetName("FOOBAR ZOMBO");
            // zomb2->SetSkeleton(zombie_entity->GetSkeleton());
            zomb2->SetSkeleton(CreateObject<Skeleton>());
            zomb2->RebuildRenderableAttributes();

            InitObject(zomb2);
            m_scene->AddEntity(zomb2);
        }
        
        { // adding lights to scene
            m_sun = CreateObject<Light>(DirectionalLight(
                Vector3(-0.1f, 0.1f, 0.1f).Normalize(),
                Color(1.0f, 1.0f, 1.0f),
                250000.0f
            ));

            m_scene->AddLight(m_sun);

            m_point_lights.PushBack(CreateObject<Light>(PointLight(
                Vector3(0.5f, 50.0f, 70.1f),
                Color(0.0f, 0.0f, 1.0f),
                50000.0f,
                40.0f
            )));
            m_point_lights.PushBack(CreateObject<Light>(PointLight(
                Vector3(0.5f, 50.0f, -70.1f),
                Color(1.0f, 0.0f, 0.0f),
                10000.0f,
                40.0f
            )));
            
            m_point_lights.PushBack(CreateObject<Light>(PointLight(
                Vector3(40.5f, 50.0f, 40.1f),
                Color(0.0f, 1.0f, 0.0f),
                10000.0f,
                40.0f
            )));
            m_point_lights.PushBack(CreateObject<Light>(PointLight(
                Vector3(-40.5f, 50.0f, -40.1f),
                Color(0.0f, 1.0f, 1.0f),
                10000.0f,
                40.0f
            )));

            for (auto &light : m_point_lights) {
                m_scene->AddLight(light);
            }
        }

        if (false) { // adding cubemap rendering with a bounding box
            m_scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
                test_model.GetWorldAABB()
            );
        }

        if (true) {
            m_scene->GetEnvironment()->AddRenderComponent<EnvGrid>(test_model.GetWorldAABB());
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
        // m_scene->AddEntity(std::move(skybox_spatial));
        
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
        
        m_scene->GetFogParams().start_distance = 5000.0f;
        m_scene->GetFogParams().end_distance = 40000.0f;
        
        if (true) { // paged procedural terrain
            auto terrain_entity = CreateObject<Entity>();
            GetScene()->AddEntity(terrain_entity);
            terrain_entity->AddController<TerrainPagingController>(0xBEEF, Extent3D { 256 } , Vector3(8.0f, 8.0f, 8.0f), 1.0f);
        }

        if (true) { // skydome
            if (auto skydome_node = m_scene->GetRoot().AddChild()) {
                skydome_node.SetEntity(CreateObject<Entity>());
                skydome_node.GetEntity()->AddController<SkydomeController>();
            }
        }

        if (true) { // adding shadow maps
            m_scene->GetEnvironment()->AddRenderComponent<ShadowRenderer>(
                Handle<Light>(m_sun),
                test_model.GetWorldAABB()
            );
        }

        if (auto monkey = Engine::Get()->GetAssetManager().Load<Node>("models/monkey/monkey.obj")) {
            monkey.SetName("monkey");
            auto monkey_entity = monkey[0].GetEntity();
            monkey_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED, false);
            // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.15f);
            // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 1.0f);
            // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, Handle<Texture>());
            // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Handle<Texture>());
            // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
            // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
           //monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_TRANSMISSION, 0.95f);
            // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
           // monkey_entity->GetMaterial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            //monkey_entity->GetMaterial()->SetIsAlphaBlended(true);
            monkey_entity->RebuildRenderableAttributes();
            monkey.Translate(Vector3(0.0f, 160.5f, 0.0f));
            monkey.Scale(6.0f);
            InitObject(monkey_entity);

            monkey_entity->AddController<ScriptedController>(
                Engine::Get()->GetAssetManager().Load<Script>("scripts/examples/controller.hypscript")
            );

            monkey_entity->CreateBLAS();
            m_scene->GetRoot().AddChild(monkey);

            /*monkey[0].GetEntity()->AddController<RigidBodyController>(
                UniquePtr<physics::BoxPhysicsShape>::Construct(BoundingBox(-1, 1)),
                physics::PhysicsMaterial { .mass = 1.0f }
            );*/
        }

        if (true) {
            auto mh = Engine::Get()->GetAssetManager().Load<Node>("models/mh/mh1.obj");
            mh.SetName("mh_model");
            mh.Scale(1.0f);
            for (auto &mh_child : mh.GetChildren()) {
                mh_child.SetEntity(Handle<Entity>::empty);

                if (auto entity = mh_child.GetEntity()) {
                    // entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
                    // entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
                    // entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                    // entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.05f);
                    // entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 1.0f);
                }
            }
            GetScene()->GetRoot().AddChild(mh);


            NodeProxy tree = Engine::Get()->GetAssetManager().Load<Node>("models/conifer/Conifer_Low.obj");
            tree.SetName("tree");
            tree.Scale(1.0f);
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
        }


        if (false) {
            // add a plane physics shape
            auto plane = CreateObject<Entity>();
            plane->SetName("Plane entity");
            plane->SetTranslation(Vector3(0, 0, 0));
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
            //plane->GetController<RigidBodyController>()->GetRigidBody()->SetIsKinematic(false);
        }

        

        if (true) { // particles test
            auto particle_spawner = CreateObject<ParticleSpawner>(ParticleSpawnerParams {
                .texture = Engine::Get()->GetAssetManager().Load<Texture>("textures/smoke.png"),
                .max_particles = 1024u,
                .origin = Vector3(0.0f, 50.0f, -25.0f),
                .lifespan = 8.0f
            });

            InitObject(particle_spawner);

            m_scene->GetEnvironment()->GetParticleSystem()->GetParticleSpawners().Add(std::move(particle_spawner));
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

        GetScene()->GetCamera()->SetTarget(GetScene()->GetRoot().Select("zombie").GetWorldTranslation());

        for (auto &light : m_point_lights) {
            light->SetPosition(Vector3(
                MathUtil::Sin(light->GetID().Value() + timer) * 30.0f,
                30.0f,
                MathUtil::Cos(light->GetID().Value() + timer) * 30.0f
            ));
        }

        if (GetInputManager()->IsKeyDown(KEY_ARROW_LEFT)) {
            m_sun->SetPosition((m_sun->GetPosition() + Vector3(0.02f, 0.0f, 0.0f)).Normalize());
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_RIGHT)) {
            m_sun->SetPosition((m_sun->GetPosition() + Vector3(-0.02f, 0.0f, 0.0f)).Normalize());
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_UP)) {
            m_sun->SetPosition((m_sun->GetPosition() + Vector3(0.0f, 0.02f, 0.0f)).Normalize());
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_DOWN)) {
            m_sun->SetPosition((m_sun->GetPosition() + Vector3(0.0f, -0.02f, 0.0f)).Normalize());
        }

        if (GetInputManager()->IsKeyPress(KEY_C)) {
            // add a cube
            /*auto cube_entity = CreateObject<Entity>();
            cube_entity->SetName("Cube entity");
            cube_entity->SetMesh(MeshBuilder::Cube());
            cube_entity->SetMaterial(CreateObject<Material>());
            cube_entity->SetShader(CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("Forward", ShaderProps(renderer::static_mesh_vertex_attributes))));
            cube_entity->SetTranslation(GetScene()->GetCamera()->GetTranslation());
            cube_entity->SetScale(Vector3(2.0f));
            GetScene()->AddEntity(cube_entity);*/

            auto cube = Engine::Get()->GetAssetManager().Load<Node>("models/cube.obj");
            cube.Scale(2.0f);
            cube.SetLocalTranslation(GetScene()->GetCamera()->GetTranslation());
            GetScene()->GetRoot().AddChild(cube);
            std::cout << "CUBE MESH ID : " << cube[0].GetEntity()->GetMesh()->GetID().Value() << "\n";
        }

        // m_sun->SetPosition(Vector3(MathUtil::Sin(timer * 0.25f), MathUtil::Cos(timer * 0.25f), -MathUtil::Sin(timer * 0.25f)).Normalize());

        if (auto house = GetScene()->GetRoot().Select("house")) {
            //house.Rotate(Quaternion(Vector3(0, 1, 0), 0.1f * delta));
        }

        #if 1 // bad performance on large meshes. need bvh
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
                    Handle<Entity> entity(ID<Entity> { hit.id });

                    if (entity) {
                        // entity->AddController<AABBDebugController>();

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
        if (auto character = GetScene()->GetRoot().Select("zombie")) {
            constexpr Float speed = 0.75f;

            character.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetDirection(), GetScene()->GetCamera()->GetUpVector()));

            if (m_input_manager->IsKeyDown(KEY_W)) {
                character.Translate(GetScene()->GetCamera()->GetDirection() * delta * speed);
            }

            if (m_input_manager->IsKeyDown(KEY_S)) {
                character.Translate(GetScene()->GetCamera()->GetDirection() * -1.0f * delta * speed);
            }

            if (m_input_manager->IsKeyDown(KEY_A)) {
                character.Translate((GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector())) * -1.0f * delta * speed);
            }

            if (m_input_manager->IsKeyDown(KEY_D)) {
                character.Translate((GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector())) * delta * speed);
            }
        }
    }

    std::unique_ptr<Node> zombie;
    GameCounter::TickUnit timer = -18.0f;
    GameCounter::TickUnit ray_cast_timer{};

};
} // namespace hyperion::v2

int main()
{
    using namespace hyperion::renderer;

    RefCountedPtr<Application> application(new SDLApplication("My Application"));
    application->SetCurrentWindow(application->CreateSystemWindow("Hyperion Engine", 1280, 768));
    
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

            DebugLog(
                LogType::Debug,
                "Number of RenderGroups: %llu\n",
                Engine::Get()->GetRenderGroupMapping().Size()
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
