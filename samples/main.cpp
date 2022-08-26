
#include "system/SdlSystem.hpp"
#include "system/Debug.hpp"
#include "rendering/backend/RendererInstance.hpp"
#include "rendering/backend/RendererDescriptorSet.hpp"
#include "rendering/backend/RendererImage.hpp"
#include "rendering/backend/RendererRenderPass.hpp"
#include "rendering/backend/rt/RendererRaytracingPipeline.hpp"

#include <core/lib/Proc.hpp>

#include <Engine.hpp>
#include <scene/Node.hpp>
#include <rendering/Atomics.hpp>
#include <animation/Bone.hpp>
#include <asset/model_loaders/OBJModelLoader.hpp>
#include <rendering/rt/AccelerationStructureBuilder.hpp>
#include <rendering/ProbeSystem.hpp>
#include <rendering/post_fx/SSAO.hpp>
#include <rendering/post_fx/FXAA.hpp>
#include <rendering/post_fx/Tonemap.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/AnimationController.hpp>
#include <scene/controllers/AABBDebugController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/paging/BasicPagingController.hpp>
#include <scene/controllers/ScriptedController.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>
#include <GameThread.hpp>
#include <Game.hpp>

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>

#include <terrain/controllers/TerrainPagingController.hpp>

#include <rendering/vct/VoxelConeTracing.hpp>

#include <util/fs/FsUtil.hpp>

#include <scene/NodeProxy.hpp>

#include <camera/FirstPersonCamera.hpp>
#include <camera/FollowCamera.hpp>

#include <builders/MeshBuilder.hpp>

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
#include "rendering/render_components/CubemapRenderer.hpp"

#include <script/ScriptBindings.hpp>

#include <util/UTF8.hpp>

using namespace hyperion;
using namespace hyperion::v2;


#define HYPERION_VK_TEST_VCT 1
#define HYPERION_VK_TEST_RAYTRACING 0
#define HYPERION_RUN_TESTS 1

namespace hyperion::v2 {
    
class MyGame : public Game {

public:
    Handle<Material> base_material;//hack

    Handle<Light> m_point_light;

    MyGame()
        : Game()
    {
    }

    virtual void InitRender(Engine *engine) override
    {
        engine->GetDeferredRenderer().GetPostProcessing().AddEffect<SSAOEffect>();
        engine->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();
    }

    virtual void InitGame(Engine *engine) override
    {
        m_scene = Handle<Scene>(new Scene(
            Handle<Camera>(new FirstPersonCamera(//FollowCamera(
                //Vector3(0, 0, 0), Vector3(0, 0.5f, -2),
                2048, 2048,//2048, 1080,
                75.0f,
                0.5f, 30000.0f
            ))
        ));

        engine->GetWorld().AddScene(Handle<Scene>(m_scene));

        base_material = Handle<Material>(new Material());
        engine->Attach(base_material);

        auto mat2 = Handle<Material>(new Material());
        engine->Attach(mat2);
        // mat2.Reset();

        auto mat3 = Handle<Material>(new Material());
        engine->Attach(mat3);

        auto ent1 = Handle<Entity>(new Entity(Handle<Mesh>(), Handle<Shader>(), std::move(mat3)));
        engine->Attach(ent1);

        if (auto res = engine->registry.Lookup<Entity>(ent1->GetId())) {
            auto void_type = TypeID::ForType<void>();
            auto default_type = TypeID();
        }

        ent1.Reset();

        // HYP_BREAKPOINT;

        auto loaded_assets = engine->assets.Load<Node>(
            "models/ogrexml/dragger_Body.mesh.xml",
            "models/sponza/sponza.obj",//testbed/testbed.obj", ////sibenik/sibenik.obj",//, //, //", //
            "models/cube.obj",
            "models/material_sphere/material_sphere.obj",
            "models/grass/grass.obj"
        );

        zombie = std::move(loaded_assets[0]);
        test_model = std::move(loaded_assets[1]);
        cube_obj = std::move(loaded_assets[2]);
        material_test_obj = std::move(loaded_assets[3]);

        for (int i = 0; i < 10; i++) {
            auto sphere = engine->assets.Load<Node>("models/sphere_hq.obj");
            sphere->Scale(1.0f);
            sphere->SetName("sphere");
            sphere->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.1f, 0.8f, 0.35f, 1.0f));
            sphere->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, MathUtil::Clamp(float(i) / 10.0f, 0.05f, 0.95f));
            sphere->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
            // sphere->GetChild(0).Get()->GetEntity()->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>(engine->assets.Load<Texture>("textures/plastic/plasticpattern1-normal2-unity2b.png").release()));
            sphere->GetChild(0).Get()->GetEntity()->GetInitInfo().flags &= ~Entity::ComponentInitInfo::Flags::ENTITY_FLAGS_RAY_TESTS_ENABLED;
            sphere->SetLocalTranslation(Vector3(0 + (i * 6.0f), 7.0f, 0.0f));
            m_scene->GetRoot().AddChild(NodeProxy(sphere.release()));
        }

