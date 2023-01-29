
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
#include <rendering/EnvGrid.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/AnimationController.hpp>
#include <scene/controllers/AabbDebugController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/paging/BasicPagingController.hpp>
#include <scene/controllers/ScriptedController.hpp>
#include <scene/controllers/physics/RigidBodyController.hpp>
#include <ui/controllers/UIButtonController.hpp>
#include <ui/controllers/UIPaneController.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>
#include <GameThread.hpp>
#include <Game.hpp>

#include <rendering/rt/BlurRadiance.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/FontAtlas.hpp>

#include <ui/UIText.hpp>
#include <font/Font.hpp>

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

    FilePath scene_export_filepath;

    MyGame(RefCountedPtr<Application> application)
        : Game(application),
          scene_export_filepath(Engine::Get()->GetAssetManager().GetBasePath() / "export.hypnode")
    {
    }

    virtual void InitRender() override
    {
        //Engine::Get()->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();
    }

    virtual void InitGame() override
    {
        Game::InitGame();

        m_scene->SetCamera(CreateObject<Camera>(
            70.0f,
            1280, 720,
            0.01f, 30000.0f
        ));

        /*m_scene->GetCamera()->SetCameraController(UniquePtr<FollowCameraController>::Construct(
            Vector3(0.0f), Vector3(0.0f, 150.0f, -15.0f)
        ));*/
        m_scene->GetCamera()->SetCameraController(UniquePtr<FirstPersonCameraController>::Construct());
        /*
        { // adding lights to scene
            m_sun = CreateObject<Light>(DirectionalLight(
                Vector3(-0.105425f, 0.988823f, 0.105425f).Normalize(),
                Color(1.0f, 1.0f, 1.0f),
                10.0f
            ));

            m_scene->AddLight(m_sun);

            //m_point_lights.PushBack(CreateObject<Light>(PointLight(
            //    Vector3(0.5f, 50.0f, 70.1f),
            //    Color(1.0f, 1.0f, 1.0f),
            //    1.0f,
            //    1.0f
            //)));
            //m_point_lights.PushBack(CreateObject<Light>(PointLight(
            //    Vector3(0.5f, 50.0f, -70.1f),
            //    Color(1.0f, 0.0f, 0.0f),
            //    2.0f,
            //    40.0f
            //)));

            //m_point_lights.PushBack(CreateObject<Light>(PointLight(
            //    Vector3(40.5f, 50.0f, 40.1f),
            //    Color(0.0f, 1.0f, 0.0f),
            //    2.0f,
            //    40.0f
            //)));
            //m_point_lights.PushBack(CreateObject<Light>(PointLight(
            //    Vector3(-40.5f, 50.0f, -40.1f),
            //    Color(0.0f, 1.0f, 1.0f),
            //    1.0f,
            //    40.0f
            //)));

            for (auto &light : m_point_lights) {
                m_scene->AddLight(light);
            }
        }


        if (true) {
            auto btn_node = GetUI().GetScene()->GetRoot().AddChild();
            btn_node.SetEntity(CreateObject<Entity>());
            btn_node.GetEntity()->SetTranslation(Vector3(0.0f, 0.85f, 0.0f));
            btn_node.GetEntity()->AddController<UIButtonController>();

            if (UIButtonController *controller = btn_node.GetEntity()->GetController<UIButtonController>()) {
                controller->SetScript(Engine::Get()->GetAssetManager().Load<Script>("scripts/examples/ui_controller.hypscript"));
            }

            btn_node.Scale(0.01f);
        }

        { // allow ui rendering
            m_scene->GetEnvironment()->AddRenderComponent<UIRenderer>(HYP_NAME(UIRenderer0), GetUI().GetScene());
        }

        if (scene_export_filepath.Exists()) {
            // read file if it already exists.
            fbom::FBOMReader reader(fbom::FBOMConfig { });
            fbom::FBOMDeserializedObject deserialized;

            DebugLog(
                LogType::Info,
                "Attempting to load exported scene %s...\n",
                scene_export_filepath.Data()
            );

            if (auto err = reader.LoadFromFile(scene_export_filepath, deserialized)) {
                DebugLog(
                    LogType::Error,
                    "Failed to load scene export. Message was: %s\n",
                    err.message
                );

                HYP_BREAKPOINT;
            } else if (auto exported_node = deserialized.Get<Node>()) {
                Node *n = exported_node.Get();
                std::cout << " node name " << n->GetName() << std::endl;

                for (auto ch : n->GetChildren()) {
                    std::cout << "ch : " << ch.GetName() << std::endl;
                    std::cout << "entity : " << (ch.GetEntity() ? ch.GetEntity()->GetName().LookupString() : " <no entity>") << std::endl;

                    for (auto ch2 : ch.GetChildren()) {
                        std::cout << "\tch2 : " << ch2.GetName() << std::endl;

                        std::cout << "\tentity : " << (ch2.GetEntity() ? ch2.GetEntity()->GetName().LookupString() : " <no entity>") << std::endl;
                    }
                }

                m_scene->GetRoot().AddChild(exported_node);
                Engine::Get()->GetWorld()->AddScene(m_scene);

                return;
            }
        }

        if (false) { // skydome
            if (auto skydome_node = m_scene->GetRoot().AddChild()) {
                skydome_node.SetEntity(CreateObject<Entity>());
                skydome_node.GetEntity()->AddController<SkydomeController>();
            }
        }

        if (Engine::Get()->GetConfig().Get(CONFIG_VOXEL_GI)) { // voxel cone tracing for indirect light and reflections
            m_scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(
                HYP_NAME(VCTRenderer0),
                VoxelConeTracing::Params { BoundingBox(-256.0f, 256.0f) }
            );
        }

        // m_scene->GetCamera()->SetCameraController(UniquePtr<FirstPersonCameraController>::Construct());


        m_scene->GetFogParams().start_distance = 5000.0f;
        m_scene->GetFogParams().end_distance = 40000.0f;

        Engine::Get()->GetWorld()->AddScene(m_scene);

        auto batch = Engine::Get()->GetAssetManager().CreateBatch();
        batch.Add<Node>("zombie", "models/ogrexml/dragger_Body.mesh.xml");
        batch.Add<Node>("test_model", "models/sponza/sponza.obj");//"mideval/p3d_medieval_enterable_bld-13.obj");//"San_Miguel/san-miguel-low-poly.obj");
        batch.Add<Node>("cube", "models/cube.obj");
        batch.Add<Node>("material", "models/material_sphere/material_sphere.obj");
        batch.Add<Node>("grass", "models/grass/grass.obj");

        //batch.Add<Node>("dude3", "models/dude3/Dude3_Body.mesh.xml");

        //batch.Add<Node>("monkey_fbx", "models/zombieSuit.fbx");
        batch.LoadAsync();

        auto obj_models = batch.AwaitResults();
        auto zombie = obj_models["zombie"].Get<Node>();
        auto test_model = obj_models["test_model"].Get<Node>();//Engine::Get()->GetAssetManager().Load<Node>("../data/dump2/sponza.fbom");
        auto cube_obj = obj_models["cube"].Get<Node>();
        auto material_test_obj = obj_models["material"].Get<Node>();

        auto monkey_fbx = GetScene()->GetRoot().AddChild(obj_models["monkey_fbx"].Get<Node>());
        monkey_fbx.SetName("monkey_fbx");
        //monkey_fbx.Scale(0.2f);
        monkey_fbx.Rotate(Vector3(90, 0, 0));

        material_test_obj.Scale(2.0f);
        material_test_obj.Translate(Vector3(0.0f, 4.0f, 9.0f));
        GetScene()->GetRoot().AddChild(material_test_obj);

        if (auto dude = obj_models["dude3"].Get<Node>()) {
            dude.SetName("dude");
            for (auto &child : dude.GetChildren()) {
                if (auto entity = child.GetEntity()) {
                    if (auto *animation_controller = entity->GetController<AnimationController>()) {
                        animation_controller->Play(1.0f, LoopMode::REPEAT);
                    }
                }
            }

            GetScene()->GetRoot().AddChild(dude);
        }

        test_model.Scale(0.025f);

        if (Engine::Get()->GetConfig().Get(CONFIG_ENV_GRID_GI)) {
            m_scene->GetEnvironment()->AddRenderComponent<EnvGrid>(
                HYP_NAME(AmbientGrid0),
                test_model.GetWorldAABB(),//BoundingBox(Vector3(-100.0f, -10.0f, -100.0f), Vector3(100.0f, 100.0f, 100.0f)),
                Extent3D { 17, 2, 17 }
            );
        }

        if (Engine::Get()->GetConfig().Get(CONFIG_ENV_GRID_REFLECTIONS)) {
            m_scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
                HYP_NAME(EnvProbe0),
                test_model.GetWorldAABB()
            );
        }

        m_scene->GetEnvironment()->AddRenderComponent<ShadowRenderer>(
            HYP_NAME(Shadows),
            m_sun, test_model.GetWorldAABB()
        );

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
                    ent->GetController<RigidBodyController>()->GetRigidBody()->SetIsKinematic(false);

                    i++;
                }
            }
        }

        if (true) {
            auto container_node = GetUI().GetScene()->GetRoot().AddChild();
            container_node.SetEntity(CreateObject<Entity>());
            container_node.GetEntity()->SetTranslation(Vector3(0.4f, 0.4f, 0.0f));
            container_node.GetEntity()->AddController<UIPaneController>();

            container_node.Scale(0.2f);

            auto btn_node = container_node.AddChild();
            btn_node.SetEntity(CreateObject<Entity>());
            btn_node.GetEntity()->SetTranslation(Vector3(0.0f, 0.85f, 0.0f));
            btn_node.GetEntity()->AddController<UIButtonController>();

            if (UIButtonController *controller = btn_node.GetEntity()->GetController<UIButtonController>()) {
                controller->SetGridOffset({ 1, 1 });
                //controller->SetScript(
                //    Engine::Get()->GetAssetManager().Load<Script>("scripxts/examples/ui_controller.hypscript"));
            }

            btn_node.Scale(0.05f);
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
        cubemap->GetImage()->SetIsSRGB(true);
        InitObject(cubemap);

        if (false) { // hardware skinning
            auto zombie_entity = zombie[0].GetEntity();
            zombie_entity->GetController<AnimationController>()->Play(1.0f, LoopMode::REPEAT);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.001f);
            zombie_entity->GetMaterial()->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 1.0f);
            zombie_entity->RebuildRenderableAttributes();
            zombie_entity->SetTranslation(Vector3(0, 45, 0));
            zombie_entity->SetScale(Vector3(4.5f));

            // zombie_entity->AddController<AABBDebugController>();

            // auto *rigid_body_controller = zombie_entity->AddController<RigidBodyController>(
            //     UniquePtr<physics::BoxPhysicsShape>::Construct(zombie_entity->GetWorldAABB()),
            //     physics::PhysicsMaterial { .mass = 250.0f }
            // );
            //rigid_body_controller->GetRigidBody()->SetIsKinematic(false);

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
            // zomb2->SetSkeleton(zombie_entity->GetSkeleton());
            zomb2->SetSkeleton(CreateObject<Skeleton>());
            zomb2->RebuildRenderableAttributes();

            InitObject(zomb2);
            m_scene->AddEntity(zomb2);
        }

        cube_obj.Scale(50.0f);

        if (false) {
            auto axis_angles = Engine::Get()->GetAssetManager().Load<Node>("models/editor/axis_arrows.obj");
            axis_angles.Scale(10.0f);
            GetScene()->GetRoot().AddChild(axis_angles);
        }
        
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
        
        if (false) { // paged procedural terrain
            auto terrain_entity = CreateObject<Entity>();
            GetScene()->AddEntity(terrain_entity);
            terrain_entity->AddController<TerrainPagingController>(0xBEEF, Extent3D { 256 }, Vector3(8.0f, 8.0f, 8.0f), 1.0f);
        }

        if (false) { // physics
            for (int i = 0; i < 6; i++) {
                if (auto cube = Engine::Get()->GetAssetManager().Load<Node>("models/cube.obj")) {
                    cube.SetName("cube " + String::ToString(i));
                    auto cube_entity = cube[0].GetEntity();
                    cube_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED, false);
                    cube_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.3f);
                    cube_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
                    cube_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, Handle<Texture>());
                    cube_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Handle<Texture>());
                    cube_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
                    cube_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
                    cube_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(MathUtil::RandRange(0.0f, 1.0f), MathUtil::RandRange(0.0f, 1.0f), MathUtil::RandRange(0.0f, 1.0f), 1.0f));
                    cube_entity->RebuildRenderableAttributes();

                    cube_entity->SetScale(Vector3(3.0f));
                    cube_entity->SetTranslation(Vector3(0, i * 40 + 50, 0));

                    cube_entity->CreateBLAS();
                    InitObject(cube_entity);
                    m_scene->GetRoot().AddChild(cube);

                    cube_entity->AddController<RigidBodyController>(
                        UniquePtr<physics::BoxPhysicsShape>::Construct(cube_entity->GetWorldAABB()),
                        physics::PhysicsMaterial { .mass = 1.0f }
                    );
                }
            }
        }

        if (true) {
            if (auto monkey = Engine::Get()->GetAssetManager().Load<Node>("models/monkey/monkey.obj")) {
                monkey.SetName("monkey");
                auto monkey_entity = monkey[0].GetEntity();
                monkey_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED, false);
                // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.55f);
                // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 1.0f);
                // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, Handle<Texture>());
                // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Handle<Texture>());
                // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>());
                // monkey_entity->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>());
                // monkey_entity->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Color(1.0f, 0.2f, 0.0f, 1.0f));
                monkey_entity->RebuildRenderableAttributes();
                monkey.SetLocalTranslation(Vector3(0.0f, 0.0f, 0.0f));
                monkey.Scale(1.2f);
                monkey.Rotate(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(90.0f)));
                InitObject(monkey_entity);

                monkey_entity->AddController<ScriptedController>(
                    Engine::Get()->GetAssetManager().Load<Script>("scripts/examples/controller.hypscript")
                );

                monkey_entity->CreateBLAS();
                m_scene->GetRoot().AddChild(monkey);

                //monkey[0].GetEntity()->AddController<RigidBodyController>(
                //    UniquePtr<physics::BoxPhysicsShape>::Construct(monkey[0].GetWorldAABB()),
                //    physics::PhysicsMaterial { .mass = 1.0f }
            // );
            }
        }

        if (true) {
            auto mh = Engine::Get()->GetAssetManager().Load<Node>("models/mh/mh1.obj");
            mh.SetName("mh_model");
            mh.Scale(0.05f);
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
            tree.Scale(0.05f);
            if (auto needles = tree.Select("Needles")) {
                if (needles.GetEntity() && needles.GetEntity()->GetMaterial()) {
                    needles.GetEntity()->GetMaterial()->SetFaceCullMode(FaceCullMode::NONE);
                    //needles.GetEntity()->GetMaterial()->SetBucket(BUCKET_TRANSLUCENT);
                }
            }

            GetScene()->GetRoot().AddChild(tree);
        }

        if (true) {
            auto cube_model = Engine::Get()->GetAssetManager().Load<Node>("models/cube.obj");

            // add a plane physics shape
            auto plane = CreateObject<Entity>();
            plane->SetTranslation(Vector3(0, 0.1f, 0));
            plane->SetMesh(MeshBuilder::Quad());//Cube());
            plane->GetMesh()->SetVertexAttributes(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes);
            plane->SetScale(Vector3(15.0f));
            plane->SetMaterial(CreateObject<Material>());
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.0f, 0.75f, 1.0f, 1.0f));
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.008f);
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_UV_SCALE, Vector2(2.0f));
            plane->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, Engine::Get()->GetAssetManager().Load<Texture>("textures/water.jpg"));
            plane->GetMaterial()->SetParameter(Material::MATERIAL_KEY_NORMAL_MAP_INTENSITY, 0.08f);
            // plane->GetMaterial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            plane->SetShader(Handle<Shader>(Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(Forward), ShaderProps(plane->GetMesh()->GetVertexAttributes()))));
            plane->RebuildRenderableAttributes();
            plane->CreateBLAS();
            if (NodeProxy plane_node_proxy = GetScene()->AddEntity(plane)) {
                plane_node_proxy.SetWorldRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(-90.0f)));
                // plane->AddController<RigidBodyController>(
                //     UniquePtr<physics::BoxPhysicsShape>::Construct(plane->GetWorldAABB()),
                //     physics::PhysicsMaterial { .mass = 0.0f }
                // );
                // plane->GetController<RigidBodyController>()->GetRigidBody()->SetIsKinematic(false);

                // plane->AddController<AABBDebugController>();
            }
        }

        if (false) { // particles test
            auto particle_spawner = CreateObject<ParticleSpawner>(ParticleSpawnerParams {
                .texture = Engine::Get()->GetAssetManager().Load<Texture>("textures/smoke.png"),
                .max_particles = 1024u,
                .origin = Vector3(0.0f, 50.0f, -25.0f),
                .lifespan = 8.0f
            });

            InitObject(particle_spawner);

            m_scene->GetEnvironment()->GetParticleSystem()->GetParticleSpawners().Add(std::move(particle_spawner));
        }
        */
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
        /*
        timer += delta;

        m_ui.Update(delta);

        HandleCameraMovement(delta);

        if (auto fbx_node = GetScene()->GetRoot().Select("monkey_fbx")) {
            if (auto body = fbx_node.Select("Models:Body")) {
                if (const auto &entity = body.GetEntity()) {
                    if (const auto &skeleton = entity->GetSkeleton()) {
                        if (Bone *bone = skeleton->FindBone("thigh.L")) {
                            //bone->SetLocalRotation(Quaternion(Vector3(MathUtil::RadToDeg(MathUtil::Sin(delta * 2.0f)), 0.0f, 0.0f)));
                        }
                    }
                }
            }
        }

        //GetScene()->GetCamera()->SetTarget(GetScene()->GetRoot().Select("zombie")[0].GetWorldTranslation());

        //for (auto &light : m_point_lights) {
        //    light->SetPosition(Vector3(
        //        MathUtil::Sin(light->GetID().Value() + timer) * 30.0f,
        //        30.0f,
        //        MathUtil::Cos(light->GetID().Value() + timer) * 30.0f
        //    ));
        //}

        if (!m_point_lights.Empty()) {
        //    m_point_lights.Front()->SetPosition(GetScene()->GetCamera()->GetTranslation() + GetScene()->GetCamera()->GetDirection() * 15.0f);
        }

        bool sun_position_changed = false;

        if (GetInputManager()->IsKeyDown(KEY_ARROW_LEFT)) {
            m_sun->SetPosition((m_sun->GetPosition() + Vector3(0.02f, 0.0f, 0.0f)).Normalize());
            sun_position_changed = true;
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_RIGHT)) {
            m_sun->SetPosition((m_sun->GetPosition() + Vector3(-0.02f, 0.0f, 0.0f)).Normalize());
            sun_position_changed = true;
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_UP)) {
            m_sun->SetPosition((m_sun->GetPosition() + Vector3(0.0f, 0.02f, 0.0f)).Normalize());
            sun_position_changed = true;
        } else if (GetInputManager()->IsKeyDown(KEY_ARROW_DOWN)) {
            m_sun->SetPosition((m_sun->GetPosition() + Vector3(0.0f, -0.02f, 0.0f)).Normalize());
            sun_position_changed = true;
        }

        if (sun_position_changed) {
            std::cout << "Sun position: " << m_sun->GetPosition() << "\n";
            std::cout << "Camera position " << GetScene()->GetCamera()->GetTranslation() << "\n";
        }

        if (m_export_task == nullptr || m_export_task->IsCompleted()) {
            if (GetInputManager()->IsKeyStateChanged(KEY_C, &m_export_pressed) && m_export_pressed) {
                m_export_task.Reset(new TaskBatch);
                m_export_task->AddTask([export_path = scene_export_filepath, node = m_scene->GetRoot().Get()](...) {
                    DebugLog(LogType::Info, "Begin export task, exporting to path: %s\n", export_path.Data());

                    UniquePtr<fbom::FBOMWriter> writer(new fbom::FBOMWriter);
                    writer->Append(*node);

                    FileByteWriter byte_writer(export_path.Data());
                    auto err = writer->Emit(&byte_writer);
                    byte_writer.Close();

                    if (err.value != fbom::FBOMResult::FBOM_OK) {
                        DebugLog(LogType::Error, "Failed to export scene: %s\n", err.message);
                    } else {
                        DebugLog(LogType::Info, "Finished exporting!\n");
                    }
                });

                Engine::Get()->task_system.EnqueueBatch(m_export_task.Get());
            }
        }*/

        // m_sun->SetPosition(Vector3(MathUtil::Sin(timer * 0.25f), MathUtil::Cos(timer * 0.25f), -MathUtil::Sin(timer * 0.25f)).Normalize());

        #if 1 && 0 // bad performance on large meshes. need bvh
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

            auto ray_direction = mouse_world.Normalized();

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
        bool moving = false;

        Vector3 dir;

        if (m_input_manager->IsKeyDown(KEY_W)) {
            dir = GetScene()->GetCamera()->GetDirection();

            moving = true;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_FORWARD } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_S)) {
            dir = GetScene()->GetCamera()->GetDirection() * -1.0f;

            moving = true;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_BACKWARD } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_A)) {
            dir = GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector()) * -1.0f;

            moving = true;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_LEFT } }
            });
        }

        if (m_input_manager->IsKeyDown(KEY_D)) {
            dir = GetScene()->GetCamera()->GetDirection().Cross(GetScene()->GetCamera()->GetUpVector());

            moving = true;

            GetScene()->GetCamera()->GetCameraController()->PushCommand({
                CameraCommand::CAMERA_COMMAND_MOVEMENT,
                { .movement_data = { CameraCommand::CAMERA_MOVEMENT_RIGHT } }
            });
        }

        /*
        if (auto character = GetScene()->GetRoot().Select("zombie")) {
            character.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetDirection(), GetScene()->GetCamera()->GetUpVector()));

            if (const auto &entity = character[0].GetEntity()) {
                if (auto *controller = entity->GetController<RigidBodyController>()) {
                    controller->GetRigidBody()->ApplyForce(dir);
                }
            }
        }*/

