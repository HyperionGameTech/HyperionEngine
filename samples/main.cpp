
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
#include <util/img/Bitmap.hpp>

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
#include <rendering/rt/BlurRadiance.hpp>

#include <script/ScriptBindings.hpp>

#include <util/UTF8.hpp>

using namespace hyperion;
using namespace hyperion::v2;


#define HYPERION_VK_TEST_VCT 0
#define HYPERION_VK_TEST_RAYTRACING 1
#define HYPERION_RUN_TESTS 1

namespace hyperion::v2 {
    
class MyGame : public Game {

public:
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
        m_scene->SetCamera(engine->CreateHandle<Camera>(new FirstPersonCamera(//FollowCamera(
            //Vector3(0, 0, 0), Vector3(0, 0.5f, -2),
            2048, 2048,//2048, 1080,
            75.0f,
            0.5f, 30000.0f
        )));

        auto loaded_assets = engine->assets.Load<Node>(
            "models/ogrexml/dragger_Body.mesh.xml",
            "models/sponza/sponza.obj", //testbed/testbed.obj", //"San_Miguel/san-miguel-low-poly.obj", //"sibenik/sibenik.obj", //"
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
            sphere->GetChild(0).Get()->GetEntity()->GetInitInfo().flags &= ~Entity::ComponentInitInfo::Flags::ENTITY_FLAGS_RAY_TESTS_ENABLED;
            sphere->SetLocalTranslation(Vector3(2.0f + (i * 6.0f), 7.0f, -5.0f));
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

#if 0
        if (auto terrain_node = m_scene->GetRoot().AddChild()) {
            terrain_node.Get()->SetEntity(engine->CreateHandle<Entity>());
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
        material_test_obj->Scale(6.45f);
        material_test_obj->Translate(Vector3(0, 9, 0));
        engine->InitObject(material_test_obj->GetChild(0).Get()->GetEntity());
        material_test_obj->GetChild(0).Get()->GetEntity()->CreateBLAS();
        m_scene->GetRoot().AddChild(NodeProxy(material_test_obj.release()));

        // remove textures so we can manipulate the material and see our changes easier
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ROUGHNESS_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_METALNESS_MAP, nullptr);
        
        auto cubemap = engine->CreateHandle<Texture>(new TextureCube(
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
        engine->InitObject(cubemap);

        zombie->GetChild(0).Get()->GetEntity()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
        zombie->Scale(1.25f);
        zombie->Translate({0, 0, -5});
        zombie->GetChild(0).Get()->GetEntity()->GetController<AnimationController>()->Play(1.0f, LoopMode::REPEAT);
        // zombie->GetChild(0)->GetEntity()->AddController<AABBDebugController>(engine);
        // m_scene->GetRoot().AddChild(NodeProxy(zombie.release()));

        //zombie->GetChild(0)->GetEntity()->GetSkeleton()->FindBone("thigh.L")->SetLocalRotation(Quaternion({1.0f, 0.0f, 0.0f}, MathUtil::DegToRad(90.0f)));
        //zombie->GetChild(0)->GetEntity()->GetSkeleton()->GetRootBone()->UpdateWorldTransform();

        auto my_light = engine->CreateHandle<Light>(new DirectionalLight(
            Vector3(-0.1f, 1.0f, 0.0f).Normalize(),
            Vector4::One(),
            150000.0f
        ));
        m_scene->GetEnvironment()->AddLight(Handle<Light>(my_light));

        m_point_light = engine->CreateHandle<Light>(new PointLight(
            Vector3(0.0f, 6.0f, 0.0f),
            Vector4(1.0f, 0.3f, 0.1f, 1.0f),
            1000.0f,
            4.0f
        ));

        // m_scene->GetEnvironment()->AddLight(Handle<Light>(m_point_light));
        //test_model->Scale(10.0f);
        test_model->Scale(0.15f);//14.075f);


#if HYPERION_VK_TEST_VCT
        m_scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(
            VoxelConeTracing::Params {
                BoundingBox(-128, 128)//test_model->GetWorldAABB()
            }
        );
#endif

        // test_model->Translate(Vector3(0, 0, -25.0f));

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
        }

        auto quad = engine->CreateHandle<Mesh>(MeshBuilder::NormalizedCubeSphere(8).release());//MeshBuilder::DividedQuad(8).release());    //MeshBuilder::Quad());
        // quad->SetVertexAttributes(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes);
        auto quad_spatial = engine->CreateHandle<Entity>(
            std::move(quad),
            Handle<Shader>(engine->shader_manager.GetShader(ShaderKey::BASIC_FORWARD)),
            engine->CreateHandle<Material>()
        );
    
        engine->InitObject(quad_spatial);
        quad_spatial->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f));//0.00f, 0.4f, 0.9f, 1.0f));
        quad_spatial->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.2f);
        quad_spatial->SetScale(Vector3(3.0f));
        quad_spatial->SetRotation(Quaternion(Vector3(1, 1, 1), MathUtil::DegToRad(-40.0f)));
        quad_spatial->SetTranslation(Vector3(0, 12.0f, 0));
        // m_scene->AddEntity(std::move(quad_spatial));
        