#if 0// serialize/deseriale test

#if 1
        FileByteWriter bw("data/dump/sponza.fbom");
        // test_model->GetChild(0).Get()->GetEntity()->SetTranslation(Vector3(0, 9999, 0));
        // test_model->GetChild(0).Get()->GetEntity()->SetScale(2.5f);
        fbom::FBOMWriter fw;
        // fw.Append(*test_model->GetChild(0).Get()->GetEntity());
        fw.Append(*test_model);
        auto err = fw.Emit(&bw);

        AssertThrowMsg(err.value != fbom::FBOMResult::FBOM_ERR, "%s\n", err.message);

        bw.Close();

#endif


        fbom::FBOMLoader fr(engine->resources);
        fbom::FBOMDeserializedObject result;
        if (auto err = fr.LoadFromFile("data/dump/sponza.fbom", result)) {
            AssertThrowMsg(false, "%s\n", err.message);
        }

        auto *_node = result.Release<Node>();
        m_scene->GetRoot().AddChild(NodeProxy(_node));
        
        // auto ent = engine->resources->entities.Add(result.Release<Entity>());
        // m_scene->GetRoot().AddChild().Get()->SetEntity(std::move(ent));
#endif

        // auto character_entity = engine->resources->entities.Add(new Spatial());
        // character_entity->AddController<BasicCharacterController>();
        // m_scene->AddSpatial(std::move(character_entity));

        // auto tmp_terrain = engine->assets.Load<Node>("models/tmp_terrain.obj");
        // tmp_terrain->Rotate(Quaternion({ 1, 0, 0 }, MathUtil::DegToRad(90.0f)));
        // tmp_terrain->Scale(500.0f);
        // m_scene->AddSpatial(tmp_terrain->GetChild(0)->GetEntity().IncRef());


#if 0
        if (auto terrain_node = m_scene->GetRoot().AddChild()) {
            terrain_node.Get()->SetEntity(engine->resources->entities.Add(new Entity()));
            terrain_node.Get()->GetEntity()->AddController<TerrainPagingController>(0xBEEF, Extent3D { 256 } , Vector3(35.0f, 32.0f, 35.0f), 2.0f);
        }