#if 0
        if (auto character = GetScene()->GetRoot().Select("dude")) {
            character.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetDirection() * Vector3(1.0f, 0.0f, 1.0f), GetScene()->GetCamera()->GetUpVector()));
            character.SetWorldTranslation(GetScene()->GetCamera()->GetTranslation() - (GetScene()->GetCamera()->GetUpVector() * 16.0f));

            if (auto *animation_controller = character[0].GetEntity()->GetController<AnimationController>()) {
                if (moving) {
                    animation_controller->Play("Sprint", 1.0f, LoopMode::REPEAT);
                } else {
                    animation_controller->Stop();
                }
            }
        }
#endif
    }

    std::unique_ptr<Node> zombie;
    GameCounter::TickUnit timer = -18.0f;
    GameCounter::TickUnit ray_cast_timer{};
    bool m_export_pressed = false;
    bool m_export_in_progress = false;
    UniquePtr<TaskBatch> m_export_task;

};

} // namespace hyperion::v2

int main()
{
    using namespace hyperion::renderer;

    RefCountedPtr<Application> application(new SDLApplication("My Application"));
    application->SetCurrentWindow(application->CreateSystemWindow("Hyperion Engine", 1280, 720));
    
    SystemEvent event;

    auto *my_game = new MyGame(application);

    Engine::Get()->Initialize(application);

    my_game->Init();

    Engine::Get()->Compile();
    
    Engine::Get()->game_thread.Start(my_game);


    font::FontEngine font_engine;
    font::Face face = font_engine.LoadFont("/usr/share/fonts/cantarell/Cantarell-VF.otf");

    face.RequestPixelSizes(0, 32);

    FontAtlas font_atlas(face);

    FontRenderer fr;
    fr.RenderAtlas(font_atlas);

    face.GetFace();

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