        m_scene->GetEnvironment()->AddRenderComponent<ShadowRenderer>(
            Handle<Light>(my_light),
            Vector3(0, 0, 0),
            250.0f
        );

        m_scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
            renderer::Extent2D {128, 128},
            BoundingBox { Vector(-128, -10, -128), Vector(128, 100, 128) },
            renderer::Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
        );
        m_scene->ForceUpdate();


        cube_obj->Scale(50.0f);

        auto skybox_material = engine->CreateHandle<Material>();
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

        m_scene->AddEntity(Handle<Entity>(cube_obj->GetChild(0).Get()->GetEntity()));

        auto monkey = engine->assets.Load<Node>("models/monkey/monkey.obj");

        monkey->GetChild(0).Get()->GetEntity()->AddController<ScriptedController>(engine->assets.Load<Script>("scripts/examples/controller.hypscript"));
        monkey->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.175f);
        monkey->Translate(Vector3(0, 22.5f, 0));
        monkey->Scale(4.0f);
        m_scene->GetRoot().AddChild(NodeProxy(monkey.release()));

        for (auto &x : m_scene->GetRoot().GetChildren()) {
            DebugLog(LogType::Debug, "%s\n", x.GetName().Data());
        }
    }

    virtual void Teardown(Engine *engine) override
    {
        Game::Teardown(engine);
    }

    virtual void OnFrameBegin(Engine *engine, Frame *frame) override
    {
        m_scene->GetEnvironment()->RenderComponents(engine, frame);

        engine->render_state.BindScene(m_scene.Get());
    }

    virtual void OnFrameEnd(Engine *engine, Frame *) override
    {
        engine->render_state.UnbindScene();
    }

    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) override
    {
        timer += delta;

        HandleCameraMovement();

        #if 1 // bad performance on large meshes. need bvh
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

            if (engine->GetWorld().GetOctree().TestRay(ray, results)) {
                // std::cout << "hit with aabb : " << results.Front().hitpoint << "\n";
                RayTestResults triangle_mesh_results;

                for (const auto &hit : results) {
                    // now ray test each result as triangle mesh to find exact hit point
                    if (auto lookup_result = engine->registry.template Lookup<Entity>(Entity::ID { TypeID::ForType<Entity>(), hit.id })) {
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

};
} // namespace hyperion::v2


template <class T>
using RC = RefCountedPtr<T>;
template <class T>
using Weak = WeakRefCountedPtr<T>;

int main()
{
    using namespace hyperion::renderer;


#if 0

    // Profile profile_1([]() {
    //     std::unordered_map<int, unsigned> items;
        
    //     for (int i = 0; i < 100; i++) {
    //         items[i] = static_cast<unsigned>(i) & static_cast<unsigned>(i * 2);
    //     }
        
    //     for (auto &it : items) {
    //         std::cout << it.first << ": " << it.second << "\n";
    //     }
    // });
    // Profile profile_2([]() {
    //     FlatMap<int, int> items;
        
    //     for (int i = 0; i < 100; i++) {
    //         items[i] = static_cast<unsigned>(i) & static_cast<unsigned>(i * 2);
    //     }
        
    //     for (auto &it : items) {
    //         std::cout << it.first << ": " << it.second << "\n";
    //     }
    // });

    Profile profile_1([]() {
        std::queue<Proc<void>> items;
        for (int i = 0; i < 1024; i++) {
            EntityDrawProxy ent;
            ent.entity_id.value = i;
            items.push([ent]() {
                std::cout << "I am item number " << ent.entity_id.value << "\n";
            });
        }
        
        while (!items.empty()) {
            items.front()();
            items.pop();
        }
    });

    Profile profile_2([]() {
        std::queue<std::function<void()>> items;
        for (int i = 0; i < 1024; i++) {
            EntityDrawProxy ent;
            ent.entity_id.value = i;
            items.push([ent]() {
                std::cout << "I am item number " << ent.entity_id.value << "\n";
            });
        }
        
        while (!items.empty()) {
            items.front()();
            items.pop();
        }
    });

    auto results = Profile::RunInterleved({ profile_1, profile_2, profile_1, profile_2 }, 5, 5, 30);

    HYP_BREAKPOINT;
#endif
    
    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateSystemWindow("Hyperion Engine", 1024, 1024);
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
        engine->CreateHandle<Shader>(
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
        )
    );

    engine->shader_manager.SetShader(
        ShaderKey::DEBUG_AABB,
        engine->CreateHandle<Shader>(
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
        )
    );

    engine->shader_manager.SetShader(
        ShaderKey::BASIC_FORWARD,
        engine->CreateHandle<Shader>(
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
        )
    );

    engine->shader_manager.SetShader(
        ShaderKey::TERRAIN,
        engine->CreateHandle<Shader>(
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
        )
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
            engine->CreateHandle<Shader>(
                std::vector<SubShader>{
                    {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/skybox_vert.spv")).Read()}},
                    {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/skybox_frag.spv")).Read()}}
                }
            )
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
    AssertThrow(my_game->GetScene()->CreateTLAS());

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

    auto tmp_asset = engine->assets.Load<Node>("models/ogrexml/dragger_Body.mesh.xml");

    auto cube_obj = std::move(tmp_asset->GetChild(0).Get()->GetEntity()->GetMesh());//MeshBuilder::NormalizedCubeSphere(16).release());
    auto cube_obj2 = engine->CreateHandle<Mesh>(MeshBuilder::Cube().release());
    auto cube_obj3 = engine->CreateHandle<Mesh>(MeshBuilder::Cube().release());
    auto cube_obj4 = engine->CreateHandle<Mesh>(MeshBuilder::Cube().release());


    ProbeGrid probe_system({
        .aabb = {{-20.0f, -5.0f, -20.0f}, {20.0f, 5.0f, 20.0f}}
    });
    probe_system.Init(engine);
    
    auto my_material = engine->CreateHandle<Material>();
    my_material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 1.0f);
    my_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, engine->CreateHandle<Texture>(engine->assets.Load<Texture>("textures/grass.jpg").release()));
    my_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    engine->InitObject(my_material);

    auto my_material2 = engine->CreateHandle<Material>();
    my_material2->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.1f);
    my_material2->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.0f, 1.0f, 0.0f, 1.0f));
    engine->InitObject(my_material2);
    
    
    my_game->GetScene()->GetTLAS()->AddBLAS(engine->CreateHandle<BLAS>(
        std::move(cube_obj),
        Handle<Material>(my_material),
        Transform { Vector3 { 0, 7, 0 } }
    ));

    my_game->GetScene()->GetTLAS()->AddBLAS(engine->CreateHandle<BLAS>(
        std::move(cube_obj2),
        std::move(my_material2),
        Transform { Vector3 { 0, 7, 4 } }
    ));

    my_game->GetScene()->GetTLAS()->AddBLAS(engine->CreateHandle<BLAS>(
        std::move(cube_obj3),
        engine->CreateHandle<Material>(),
        Transform { Vector3 { 4, 7, 0 } }
    ));

    my_game->GetScene()->GetTLAS()->AddBLAS(engine->CreateHandle<BLAS>(
        std::move(cube_obj4),
        engine->CreateHandle<Material>(),
        Transform { Vector3 { 4, 7, 4 } }
    ));

    engine->InitObject(my_material);

    engine->InitObject(my_game->GetScene()->GetTLAS());


    //my_tlas->Get().GetBlas()[0]->GetGeometries()[0]->SetMaterialIndex(1);
    //my_tlas->Get().GetBlas()[0]->SetFlag(AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);

    Image *rt_image_storage = new StorageImage(
        Extent3D{1024, 1024, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView rt_image_storage_view;

    
    Image *rt_normals_roughness_weight = new StorageImage(
        Extent3D{1024, 1024, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );
    ImageView rt_normals_roughness_weight_view;

    
    Image *rt_depth_image = new StorageImage(
        Extent3D{1024, 1024, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R32F,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView rt_depth_image_view;

    auto *rt_descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING);
    rt_descriptor_set->AddDescriptor<TlasDescriptor>(0)
        ->SetSubDescriptor({ .acceleration_structure = &my_game->GetScene()->GetTLAS()->Get() });
    rt_descriptor_set->AddDescriptor<StorageImageDescriptor>(1)
        ->SetSubDescriptor({ .image_view = &rt_image_storage_view });
    rt_descriptor_set->AddDescriptor<StorageImageDescriptor>(2)
        ->SetSubDescriptor({ .image_view = &rt_normals_roughness_weight_view });
    rt_descriptor_set->AddDescriptor<StorageImageDescriptor>(3)
        ->SetSubDescriptor({ .image_view = &rt_depth_image_view });
    
    // mesh descriptions
    auto rt_storage_buffer = rt_descriptor_set->AddDescriptor<StorageBufferDescriptor>(4);
    rt_storage_buffer->SetSubDescriptor({ .buffer = my_game->GetScene()->GetTLAS()->Get().GetMeshDescriptionsBuffer() });

    // materials
    auto rt_material_buffer = rt_descriptor_set->AddDescriptor<StorageBufferDescriptor>(5);
    rt_material_buffer->SetSubDescriptor({
        .buffer = engine->shader_globals->materials.GetBuffers()[0].get()
    });

    HYPERION_ASSERT_RESULT(rt_image_storage->Create(
        device,
        engine->GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));
    HYPERION_ASSERT_RESULT(rt_image_storage_view.Create(engine->GetDevice(), rt_image_storage));

    HYPERION_ASSERT_RESULT(rt_normals_roughness_weight->Create(
        device,
        engine->GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));
    HYPERION_ASSERT_RESULT(rt_normals_roughness_weight_view.Create(engine->GetDevice(), rt_normals_roughness_weight));

    HYPERION_ASSERT_RESULT(rt_depth_image->Create(
        device,
        engine->GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));
    HYPERION_ASSERT_RESULT(rt_depth_image_view.Create(engine->GetDevice(), rt_depth_image));



#endif

    engine->Compile();

#if HYPERION_VK_TEST_RAYTRACING
    HYPERION_ASSERT_RESULT(rt->Create(engine->GetDevice(), &engine->GetInstance()->GetDescriptorPool()));
#endif

    
    BlurRadiance blur_radiance(Extent2D { 1024, 1024 }, &rt_image_storage_view, &rt_normals_roughness_weight_view, &rt_depth_image_view);
    blur_radiance.Create(engine);


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

        auto *frame = engine->GetInstance()->GetFrameHandler()->GetCurrentFrameData().Get<Frame>();
        auto *command_buffer = frame->GetCommandBuffer();
        const auto frame_index = engine->GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();

        engine->PreFrameUpdate(frame);

        /* === rendering === */
        HYPERION_ASSERT_RESULT(frame->BeginCapture(engine->GetInstance()->GetDevice()));

        // set visibility cursor to previous octree visibility cursor (atomic, relaxed)
        engine->GetWorld().Render(engine, frame);

        my_game->OnFrameBegin(engine, frame);

        rt->Bind(frame->GetCommandBuffer());
    
        const auto scene_binding = engine->render_state.GetScene().id;
        const auto scene_index = scene_binding ? scene_binding.value - 1 : 0u;

        frame->GetCommandBuffer()->BindDescriptorSet(
            engine->GetInstance()->GetDescriptorPool(),
            rt.get(),
            DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()],
            DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
            FixedArray {
                UInt32(sizeof(v2::SceneShaderData) * scene_index),
                UInt32(sizeof(v2::LightShaderData) * 0)
            }
        );

        frame->GetCommandBuffer()->BindDescriptorSet(
            engine->GetInstance()->GetDescriptorPool(),
            rt.get(),
            DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, frame->GetFrameIndex()),
            DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
        );

        frame->GetCommandBuffer()->BindDescriptorSet(
            engine->GetInstance()->GetDescriptorPool(),
            rt.get(),
            DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
            DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING
        );
        
        rt_image_storage->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );
        rt_normals_roughness_weight->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );
        rt_depth_image->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );
        rt->TraceRays(engine->GetDevice(), frame->GetCommandBuffer(), rt_image_storage->GetExtent());
        rt_image_storage->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );
        rt_normals_roughness_weight->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );
        rt_depth_image->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );

        blur_radiance.Render(engine, frame);

        probe_system.RenderProbes(engine, frame);
        probe_system.ComputeIrradiance(engine, frame);

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