#endif
        
        if (auto grass = m_scene->GetRoot().AddChild(NodeProxy(loaded_assets[4].release()))) {
            //grass->GetChild(0)->GetEntity()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            grass.GetChild(0).Get()->GetEntity()->SetShader(Handle<Shader>(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_VEGETATION)));
            grass.Scale(1.0f);
            grass.Translate({0, 1, 0});
        }


        material_test_obj->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_PARALLAX_HEIGHT, 0.1f);
        material_test_obj->Scale(3.45f);
        material_test_obj->Translate(Vector3(0, 22, 0));
        m_scene->GetRoot().AddChild(NodeProxy(material_test_obj.release()));

        // remove textures so we can manipulate the material and see our changes easier
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ROUGHNESS_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_METALNESS_MAP, nullptr);
        
        auto cubemap = Handle<Texture>(new TextureCube(
           engine->assets.Load<Texture>(
               "textures/chapel/posx.jpg",
               "textures/chapel/negx.jpg",
               "textures/chapel/posy.jpg",
               "textures/chapel/negy.jpg",
               "textures/chapel/posz.jpg",
               "textures/chapel/negz.jpg"
            )
        ));
        cubemap->GetImage().SetIsSRGB(true);
        engine->Attach(cubemap);
        // cubemap->Init(engine);

        zombie->GetChild(0).Get()->GetEntity()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
        zombie->Scale(1.25f);
        zombie->Translate({0, 0, -5});
        zombie->GetChild(0).Get()->GetEntity()->GetController<AnimationController>()->Play(1.0f, LoopMode::REPEAT);
        // zombie->GetChild(0)->GetEntity()->AddController<AABBDebugController>(engine);
        m_scene->GetRoot().AddChild(NodeProxy(zombie.release()));

        //zombie->GetChild(0)->GetEntity()->GetSkeleton()->FindBone("thigh.L")->SetLocalRotation(Quaternion({1.0f, 0.0f, 0.0f}, MathUtil::DegToRad(90.0f)));
        //zombie->GetChild(0)->GetEntity()->GetSkeleton()->GetRootBone()->UpdateWorldTransform();

        auto my_light = Handle<Light>(new DirectionalLight(
            Vector3(-0.5f, 0.5f, 0.0f).Normalize(),
            Vector4::One(),
            110000.0f
        ));
        m_scene->GetEnvironment()->AddLight(Handle<Light>(my_light));

        m_point_light = Handle<Light>(new PointLight(
            Vector3(0.0f, 6.0f, 0.0f),
            Vector4(1.0f, 0.3f, 0.1f, 1.0f),
            500.0f,
            35.0f
        ));

        m_scene->GetEnvironment()->AddLight(Handle<Light>(m_point_light));

        // test_model->Scale(10.0f);
        test_model->Scale(0.08f);//14.075f);

        /*auto &terrain_material = test_model->GetChild(0)->GetEntity()->GetMaterial();
        terrain_material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, 50.0f);
        terrain_material->SetParameter(Material::MATERIAL_KEY_PARALLAX_HEIGHT, 0.08f);
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture>(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-albedo.png")));
        terrain_material->GetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP)->GetImage().SetIsSRGB(true);
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Handle<Texture>(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-normal-dx.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_AO_MAP, Handle<Texture>(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-ao.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_PARALLAX_MAP, Handle<Texture>(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1_Height.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Handle<Texture>(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1_Roughness.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, Handle<Texture>(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-metallic.png")));
        test_model->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));*/

        if (auto test = m_scene->GetRoot().AddChild(NodeProxy(test_model.release()))) {
            // for (auto &child : test->GetChildren()) {
            //     if (auto &ent = child->GetEntity()) {
            //         std::cout << "Adding debug controller to  " << child->GetName() << "\n";
            //         ent->AddController<AABBDebugController>(engine);
            //     }
            // }
        }

        auto quad = Handle<Mesh>(MeshBuilder::NormalizedCubeSphere(8).release());//MeshBuilder::DividedQuad(8).release());    //MeshBuilder::Quad());
        // quad->SetVertexAttributes(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes);
        auto quad_spatial = engine->resources->entities.Add(new Entity(
            std::move(quad),
            Handle<Shader>(engine->shader_manager.GetShader(ShaderKey::BASIC_FORWARD)),
            Handle<Material>(new Material())
        ));
    
        quad_spatial.Init();
        quad_spatial->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f));//0.00f, 0.4f, 0.9f, 1.0f));
        quad_spatial->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.2f);
        quad_spatial->SetScale(Vector3(3.0f));
        quad_spatial->SetRotation(Quaternion(Vector3(1, 1, 1), MathUtil::DegToRad(-40.0f)));
        quad_spatial->SetTranslation(Vector3(0, 12.0f, 0));
        // m_scene->AddEntity(std::move(quad_spatial));
        
        m_scene->GetEnvironment()->AddRenderComponent<ShadowRenderer>(
            Handle<Light>(my_light),
            Vector3::Zero(),
            80.0f
        );

        m_scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
            renderer::Extent2D {128, 128},
            BoundingBox { Vector(-128, -10, -128), Vector(128, 100, 128) },
            renderer::Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
        );
        m_scene->ForceUpdate();

#if HYPERION_VK_TEST_VCT
        m_scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(
            VoxelConeTracing::Params {
                BoundingBox(-128, 128)
            }
        );
#endif

        cube_obj->Scale(50.0f);

        auto skybox_material = Handle<Material>(new Material());
        skybox_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));
        skybox_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, std::move(cubemap));

        auto &skybox_spatial = cube_obj->GetChild(0).Get()->GetEntity();
        skybox_spatial->SetMaterial(std::move(skybox_material));
        skybox_spatial->SetBucket(BUCKET_SKYBOX);
        skybox_spatial->SetShader(Handle<Shader>(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_SKYBOX)));
        // skybox_spatial->SetMeshAttributes(
        //     FaceCullMode::FRONT,
        //     false,
        //     false
        // );

        m_scene->AddEntity(cube_obj->GetChild(0).Get()->GetEntity().IncRef());

        auto monkey = engine->assets.Load<Node>("models/monkey/monkey.obj");

        monkey->GetChild(0).Get()->GetEntity()->AddController<ScriptedController>(engine->assets.Load<Script>("scripts/examples/controller.hypscript"));
        monkey->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.35f);
        monkey->Translate(Vector3(0, 12.5f, 0));
        monkey->Scale(2.0f);
        m_scene->GetRoot().AddChild(NodeProxy(monkey.release()));

        for (auto &x : m_scene->GetRoot().GetChildren()) {
            DebugLog(LogType::Debug, "%s\n", x.GetName().Data());
        }
    }

    virtual void Teardown(Engine *engine) override
    {
        engine->GetWorld().RemoveScene(m_scene->GetId());
        m_scene.Reset();

        Game::Teardown(engine);
    }

    virtual void OnFrameBegin(Engine *engine, Frame *frame) override
    {
        m_scene->GetEnvironment()->RenderComponents(engine, frame);

        engine->render_state.visibility_cursor = engine->GetWorld().GetOctree().LoadPreviousVisibilityCursor();
        engine->render_state.BindScene(m_scene.Get());
    }

    virtual void OnFrameEnd(Engine *engine, Frame *) override
    {
        engine->render_state.UnbindScene();
    }

    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) override
    {
        timer += delta;
        ++counter;

        HandleCameraMovement();

        engine->GetWorld().Update(engine, delta);

        #if 0 // bad performance on large meshes. need bvh
        //if (input_manager->IsButtonDown(MOUSE_BUTTON_LEFT) && ray_cast_timer > 1.0f) {
        //    ray_cast_timer = 0.0f;
            const auto &mouse_position = input_manager->GetMousePosition();

            const auto mouse_x = mouse_position.x.load();
            const auto mouse_y = mouse_position.y.load();

            const auto mouse_world = m_scene->GetCamera()->TransformScreenToWorld(
                Vector2(
                    mouse_x / static_cast<float>(input_manager->GetWindow()->width),
                    mouse_y / static_cast<float>(input_manager->GetWindow()->height)
                )
            );

            auto ray_direction = mouse_world.Normalized() * -1.0f;

            // std::cout << "ray direction: " << ray_direction << "\n";

            Ray ray{m_scene->GetCamera()->GetTranslation(), Vector3(ray_direction)};
            RayTestResults results;

            if (engine->GetWorld().GetOctree().TestRay(ray, results)) {
                // std::cout << "hit with aabb : " << results.Front().hitpoint << "\n";
                RayTestResults triangle_mesh_results;

                for (const auto &hit : results) {
                    // now ray test each result as triangle mesh to find exact hit point

                    if (auto lookup_result = engine->resources->entities.Lookup(Entity::ID{hit.id})) {
                        if (auto &mesh = lookup_result->GetMesh()) {
                            ray.TestTriangleList(
                                mesh->GetVertices(),
                                mesh->GetIndices(),
                                lookup_result->GetTransform(),
                                lookup_result->GetId().value,
                                triangle_mesh_results
                            );
                        }
                    }
                }

                if (!triangle_mesh_results.Empty()) {
                    const auto &mesh_hit = triangle_mesh_results.Front();

                    if (auto sphere = m_scene->GetRoot().Select("sphere")) {
                        sphere.SetLocalTranslation(mesh_hit.hitpoint);
                    }
                }
            }
        #endif
    }

    // virtual void OnInputEvent(Engine *engine, const SystemEvent &event) override;

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

    std::unique_ptr<Node> test_model, zombie, cube_obj, material_test_obj;
    GameCounter::TickUnit timer{};
    GameCounter::TickUnit ray_cast_timer{};
    std::atomic_uint32_t counter{0};

};
} // namespace hyperion::v2

int main()
{
    using namespace hyperion::renderer;

#if 0
    Profile control([]() {
        std::queue<TestStruct> test_queue;
        for (int i = 0; i < 1000; i++) {
            test_queue.push(TestStruct{i});
        }

        // std::cout << "Size: " << test_queue.size() << "\n";

        while (!test_queue.empty()) {
            test_queue.pop();
        }
    });
    // Profile profile_1([]() {
    //     std::queue<TestStruct> test_queue;
    //     for (int i = 0; i < 500; i++) {
    //         test_queue.push(TestStruct{i});
    //     }

    //     std::queue<TestStruct> test_queue_copy = test_queue;

    //     for (int i = 0; i < 5; i++) {
    //         while (!test_queue.empty()) {
    //             std::cout << "Front: " << test_queue.front().id << "\n";
    //             test_queue.pop();
    //         }
    //     }
    //     std::cout << "Size: " << test_queue_copy.size() << "\n";

    //     while (!test_queue_copy.empty()) {
    //         test_queue_copy.pop();
    //     }
    // });

    // Profile profile_2([]() {
    //     hyperion::Queue<TestStruct> test_queue;
    //     test_queue.Reserve(1000);
    //     for (int i = 0; i < 500; i++) {
    //         test_queue.Push(TestStruct{i});
    //     }

    //     hyperion::Queue<TestStruct> test_queue_copy = test_queue;

    //     for (int i = 0; i < 5; i++) {
    //         while (!test_queue.Empty()) {
    //             std::cout << "Front: " << test_queue.Front().id << "\n";
    //             test_queue.Pop();
    //         }
    //     }

    //     std::cout << "Size: " << test_queue_copy.Size() << "\n";

    //     while (!test_queue_copy.Empty()) {
    //         test_queue_copy.Pop();
    //     }
    // });


    
    Profile profile_1([]() {
        std::vector<TestStruct> test_queue;
        // test_queue.reserve(1000);
        for (int i = 0; i < 256; i++) {
            test_queue.push_back(TestStruct{i});
        }

        std::vector<TestStruct> test_queue_copy = test_queue;

        // for (int i = 0; i < 5; i++) {
            while (!test_queue.empty()) {
                // std::cout << "Back: " << test_queue.front().id << "\n";
                test_queue.pop_back();
            }
        // }
        // std::cout << "Size: " << test_queue_copy.size() << "\n";

        while (!test_queue_copy.empty()) {
            test_queue_copy.pop_back();
        }
    });

    Profile profile_2([]() {
        hyperion::DynArray<TestStruct> test_queue;
        // test_queue.Reserve(1000);
        for (int i = 0; i < 256; i++) {
            test_queue.PushBack(TestStruct{i});
        }
        // std::cout << "Size after pushing: " << test_queue.Size() << "\n";

        hyperion::DynArray<TestStruct> test_queue_copy = test_queue;

        // for (int i = 0; i < 5; i++) {
            while (!test_queue.Empty()) {
                // std::cout << "Back: " << test_queue.Front().id << "\n";
                test_queue.PopBack();
            }
        // }
        // std::cout << "back: " << test_queue_copy.Back().id << "\n";


        while (!test_queue_copy.Empty()) {
            test_queue_copy.PopBack();
        }
    });

    // profile_2.Run(10, 10);

    auto results = Profile::RunInterleved({ control, profile_1, profile_2 }, 5, 10, 1000);

    HYP_BREAKPOINT;
#endif
    
    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateSystemWindow("Hyperion Engine", 1024, 1024);//2048, 1080);
    system.SetCurrentWindow(window);

    SystemEvent event;

    auto *engine = new Engine(system, "My app");

    engine->assets.SetBasePath(FileSystem::Join(HYP_ROOT_DIR, "..", "res"));

    auto *my_game = new MyGame;
;
    engine->Initialize();

    Device *device = engine->GetInstance()->GetDevice();

    engine->shader_manager.SetShader(
        ShaderKey::BASIC_VEGETATION,
        Handle<Shader>(new Shader(
            std::vector<SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vegetation.vert.spv")).Read(),
                        {.name = "vegetation vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/forward_frag.spv")).Read(),
                        {.name = "forward frag"}
                    }
                }
            }
        ))
    );

    engine->shader_manager.SetShader(
        ShaderKey::DEBUG_AABB,
        Handle<Shader>(new Shader(
            std::vector<SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/aabb.vert.spv")).Read(),
                        {.name = "aabb vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/aabb.frag.spv")).Read(),
                        {.name = "aabb frag"}
                    }
                }
            }
        ))
    );

    engine->shader_manager.SetShader(
        ShaderKey::BASIC_FORWARD,
        Handle<Shader>(new Shader(
            std::vector<SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vert.spv")).Read(),
                        {.name = "main vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/forward_frag.spv")).Read(),
                        {.name = "forward frag"}
                    }
                }
            }
        ))
    );

    engine->shader_manager.SetShader(
        ShaderKey::TERRAIN,
        Handle<Shader>(new Shader(
            std::vector<SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vert.spv")).Read(),
                        {.name = "main vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/Terrain.frag.spv")).Read(),
                        {.name = "Terrain frag"}
                    }
                }
            }
        ))
    );
    

    // TODO: move elsewhere once RT stuff is established.

    
    // engine->shader_manager.SetShader(
    //     ShaderManager::Key::STENCIL_OUTLINE,
    //     Handle<Shader>(new Shader(
    //         std::vector<SubShader>{
    //             {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/outline.vert.spv")).Read()}},
    //             {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/outline.frag.spv")).Read()}}
    //         }
    //     ))
    // );


    {
        engine->shader_manager.SetShader(
            ShaderManager::Key::BASIC_SKYBOX,
            Handle<Shader>(new Shader(
                std::vector<SubShader>{
                    {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/skybox_vert.spv")).Read()}},
                    {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/skybox_frag.spv")).Read()}}
                }
            ))
        );
    }
    
    {
        auto translucent_renderer_instance = std::make_unique<RendererInstance>(
            Handle<Shader>(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD)),
            Handle<v2::RenderPass>(engine->GetRenderListContainer().Get(BUCKET_TRANSLUCENT).GetRenderPass()),
            RenderableAttributeSet(
                MeshAttributes {
                    .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
                },
                MaterialAttributes {
                    .bucket = Bucket::BUCKET_TRANSLUCENT
                }
            )
        );
        
        engine->AddRendererInstance(std::move(translucent_renderer_instance));
    }
    

    my_game->Init(engine, window);

#if HYPERION_VK_TEST_RAYTRACING
    auto rt_shader = std::make_unique<ShaderProgram>();
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_GEN, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rgen.spv")).Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_MISS, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rmiss.spv")).Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_CLOSEST_HIT, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rchit.spv")).Read()
    });

    auto rt = std::make_unique<RaytracingPipeline>(std::move(rt_shader));

    my_game->material_test_obj->GetChild(0).Get()->GetEntity()->SetTransform({{ 0, 7, 0 }});


    ProbeGrid probe_system({
        .aabb = {{-20.0f, -5.0f, -20.0f}, {20.0f, 5.0f, 20.0f}}
    });
    probe_system.Init(engine);

    auto my_tlas = std::make_unique<Tlas>();

    my_tlas->AddBlas(engine->resources->blas.Add(new Blas(
        engine->resources->meshes.IncRef(my_game->material_test_obj->GetChild(0).Get()->GetEntity()->GetMesh()),
        my_game->material_test_obj->GetChild(0).Get()->GetEntity()->GetTransform()
    )));
    
    my_tlas->AddBlas(engine->resources->blas.Add(new Blas(
        engine->resources->meshes.IncRef(my_game->cube_obj->GetChild(0).Get()->GetEntity()->GetMesh()),
        my_game->cube_obj->GetChild(0).Get()->GetEntity()->GetTransform()
    )));

    my_tlas->Init(engine);
    
    Image *rt_image_storage = new StorageImage(
        Extent3D{1024, 1024, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView rt_image_storage_view;

    auto *rt_descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING);
    rt_descriptor_set->AddDescriptor<TlasDescriptor>(0)
        ->SetSubDescriptor({.acceleration_structure = &my_tlas->Get()});
    rt_descriptor_set->AddDescriptor<StorageImageDescriptor>(1)
        ->SetSubDescriptor({.image_view = &rt_image_storage_view});
    
    auto rt_storage_buffer = rt_descriptor_set->AddDescriptor<StorageBufferDescriptor>(3);
    rt_storage_buffer->SetSubDescriptor({.buffer = my_tlas->Get().GetMeshDescriptionsBuffer()});

    HYPERION_ASSERT_RESULT(rt_image_storage->Create(
        device,
        engine->GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));
    HYPERION_ASSERT_RESULT(rt_image_storage_view.Create(engine->GetDevice(), rt_image_storage));
#endif

    engine->Compile();

#if HYPERION_VK_TEST_RAYTRACING
    HYPERION_ASSERT_RESULT(rt->Create(engine->GetDevice(), &engine->GetInstance()->GetDescriptorPool()));
#endif

    engine->game_thread.Start(engine, my_game, window);

    float tmp_render_timer = 0.0f;

    UInt num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    while (engine->IsRenderLoopActive()) {

        // input manager stuff
        while (SystemSDL::PollEvent(&event)) {
            my_game->HandleEvent(engine, event);
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

#if HYPERION_VK_TEST_RAYTRACING // for testing RT stuff.
        HYPERION_ASSERT_RESULT(engine->GetInstance()->GetFrameHandler()->PrepareFrame(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetSwapchain()
        ));

        frame = engine->GetInstance()->GetFrameHandler()->GetCurrentFrameData().Get<Frame>();
        auto *command_buffer = frame->GetCommandBuffer();
        const auto frame_index = engine->GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();

        engine->PreFrameUpdate(frame);

        /* === rendering === */
        HYPERION_ASSERT_RESULT(frame->BeginCapture(engine->GetInstance()->GetDevice()));

        my_game->OnFrameBegin(engine, frame);

        rt->Bind(frame->GetCommandBuffer());
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            frame->GetCommandBuffer(),
            rt.get(),
            {
                {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, .count = 1},
                {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
                {.offsets = {0, 0}}
            }
        );
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            frame->GetCommandBuffer(),
            rt.get(),
            {{
                .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
                .count = 1
            }}
        );
        rt->TraceRays(engine->GetDevice(), frame->GetCommandBuffer(), rt_image_storage->GetExtent());
        rt_image_storage->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );

        
        probe_system.RenderProbes(engine, frame->GetCommandBuffer());
        probe_system.ComputeIrradiance(engine, frame->GetCommandBuffer());

        engine->RenderDeferred(frame);
        

        engine->RenderFinalPass(frame);

        HYPERION_ASSERT_RESULT(frame->EndCapture(engine->GetInstance()->GetDevice()));
        HYPERION_ASSERT_RESULT(frame->Submit(&engine->GetInstance()->GetGraphicsQueue()));
        
        my_game->OnFrameEnd(engine, frame);

        engine->GetInstance()->GetFrameHandler()->PresentFrame(&engine->GetInstance()->GetGraphicsQueue(), engine->GetInstance()->GetSwapchain());
        engine->GetInstance()->GetFrameHandler()->NextFrame();

#else

        // simpler loop. this is what it will look like for RT too, soon enough
        engine->RenderNextFrame(my_game);
#endif
    }

    AssertThrow(engine->GetInstance()->GetDevice()->Wait());
    
#if HYPERION_VK_TEST_RAYTRACING

    rt_image_storage->Destroy(engine->GetDevice());
    rt_image_storage_view.Destroy(engine->GetDevice());
    //tlas->Destroy(engine->GetInstance());
    rt->Destroy(engine->GetDevice());
#endif


    // engine->Stop();

    delete my_game;

    delete engine;
    delete window;

    return 0;
}
