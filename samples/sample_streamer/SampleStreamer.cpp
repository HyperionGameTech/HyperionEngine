
#include "SampleStreamer.hpp"

#include <core/system/AppContext.hpp>
#include <core/system/Debug.hpp>

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <Engine.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/AudioComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TerrainComponent.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/components/RigidBodyComponent.hpp>
#include <scene/ecs/components/BLASComponent.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>
#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/PointLightShadowRenderer.hpp>
#include <core/utilities/Pair.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/Queue.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/system/ArgParse.hpp>
#include <util/json/JSON.hpp>
#include <rendering/lightmapper/LightmapRenderer.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <core/net/Socket.hpp>
#include <core/system/SystemEvent.hpp>

#include <Game.hpp>

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/ByteWriter.hpp>

#include <rendering/render_components/ScreenCapture.hpp>

#include <rendering/font/FontFace.hpp>
#include <rendering/font/FontAtlas.hpp>

#include <scene/camera/FirstPersonCamera.hpp>
#include <scene/camera/CameraTrackController.hpp>

#include <util/MeshBuilder.hpp>

#include <asset/model_loaders/PLYModelLoader.hpp>

#include <util/profiling/Profile.hpp>

#include "rendering/RenderEnvironment.hpp"
#include "rendering/UIRenderer.hpp"
#include <rendering/GaussianSplatting.hpp>

#include <util/UTF8.hpp>

#include <rtc/RTCClientList.hpp>
#include <rtc/RTCClient.hpp>
#include <rtc/RTCDataChannel.hpp>

// static void CollectMeshes(NodeProxy node, Array<Pair<Handle<Mesh>, Transform>> &out)
// {
//     const auto &entity = node.GetEntity();

//     if (entity) {
//         const auto &mesh = entity->GetMesh();

//         if (mesh) {
//             out.PushBack(Pair<Handle<Mesh>, Transform> { mesh, entity->GetTransform() });
//         }
//     }

//     for (auto &child : node.GetChildren()) {
//         CollectMeshes(child, out);
//     }
// }


SampleStreamer::SampleStreamer()
    : Game(ManagedGameInfo {
          "csharp.dll",
          "TestGame"
      })
{
}

void SampleStreamer::Init()
{
    Game::Init();

#if 0
    ArgParse args;
    args.Add("SignallingServerIP", "s", ArgParse::ARG_FLAGS_REQUIRED, CommandLineArgumentType::ARGUMENT_TYPE_STRING);
    args.Add("SignallingServerPort", "p", ArgParse::ARG_FLAGS_REQUIRED, CommandLineArgumentType::ARGUMENT_TYPE_INT);

    auto arg_parse_result = args.Parse(GetAppContext()->GetArguments());
    if (arg_parse_result.ok) {
        for (const auto &arg : arg_parse_result.values) {
            const TypeID type_id = arg.second.GetTypeID();

            if (type_id == TypeID::ForType<String>()) {
                DebugLog(LogType::Debug, "Argument %s = %s\n", arg.first.Data(), arg.second.Get<String>().Data());
            } else if (type_id == TypeID::ForType<int>()) {
                DebugLog(LogType::Debug, "Argument %s = %d\n", arg.first.Data(), arg.second.Get<int>());
            } else if (type_id == TypeID::ForType<float>()) {
                DebugLog(LogType::Debug, "Argument %s = %f\n", arg.first.Data(), arg.second.Get<float>());
            } else if (type_id == TypeID::ForType<bool>()) {
                DebugLog(LogType::Debug, "Argument %s = %s\n", arg.first.Data(), arg.second.Get<bool>() ? "true" : "false");
            } else {
                DebugLog(LogType::Debug, "Argument %s = <unknown>\n", arg.first.Data());
            }
        }

        const String signalling_server_ip = arg_parse_result["SignallingServerIP"].Get<String>();
        const uint16 signalling_server_port = uint16(arg_parse_result["SignallingServerPort"].Get<int>());

        // g_engine->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();

        m_rtc_instance.Reset(new RTCInstance(
            RTCServerParams {
                RTCServerAddress { signalling_server_ip, signalling_server_port, "/server" }
            }
        ));

        m_rtc_stream = m_rtc_instance->CreateStream(
            RTCStreamType::RTC_STREAM_TYPE_VIDEO,
            UniquePtr<RTCStreamEncoder>(new GStreamerRTCStreamVideoEncoder())
        );

        m_rtc_stream->Start();

        AssertThrow(m_rtc_instance->GetServer() != nullptr);

        if (const RC<RTCServer> &server = m_rtc_instance->GetServer()) {
            server->GetCallbacks().On(RTCServerCallbackMessages::ERR, [](RTCServerCallbackData data)
            {
                DebugLog(LogType::Error, "Server error: %s\n", data.error.HasValue() ? data.error.Get().message.Data() : "<unknown>");
            });

            server->GetCallbacks().On(RTCServerCallbackMessages::CONNECTED, [](RTCServerCallbackData)
            {
                DebugLog(LogType::Debug, "Server started\n");
            });

            server->GetCallbacks().On(RTCServerCallbackMessages::DISCONNECTED, [](RTCServerCallbackData)
            {
                DebugLog(LogType::Debug, "Server stopped\n");
            });

            server->GetCallbacks().On(RTCServerCallbackMessages::MESSAGE, [this](RTCServerCallbackData data)
            {
                using namespace hyperion::json;

                if (!data.bytes.HasValue()) {
                    DebugLog(LogType::Warn, "Received client message, but no bytes were provided\n");

                    return;
                }

                const ByteBuffer &bytes = data.bytes.Get();

                auto json_parse_result = JSON::Parse(String(bytes));

                if (!json_parse_result.ok) {
                    DebugLog(LogType::Warn, "Failed to parse JSON from client message: %s\n", json_parse_result.message.Data());

                    return;
                }

                JSONValue &json_value = json_parse_result.value;

                DebugLog(LogType::Debug, " -> %s\n", json_value.ToString().Data());

                m_message_queue.Push(std::move(json_value));
            });

            server->Start();
        }
    }
#endif

    // // Test freetype font rendering
    auto font_face_asset = AssetManager::GetInstance()->Load<RC<FontFace>>("fonts/Roboto/Roboto-Regular.ttf");

    if (font_face_asset.IsOK()) {
        RC<FontAtlas> atlas(new FontAtlas(font_face_asset.Result()));
        atlas->Render();

        // atlas.RenderSync();

        // ByteBuffer atlas_byte_buffer;
        // atlas.WriteToBuffer(atlas_byte_buffer);

        GetUIStage()->SetDefaultFontAtlas(atlas);

        // auto font_bitmap = atlas->GenerateBitmap();
        // font_bitmap.Write("font_bitmap.bmp");
        
        auto font_metadata_json = atlas->GenerateMetadataJSON("font_bitmap.bmp");

        FileByteWriter bw("res/fonts/default.json");
        bw.WriteString(font_metadata_json.ToString(true) + '\n');
        bw.Close();
    }


    const Extent2D window_size = GetInputManager()->GetWindow()->GetDimensions();

    m_texture = CreateObject<Texture>(Texture2D(
        window_size,
        InternalFormat::RGBA8,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    ));
    InitObject(m_texture);
    
    DebugLog(LogType::Debug, "SampleStreamer::Init : Scene ID = %u\n", m_scene.GetID().Value());
    m_scene->SetCamera(CreateObject<Camera>(
        70.0f,
        window_size.width, window_size.height,
        0.01f, 30000.0f
    ));

    /*auto particle_spawner = CreateObject<ParticleSpawner>(ParticleSpawnerParams {
        .texture = AssetManager::GetInstance()->Load<Texture>("textures/spark.png"),
        .max_particles = 1000,
        .origin = Vector3(0.0f, 0.0f, 0.0f),
        .start_size = 0.05f,
        .radius = 1.0f,
        .randomness = 0.5f,
        .lifespan = 1.0f,
        .has_physics = true
    });

    InitObject(particle_spawner);

    m_scene->GetEnvironment()->GetParticleSystem()->GetParticleSpawners().Add(particle_spawner);*/

    /*m_scene->GetCamera()->SetCameraController(UniquePtr<FollowCameraController>::Construct(
        Vector3(0.0f, 7.0f, 0.0f), Vector3(0.0f, 0.0f, 5.0f)
    ));*/
    m_scene->GetCamera()->SetCameraController(RC<CameraController>(new FirstPersonCameraController()));

    if (false) {
        auto gun_asset = AssetManager::GetInstance()->Load<Node>("models/gun/AK47NoSubdiv.obj");

        if (gun_asset.IsOK()) {
            NodeProxy gun = gun_asset.Result();

            auto gun_parent = m_scene->GetRoot()->AddChild();
            gun_parent.SetName("gun");

            gun.SetLocalScale(0.25f);
            gun.SetLocalRotation(Quaternion(Vec3f(0.0f, 1.0f, 0.0f), M_PI));
            gun_parent->AddChild(gun);

            m_scene->GetEntityManager()->AddComponent(gun[0].GetEntity(), BLASComponent { });
        }
    }

    if (false) {
        auto box_asset = AssetManager::GetInstance()->Load<Node>("models/cube.obj");

        if (box_asset.IsOK()) {
            NodeProxy box = box_asset.Result();

            m_scene->GetRoot()->AddChild(box);

            MeshComponent &mesh_component = m_scene->GetEntityManager()->GetComponent<MeshComponent>(box[0].GetEntity());
            mesh_component.material = MaterialCache::GetInstance()->GetOrCreate({
               .shader_definition = ShaderDefinition {
                   HYP_NAME(Forward),
                   ShaderProperties(static_mesh_vertex_attributes)
                },
                .bucket = Bucket::BUCKET_OPAQUE
            });

            m_scene->GetEntityManager()->AddComponent(box[0].GetEntity(), BLASComponent { });
        }
    }

    if (false) {
        auto cube_node = m_scene->GetRoot()->AddChild();
        cube_node.SetName("TestCube");

        auto entity_id = m_scene->GetEntityManager()->AddEntity();
        cube_node.SetEntity(entity_id);

        auto cube = MeshBuilder::Cube();
        InitObject(cube);

        auto material = MaterialCache::GetInstance()->GetOrCreate(
            {
                .shader_definition = ShaderDefinition {
                    HYP_NAME(Forward),
                    ShaderProperties(static_mesh_vertex_attributes)
                },
                .bucket = Bucket::BUCKET_OPAQUE
            },
            {
                { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f, 0.0f, 0.0f, 1.0f) },
                { Material::MATERIAL_KEY_METALNESS, 0.0f },
                { Material::MATERIAL_KEY_ROUGHNESS, 0.01f }
            }
        );

        m_scene->GetEntityManager()->AddComponent(entity_id, MeshComponent {
            cube,
            material
        });
        m_scene->GetEntityManager()->AddComponent(entity_id, BoundingBoxComponent {
            cube->GetAABB()
        });
        m_scene->GetEntityManager()->AddComponent(entity_id, VisibilityStateComponent { });
        m_scene->GetEntityManager()->AddComponent(entity_id, BLASComponent { });
    }

    if (false) {

        auto cube_node = m_scene->GetRoot()->AddChild();
        cube_node.SetName("TestCube2");
        cube_node.Scale(1.05f);
        cube_node.SetWorldTranslation(Vec3f { 0.0f, 150.0f, 0.0f });

        auto entity_id = m_scene->GetEntityManager()->AddEntity();
        cube_node.SetEntity(entity_id);

        auto cube = MeshBuilder::Cube();
        InitObject(cube);

        // add physics
        m_scene->GetEntityManager()->AddComponent(entity_id, RigidBodyComponent {
            CreateObject<physics::RigidBody>(
                RC<physics::PhysicsShape>(new physics::BoxPhysicsShape(BoundingBox { Vec3f { -1.0f }, Vec3f { 1.0f } })),
                physics::PhysicsMaterial {
                    .mass = 1.0f
                }
            )
        });

        m_scene->GetEntityManager()->AddComponent(entity_id, MeshComponent {
            cube,
            MaterialCache::GetInstance()->GetOrCreate(
                {
                    .shader_definition = ShaderDefinition {
                        HYP_NAME(Forward),
                        ShaderProperties(static_mesh_vertex_attributes)
                    },
                    .bucket = Bucket::BUCKET_OPAQUE
                },
                {
                    { Material::MATERIAL_KEY_ALBEDO, Vec4f(0.0f, 0.0f, 1.0f, 1.0f) },
                    { Material::MATERIAL_KEY_METALNESS, 0.0f },
                    { Material::MATERIAL_KEY_ROUGHNESS, 0.01f }
                }
            )
        });
        m_scene->GetEntityManager()->AddComponent(entity_id, BoundingBoxComponent {
            cube->GetAABB()
        });
        m_scene->GetEntityManager()->AddComponent(entity_id, VisibilityStateComponent { });
    }

    // Used for RTC streaming or for editor view.
    // Has a performance impact due to copying the framebuffer.
    auto streaming_capture_component = m_scene->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(HYP_NAME(StreamingCapture), window_size);

    if (false) {
        auto terrain_node = m_scene->GetRoot()->AddChild();
        auto terrain_entity = m_scene->GetEntityManager()->AddEntity();

        // MeshComponent
        m_scene->GetEntityManager()->AddComponent(terrain_entity, MeshComponent {
            Handle<Mesh> { },
            MaterialCache::GetInstance()->GetOrCreate({
                .shader_definition = ShaderDefinition {
                    HYP_NAME(Terrain),
                    ShaderProperties(static_mesh_vertex_attributes)
                },
                .bucket = Bucket::BUCKET_OPAQUE
            })
        });

        // TerrainComponent
        m_scene->GetEntityManager()->AddComponent(terrain_entity, TerrainComponent {
        });

        terrain_node.SetEntity(terrain_entity);
        terrain_node.SetName("TerrainNode");
    }

    if (true) { // add sun
        auto sun = CreateObject<Light>(DirectionalLight(
            Vec3f(-0.1f, 0.65f, 0.1f).Normalize(),
            Color(1.0f),
            4.0f
        ));

        InitObject(sun);

        auto sun_node = m_scene->GetRoot()->AddChild();
        sun_node.SetName("Sun");

        auto sun_entity = m_scene->GetEntityManager()->AddEntity();
        sun_node.SetEntity(sun_entity);

        sun_node.SetWorldTranslation(Vec3f { -0.1f, 0.65f, 0.1f });

        m_scene->GetEntityManager()->AddComponent(sun_entity, LightComponent {
            sun
        });

        m_scene->GetEntityManager()->AddComponent(sun_entity, ShadowMapComponent {
            .mode       = ShadowMode::PCF,
            .radius     = 18.0f,
            .resolution = { 2048, 2048 }
        });
    }

    Array<Handle<Light>> point_lights;

    // point_lights.PushBack(CreateObject<Light>(PointLight(
    //    Vector3(-5.0f, 0.5f, 0.0f),
    //    Color(1.0f, 0.0f, 0.0f),
    //    1.0f,
    //    5.0f
    // )));
    //point_lights.PushBack(CreateObject<Light>(PointLight(
    //    Vector3(5.0f, 2.0f, 0.0f),
    //    Color(0.0f, 1.0f, 0.0f),
    //    1.0f,
    //    15.0f
    //)));

    for (auto &light : point_lights) {
        auto point_light_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent(point_light_entity, ShadowMapComponent { });

        m_scene->GetEntityManager()->AddComponent(point_light_entity, TransformComponent {
            Transform(
                light->GetPosition(),
                Vec3f(1.0f),
                Quaternion::Identity()
            )
        });

        m_scene->GetEntityManager()->AddComponent(point_light_entity, LightComponent {
            light
        });
    }

    { // Add test spotlight
        auto spotlight = CreateObject<Light>(SpotLight(
            Vec3f(0.0f, 0.1f, 0.0f),
            Vec3f(-1.0f, 0.0f, 0.0f).Normalize(),
            Color(0.0f, 1.0f, 0.0f),
            2.0f,
            15.0f,
            Vec2f { MathUtil::Cos(MathUtil::DegToRad(50.0f)), MathUtil::Cos(MathUtil::DegToRad(10.0f)) }
        ));

        InitObject(spotlight);

        auto spotlight_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent(spotlight_entity, TransformComponent {
            Transform(
                spotlight->GetPosition(),
                Vec3f(1.0f),
                Quaternion::Identity()
            )
        });

        m_scene->GetEntityManager()->AddComponent(spotlight_entity, LightComponent {
            spotlight
        });
    }

    if (false) { // add test area light
        auto light = CreateObject<Light>(RectangleLight(
            Vec3f(0.0f, 1.25f, 0.0f),
            Vec3f(0.0f, 0.0f, -1.0f).Normalize(),
            Vec2f(2.0f, 2.0f),
            Color(1.0f, 0.0f, 0.0f),
            1.0f
        ));

        Handle<Texture> dummy_light_texture;

        if (auto dummy_light_texture_asset = AssetManager::GetInstance()->Load<Texture>("textures/dummy.jpg")) {
            dummy_light_texture = dummy_light_texture_asset.Result();
        }

        light->SetMaterial(MaterialCache::GetInstance()->GetOrCreate(
            {
               .shader_definition = ShaderDefinition {
                    HYP_NAME(Forward),
                    ShaderProperties(static_mesh_vertex_attributes)
                },
               .bucket = Bucket::BUCKET_OPAQUE
            },
            {
            },
            {
                {
                    Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP,
                    std::move(dummy_light_texture)
                }
            }
        ));
        AssertThrow(light->GetMaterial().IsValid());

        InitObject(light);

        auto area_light_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent(area_light_entity, TransformComponent {
            Transform(
                light->GetPosition(),
                Vec3f(1.0f),
                Quaternion::Identity()
            )
        });

        m_scene->GetEntityManager()->AddComponent(area_light_entity, LightComponent {
            light
        });
    }

    // add sample model
    {
        auto batch = AssetManager::GetInstance()->CreateBatch();
        batch->Add("test_model", "models/sponza/sponza.obj");//pica_pica/pica_pica.obj");////living_room/living_room.obj");//
        batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
        // batch->Add("cart", "models/coffee_cart/coffee_cart.obj");
        batch->LoadAsync();
        auto results = batch->AwaitResults();

        if (true) {
            auto plane_node = m_scene->GetRoot()->AddChild();
            plane_node.Rotate(Quaternion(Vec3f(1.0f, 0.0f, 0.0f), -M_PI_2));
            plane_node.Scale(1.0f);
            plane_node.Translate(Vec3f(0.0f, 2.0f, 0.0f));

            auto plane_entity = m_scene->GetEntityManager()->AddEntity();
            plane_node.SetEntity(plane_entity);

            auto mesh = MeshBuilder::Quad();
            InitObject(mesh);

            m_scene->GetEntityManager()->AddComponent(plane_entity, MeshComponent {
                mesh,
                MaterialCache::GetInstance()->GetOrCreate(
                    {
                        .shader_definition = ShaderDefinition {
                            HYP_NAME(Forward),
                            ShaderProperties(static_mesh_vertex_attributes, { { "FORWARD_LIGHTING" } })
                        },
                        .bucket = Bucket::BUCKET_TRANSLUCENT
                    },
                    {
                        { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f, 0.0f, 0.0f, 1.0f) },
                        { Material::MATERIAL_KEY_METALNESS, 0.0f },
                        { Material::MATERIAL_KEY_ROUGHNESS, 0.1f },
                        { Material::MATERIAL_KEY_TRANSMISSION, 0.9f }
                    }
                    // {
                    //     {
                    //         Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP,
                    //         AssetManager::GetInstance()->Load<Texture>("textures/dummy.jpg")
                    //     }
                    // }
                )
            });

            m_scene->GetEntityManager()->AddComponent(plane_entity, BoundingBoxComponent {
                mesh->GetAABB()
            });

            m_scene->GetEntityManager()->AddComponent(plane_entity, VisibilityStateComponent { });

            // m_scene->GetEntityManager()->AddComponent(plane_entity, RigidBodyComponent {
            //     CreateObject<physics::RigidBody>(
            //         RC<physics::PhysicsShape>(new physics::BoxPhysicsShape(
            //             BoundingBox { Vec3f(-10.0f, -0.1f, -10.0f), Vec3f(10.0f, 0.1f, 10.0f) }
            //         )),
            //         physics::PhysicsMaterial {
            //             .mass = 0.0f // static
            //         }
            //     )
            // });
        }

#if 0
        if (auto zombie = results["zombie"].ExtractAs<Node>()) {
            zombie.Scale(0.25f);
            auto zombie_entity = zombie[0].GetEntity();

            m_scene->GetRoot()->AddChild(zombie);

            if (zombie_entity.IsValid()) {
                if (auto *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(zombie_entity)) {
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.25f);
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
                }

                m_scene->GetEntityManager()->AddComponent(zombie_entity, AudioComponent {
                    AssetManager::GetInstance()->Load<AudioSource>("sounds/cartoon001.wav"),
                    {
                        .status = AUDIO_PLAYBACK_STATUS_PLAYING,
                        .loop_mode = AUDIO_LOOP_MODE_REPEAT,
                        .speed = 1.0f
                    }
                });

                // if (auto *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(zombie_entity)) {
                //     transform_component->transform.SetTranslation(Vector3(0, 1, 0));
                //     transform_component->transform.SetScale(Vector3(0.25f));
                // }
            }

            zombie.SetName("zombie");
        }
#endif

        if (results["cart"]) {
            auto cart = results["cart"].ExtractAs<Node>();
            cart.Scale(1.5f);
            cart.SetName("cart");

            //m_scene->GetRoot()->AddChild(cart);

            // ByteBuffer lightmap_bitmap_bytebuffer = lightmap_result.result.bitmap.ToByteBuffer();
            // UniquePtr<StreamedData> streamed_data(new MemoryStreamedData(std::move(lightmap_bitmap_bytebuffer)));
            // streamed_data->Load();

            // auto lightmap_texture = CreateObject<Texture>(
            //     Extent3D { lightmap_result.result.bitmap.GetWidth(), lightmap_result.result.bitmap.GetHeight(), 1 },
            //     InternalFormat::RGB8,
            //     ImageType::TEXTURE_TYPE_2D,
            //     FilterMode::TEXTURE_FILTER_LINEAR,
            //     WrapMode::TEXTURE_WRAP_REPEAT,
            //     std::move(streamed_data)
            // );
            // InitObject(lightmap_texture);

            // mesh_component.material->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_LIGHT_MAP, lightmap_texture);
        }
        
        if (results["test_model"]) {
            auto node = results["test_model"].ExtractAs<Node>();
            //node.Scale(3.0f);
            node.Scale(0.0125f);
            node.SetName("test_model");
            node.LockTransform();

            GetScene()->GetRoot()->AddChild(node);

            m_scene->GetEnvironment()->AddRenderComponent<ReflectionProbeRenderer>(
                HYP_NAME(ReflectionProbe0),
                node.GetWorldAABB()
            );

            for (auto &node : node.GetChildren()) {
                if (auto child_entity = node.GetEntity()) {
                    // Add BLASComponent

                    m_scene->GetEntityManager()->AddComponent(child_entity, BLASComponent { });
                }
            }

            if (true) {
                auto env_grid_entity = m_scene->GetEntityManager()->AddEntity();

                m_scene->GetEntityManager()->AddComponent(env_grid_entity, TransformComponent {
                    node.GetWorldTransform()
                });

                m_scene->GetEntityManager()->AddComponent(env_grid_entity, BoundingBoxComponent {
                    node.GetLocalAABB() * 1.0f,
                    node.GetWorldAABB() * 1.0f
                });

                // Add env grid component
                m_scene->GetEntityManager()->AddComponent(env_grid_entity, EnvGridComponent {
                    EnvGridType::ENV_GRID_TYPE_SH
                });

                auto env_grid_node = m_scene->GetRoot()->AddChild();
                env_grid_node.SetEntity(env_grid_entity);
                env_grid_node.SetName("EnvGrid");
            }
        }
    }

    // auto my_button_entity = GetUIStage()->GetScene()->GetEntityManager()->AddEntity();
    // GetUIStage()->GetScene()->GetEntityManager()->AddComponent<UIComponent>(my_button_entity, {
    //     .type = UI_COMPONENT_TYPE_BUTTON,
    //     .name = HYP_NAME(MyButton),
    //     .bounds = {
    //         .position = Vector2(0.0f, 0.0f),
    //         .size = Vector2(100.0f, 50.0f)
    //     }
    // });


    // auto ui_text = GetUIStage()->CreateUIObject<UIText>(HYP_NAME(Sample_Text2), Vec2i { 25, 25 }, Vec2i { 100, 30 });
    // ui_text->SetText("Hello, world!");
    // ui_text->SetDepth(1);
    // ui_text->UpdatePosition();
    // ui_text->UpdateSize();

    //RC<LightmapRenderer> lightmap_renderer = m_scene->GetEnvironment()->AddRenderComponent<LightmapRenderer>(HYP_NAME(LightmapRenderer0));
}

void SampleStreamer::Teardown()
{
    Game::Teardown();
}

void SampleStreamer::HandleCompletedAssetBatch(Name name, const RC<AssetBatch> &batch)
{
    // Should already be completed.
    AssetMap loaded_assets = batch->AwaitResults();

    if (name == HYP_NAME(GaussianSplatting)) {
        json::JSONValue cameras_json = loaded_assets["cameras json"].ExtractAs<json::JSONValue>();

        struct GaussianSplattingCameraDefinition
        {
            String  id;
            String  img_name;
            uint32  width;
            uint32  height;
            Vec3f   position;
            Matrix3 rotation;
            float   fx;
            float   fy;
        };

        Array<GaussianSplattingCameraDefinition> camera_definitions;

        if (cameras_json && cameras_json.IsArray()) {
            camera_definitions.Reserve(cameras_json.AsArray().Size());

            for (const json::JSONValue &item : cameras_json.AsArray()) {
                GaussianSplattingCameraDefinition definition;
                definition.id       = item["id"].ToString();
                definition.img_name = item["img_name"].ToString();
                definition.width    = MathUtil::Floor(item["width"].ToNumber());
                definition.height   = MathUtil::Floor(item["height"].ToNumber());
                definition.fx       = float(item["fx"].ToNumber());
                definition.fy       = float(item["fy"].ToNumber());

                if (item["position"].IsArray()) {
                    definition.position = Vector3(
                        float(item["position"][0].ToNumber()),
                        float(item["position"][1].ToNumber()),
                        float(item["position"][2].ToNumber())
                    );
                }

                if (item["rotation"].IsArray()) {
                    float v[9] = {
                        float(item["rotation"][0][0].ToNumber()),
                        float(item["rotation"][0][1].ToNumber()),
                        float(item["rotation"][0][2].ToNumber()),
                        float(item["rotation"][1][0].ToNumber()),
                        float(item["rotation"][1][1].ToNumber()),
                        float(item["rotation"][1][2].ToNumber()),
                        float(item["rotation"][2][0].ToNumber()),
                        float(item["rotation"][2][1].ToNumber()),
                        float(item["rotation"][2][2].ToNumber())
                    };

                    definition.rotation = Matrix3(v);
                }

                camera_definitions.PushBack(definition);
            }
        }

        Quaternion camera_offset_rotation = Quaternion::Identity();
        Vec3f up_direction = Vec3f::UnitY();

        Array<Vec3f> all_up_directions;
        all_up_directions.Reserve(camera_definitions.Size());

        for (const auto &camera_definition : camera_definitions) {
            const Vec3f camera_up = Matrix4(camera_definition.rotation) * Vec3f::UnitY();

            all_up_directions.PushBack(camera_up);
        }

        if (all_up_directions.Size() != 0) {
            up_direction = Vec3f::Zero();

            for (const Vec3f &camera_up_direction : all_up_directions) {
                up_direction += camera_up_direction;
            }

            up_direction /= float(all_up_directions.Size());
            up_direction.Normalize();

            const Vec3f axis = up_direction.Cross(Vec3f::UnitY()).Normalize();

            const float cos_theta = up_direction.Dot(Vec3f::UnitY());
            const float theta = MathUtil::Arccos(cos_theta);

            camera_offset_rotation = Quaternion(axis, theta).Invert();
        }

        DebugLog(LogType::Debug, "Up direction = %f, %f, %f\n", up_direction.x, up_direction.y, up_direction.z);

        PLYModelLoader::PLYModel ply_model = loaded_assets["ply model"].ExtractAs<PLYModelLoader::PLYModel>();

        const SizeType num_points = ply_model.vertices.Size();

        RC<GaussianSplattingModelData> gaussian_splatting_model = RC<GaussianSplattingModelData>::Construct();
        gaussian_splatting_model->points.Resize(num_points);
        gaussian_splatting_model->transform.SetRotation(Quaternion(Vector3(1, 0, 0), M_PI));

        const bool has_rotations = ply_model.custom_data.Contains("rot_0")
            && ply_model.custom_data.Contains("rot_1")
            && ply_model.custom_data.Contains("rot_2")
            && ply_model.custom_data.Contains("rot_3");

        const bool has_scales = ply_model.custom_data.Contains("scale_0")
            && ply_model.custom_data.Contains("scale_1")
            && ply_model.custom_data.Contains("scale_2");

        const bool has_sh = ply_model.custom_data.Contains("f_dc_0")
            && ply_model.custom_data.Contains("f_dc_1")
            && ply_model.custom_data.Contains("f_dc_2");

        const bool has_opacity = ply_model.custom_data.Contains("opacity");

        for (SizeType index = 0; index < num_points; index++) {
            auto &out_point = gaussian_splatting_model->points[index];

            out_point.position = Vector4(ply_model.vertices[index].GetPosition(), 1.0f);

            if (has_rotations) {
                Quaternion rotation;

                ply_model.custom_data["rot_0"].Read(index * sizeof(float), &rotation.w);
                ply_model.custom_data["rot_1"].Read(index * sizeof(float), &rotation.x);
                ply_model.custom_data["rot_2"].Read(index * sizeof(float), &rotation.y);
                ply_model.custom_data["rot_3"].Read(index * sizeof(float), &rotation.z);

                rotation.Normalize();

                out_point.rotation = rotation;
            }

            if (has_scales) {
                Vector3 scale = Vector3::One();

                ply_model.custom_data["scale_0"].Read(index * sizeof(float), &scale.x);
                ply_model.custom_data["scale_1"].Read(index * sizeof(float), &scale.y);
                ply_model.custom_data["scale_2"].Read(index * sizeof(float), &scale.z);

                out_point.scale = Vector4(scale, 1.0f);
            }

            if (has_sh) {
                float f_dc_0 = 0.0f;
                float f_dc_1 = 0.0f;
                float f_dc_2 = 0.0f;
                float opacity = 1.0f;

                static constexpr float SH_C0 = 0.28209479177387814f;

                ply_model.custom_data["f_dc_0"].Read(index * sizeof(float), &f_dc_0);
                ply_model.custom_data["f_dc_1"].Read(index * sizeof(float), &f_dc_1);
                ply_model.custom_data["f_dc_2"].Read(index * sizeof(float), &f_dc_2);

                if (has_opacity) {
                    ply_model.custom_data["opacity"].Read(index * sizeof(float), &opacity);
                }

                out_point.color = Vector4(
                    0.5f + (SH_C0 * f_dc_0),
                    0.5f + (SH_C0 * f_dc_1),
                    0.5f + (SH_C0 * f_dc_2),
                    1.0f / (1.0f + MathUtil::Exp(-opacity))
                );
            }
        }

        uint camera_definition_index = 0;

        RC<CameraTrack> camera_track = RC<CameraTrack>::Construct();
        camera_track->SetDuration(60.0);

        //m_scene->GetCamera()->SetCameraController(UniquePtr<CameraTrackController>::Construct(camera_track));

        for (const auto &camera_definition : camera_definitions) {
            camera_track->AddPivot({
                double(camera_definition_index) / double(camera_definitions.Size()),
                gaussian_splatting_model->transform * Transform(camera_definition.position, Vector3(1.0f), Quaternion(Matrix4(camera_definition.rotation).Orthonormalized()))
            });

            camera_definition_index++;

            break;
        }

        auto gaussian_splatting_instance = CreateObject<GaussianSplattingInstance>(std::move(gaussian_splatting_model));
        InitObject(gaussian_splatting_instance);

        m_scene->GetEnvironment()->GetGaussianSplatting()->SetGaussianSplattingInstance(std::move(gaussian_splatting_instance));
    }
}


void SampleStreamer::Logic(GameCounter::TickUnit delta)
{
    if (auto gun_node = m_scene->GetRoot().Select("gun")) {
        const Vec3f &camera_position = m_scene->GetCamera()->GetTranslation();
        const Vec3f &camera_direction = m_scene->GetCamera()->GetDirection();

        const Quaternion rotation = Quaternion::LookAt(camera_direction, Vector3::UnitY());

        Vec3f gun_offset = Vec3f(-0.18f, -0.3f, -0.1f);
        gun_node.SetLocalTranslation(camera_position + (camera_direction) + (Quaternion(rotation).Invert() * gun_offset));
        gun_node.SetLocalRotation(rotation);
    }

    if (auto terrain_node = m_scene->FindNodeByName("TerrainNode")) {
        if (auto terrain_entity = terrain_node.GetEntity()) {
            TerrainComponent *terrain_component = m_scene->GetEntityManager()->TryGetComponent<TerrainComponent>(terrain_entity);

            if (terrain_component) {
                terrain_component->camera_position = m_scene->GetCamera()->GetTranslation();
            }
        }
    }

    for (auto it = m_asset_batches.Begin(); it != m_asset_batches.End();) {
        if (it->second->IsCompleted()) {
            DebugLog(LogType::Debug, "Handle completed asset batch %s\n", it->first.LookupString());

            HandleCompletedAssetBatch(it->first, it->second);

            it = m_asset_batches.Erase(it);
        } else {
            ++it;
        }
    }

    auto env_grid_node = m_scene->FindNodeByName("EnvGrid");

    if (env_grid_node) {
        // env_grid_node.SetWorldTranslation(m_scene->GetCamera()->GetTranslation());
    }

    if (m_rtc_instance) {

        while (!m_message_queue.Empty()) {
            const json::JSONValue message = m_message_queue.Pop();

            const String message_type = message["type"].ToString();
            const String id = message["id"].ToString();

            if (message_type == "request") {
                RC<RTCClient> client = m_rtc_instance->GetServer()->CreateClient(id);
                DebugLog(LogType::Debug, "Adding client with ID %s\n", id.Data());

                auto track = m_rtc_instance->CreateTrack(RTCTrackType::RTC_TRACK_TYPE_VIDEO);

                client->GetCallbacks().OnMessage.Bind([client_weak = Weak<RTCClient>(client)](RTCClientCallbackData data)
                {
                    if (!data.bytes.HasValue()) {
                        return;
                    }

                    json::ParseResult json_parse_result = json::JSON::Parse(String(data.bytes->ToByteView()));

                    if (!json_parse_result.ok) {
                        DebugLog(LogType::Warn, "Failed to parse message as JSON\n");

                        return;
                    }

                    if (!json_parse_result.value.IsObject()) {
                        DebugLog(LogType::Warn, "Invalid JSON message: Expected an object\n");

                        return;
                    }

                    json::JSONObject &message = json_parse_result.value.AsObject();

                    if (!message["type"].IsString()) {
                        DebugLog(LogType::Warn, "Invalid JSON message: message[\"type\"] should be a String\n");

                        return;
                    }

                    if (message["type"].AsString() == "Pong") {
                        if (auto client = client_weak.Lock()) {
                            Optional<RC<RTCDataChannel>> data_channel = client->GetDataChannel(HYP_NAME(ping-pong));

                            if (data_channel.HasValue()) {
                                data_channel.Get()->Send("Ping");
                            }
                        }
                    }
                });

                client->CreateDataChannel(HYP_NAME(ping-pong));

                client->AddTrack(track);
                client->Connect();
            } else if (message_type == "answer") {
                if (const Optional<RC<RTCClient>> client = m_rtc_instance->GetServer()->GetClientList().Get(id)) {
                    client.Get()->SetRemoteDescription("answer", message["sdp"].ToString());
                } else {
                    DebugLog(LogType::Warn, "Client with ID %s not found\n", id.Data());
                }
            }
        }

        // Just a test -- will optimize by doing this on another thread.
        {
            Array<RC<RTCTrack>> tracks;

            for (const auto &client : m_rtc_instance->GetServer()->GetClientList()) {
                if (client.second->GetState() != RTC_CLIENT_STATE_CONNECTED) {
                    continue;
                }

                for (const auto &track : client.second->GetTracks()) {
                    if (!track->IsOpen()) {
                        continue;
                    }

                    tracks.PushBack(track);
                }
            }

            RTCStreamDestination dest;
            dest.tracks = std::move(tracks);

            m_rtc_stream->SendSample(dest);
        }
    }

    HandleCameraMovement(delta);
}

void SampleStreamer::OnInputEvent(const SystemEvent &event)
{
    Game::OnInputEvent(event);

    const Extent2D window_size = GetInputManager()->GetWindow()->GetDimensions();

    if (event.GetType() == SystemEventType::EVENT_KEYDOWN) {
        if (event.GetKeyCode() == KeyCode::ARROW_LEFT) {
            auto sun_node = m_scene->GetRoot().Select("Sun");

            if (sun_node) {
                sun_node.SetWorldTranslation((sun_node.GetWorldTranslation() + Vec3f(-0.1f, 0.0f, 0.0f)).Normalized());
            }
        } else if (event.GetKeyCode() == KeyCode::ARROW_RIGHT) {
            auto sun_node = m_scene->GetRoot().Select("Sun");

            if (sun_node) {
                sun_node.SetWorldTranslation((sun_node.GetWorldTranslation() + Vec3f(0.1f, 0.0f, 0.0f)).Normalized());
            }
        } else if (event.GetKeyCode() == KeyCode::ARROW_UP) {
            auto sun_node = m_scene->GetRoot().Select("Sun");

            if (sun_node) {
                sun_node.SetWorldTranslation((sun_node.GetWorldTranslation() + Vec3f(0.0f, 0.1f, 0.0f)).Normalized());
            }
        } else if (event.GetKeyCode() == KeyCode::ARROW_DOWN) {
            auto sun_node = m_scene->GetRoot().Select("Sun");

            if (sun_node) {
                sun_node.SetWorldTranslation((sun_node.GetWorldTranslation() + Vec3f(0.0f, -0.1f, 0.0f)).Normalized());
            }
        }
    }

    if (event.GetType() == SystemEventType::EVENT_MOUSEBUTTON_UP) {
#if 0
        struct RENDER_COMMAND(SubmitLightmapJob) : renderer::RenderCommand
        {
            Handle<Scene> scene;

            RENDER_COMMAND(SubmitLightmapJob)(Handle<Scene> scene)
                : scene(std::move(scene))
            {
            }

            virtual ~RENDER_COMMAND(SubmitLightmapJob)() override = default;

            virtual Result operator()() override
            {
                if (scene->GetEnvironment()->HasRenderComponent<LightmapRenderer>()) {
                    scene->GetEnvironment()->RemoveRenderComponent<LightmapRenderer>();
                } else {
                    scene->GetEnvironment()->AddRenderComponent<LightmapRenderer>(Name::Unique("LightmapRenderer"));
                }

                HYPERION_RETURN_OK;
            }
        };

        // PUSH_RENDER_COMMAND(SubmitLightmapJob, m_scene);
        m_scene->GetEnvironment()->AddRenderComponent<LightmapRenderer>(Name::Unique("LightmapRenderer"));
#endif

        // shoot bullet on mouse button left
        if (event.GetMouseButtons() == MouseButtonState::LEFT) {
#if 0
            const Vec3f &camera_position = m_scene->GetCamera()->GetTranslation();
            const Vec3f &camera_direction = m_scene->GetCamera()->GetDirection();

            const Quaternion rotation = Quaternion::LookAt(camera_direction, Vector3::UnitY());

            Vec3f gun_position = camera_position + (camera_direction * 0.5f);

            if (auto gun_node = m_scene->GetRoot().Select("gun")) {
                gun_position = gun_node[0].GetWorldTranslation();
            }

            Vec3f bullet_position = gun_position;

            auto bullet = m_scene->GetRoot()->AddChild();
            bullet.SetName("Bullet");
            bullet.SetLocalTranslation(bullet_position);
            bullet.SetLocalRotation(rotation);
            bullet.SetLocalScale(Vec3f(0.06f));

            auto bullet_entity = m_scene->GetEntityManager()->AddEntity();
            bullet.SetEntity(bullet_entity);

            m_scene->GetEntityManager()->AddComponent(bullet_entity, RigidBodyComponent {
                CreateObject<physics::RigidBody>(
                    RC<physics::PhysicsShape>(new physics::SpherePhysicsShape(BoundingSphere { Vec3f(0.0f), 0.1f })),
                    physics::PhysicsMaterial {
                        .mass = 0.01f
                    }
                )
            });

            RigidBodyComponent &rigid_body_component = m_scene->GetEntityManager()->GetComponent<RigidBodyComponent>(bullet_entity);
            rigid_body_component.rigid_body->ApplyForce(camera_direction * 10.0f);

            m_scene->GetEntityManager()->AddComponent(bullet_entity, MeshComponent {
                MeshBuilder::NormalizedCubeSphere(4),
                MaterialCache::GetInstance()->GetOrCreate({
                    .shader_definition = ShaderDefinition {
                        HYP_NAME(Forward),
                        ShaderProperties(static_mesh_vertex_attributes)
                    },
                    .bucket = Bucket::BUCKET_OPAQUE
                })
            });
            m_scene->GetEntityManager()->AddComponent(bullet_entity, BoundingBoxComponent {
                BoundingBox(Vec3f(-0.1f), Vec3f(0.1f))
            });

            m_scene->GetEntityManager()->AddComponent(bullet_entity, VisibilityStateComponent { });
#endif
        }
    }

#if 0
    if (event.GetType() == SystemEventType::EVENT_MOUSEBUTTON_UP) {
        if (event.GetMouseButtons() == MouseButtonState::LEFT) {
            const auto &mouse_position = GetInputManager()->GetMousePosition();

            const int mouse_x = mouse_position.GetX();
            const int mouse_y = mouse_position.GetY();

            const auto mouse_world = m_scene->GetCamera()->TransformScreenToWorld(
                Vec2f(
                    mouse_x / float(GetInputManager()->GetWindow()->GetDimensions().width),
                    mouse_y / float(GetInputManager()->GetWindow()->GetDimensions().height)
                )
            );

            auto ray_direction = mouse_world.Normalized();

            // std::cout << "ray direction: " << ray_direction << "\n";

            Ray ray { m_scene->GetCamera()->GetTranslation(), ray_direction.GetXYZ() };
            RayTestResults results;

            if (m_scene->GetOctree().TestRay(ray, results)) {
                // std::cout << "hit with aabb : " << results.Front().hitpoint << "\n";
                RayTestResults triangle_mesh_results;

                for (const auto &hit : results) {
                    // now ray test each result as triangle mesh to find exact hit point
                    if (auto entity_id = ID<Entity>(hit.id)) {
                        MeshComponent *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity_id);
                        TransformComponent *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(entity_id);

                        if (mesh_component != nullptr && mesh_component->mesh.IsValid() && transform_component != nullptr) {
                            auto streamed_mesh_data = mesh_component->mesh->GetStreamedMeshData();
                            auto ref = streamed_mesh_data->AcquireRef();

                            ray.TestTriangleList(
                                ref->GetMeshData().vertices,
                                ref->GetMeshData().indices,
                                transform_component->transform,
                                entity_id.Value(),
                                triangle_mesh_results
                            );
                        }
                    }
                }

                if (!triangle_mesh_results.Empty()) {
                    const auto &mesh_hit = triangle_mesh_results.Front();

                    DebugLog(
                        LogType::Info,
                        "Hit mesh %u at %f, %f, %f\n",
                        mesh_hit.id,
                        mesh_hit.hitpoint.x,
                        mesh_hit.hitpoint.y,
                        mesh_hit.hitpoint.z
                    );

                    if (auto target = m_scene->GetRoot().Select("TestCube")) {
                        DebugLog(
                            LogType::Info,
                            "Setting target %s position to %f, %f, %f\n",
                            target.GetName().Data(),
                            mesh_hit.hitpoint.x,
                            mesh_hit.hitpoint.y,
                            mesh_hit.hitpoint.z
                        );
                        target.SetLocalTranslation(mesh_hit.hitpoint);
                        target.SetLocalRotation(Quaternion::LookAt((m_scene->GetCamera()->GetTranslation() - mesh_hit.hitpoint).Normalized(), Vector3::UnitY()));
                    }
                }
            }
        }
    }
#endif
}

Optional<Vec3f> SampleStreamer::GetWorldRay(const Vec2f &screen_position) const
{
    const auto mouse_world = m_scene->GetCamera()->TransformScreenToWorld(screen_position);

    auto ray_direction = mouse_world.Normalized();

    // std::cout << "ray direction: " << ray_direction << "\n";

    Ray ray { m_scene->GetCamera()->GetTranslation(), ray_direction.GetXYZ() };
    RayTestResults results;

    if (m_scene->GetOctree().TestRay(ray, results)) {
        // std::cout << "hit with aabb : " << results.Front().hitpoint << "\n";
        RayTestResults triangle_mesh_results;

        for (const auto &hit : results) {
            // now ray test each result as triangle mesh to find exact hit point
            if (auto entity_id = ID<Entity>(hit.id)) {
                MeshComponent *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity_id);
                TransformComponent *transform_component = m_scene->GetEntityManager()->TryGetComponent<TransformComponent>(entity_id);

                if (mesh_component != nullptr && mesh_component->mesh.IsValid() && transform_component != nullptr) {
                    // @TODO: generate a BVH instead of using raw mesh data
                    auto streamed_mesh_data = mesh_component->mesh->GetStreamedMeshData();

                    if (!streamed_mesh_data) {
                        continue;
                    }

                    auto ref = streamed_mesh_data->AcquireRef();

                    ray.TestTriangleList(
                        ref->GetMeshData().vertices,
                        ref->GetMeshData().indices,
                        transform_component->transform,
                        entity_id.Value(),
                        triangle_mesh_results
                    );
                }
            }
        }

        if (!triangle_mesh_results.Empty()) {
            const auto &mesh_hit = triangle_mesh_results.Front();

            return mesh_hit.hitpoint;
        }
    }

    return { };
}

void SampleStreamer::OnFrameEnd(Frame *frame)
{
    if (!m_scene || !m_scene->IsReady()) {
        return;
    }

    if (m_rtc_stream) {
        const ScreenCaptureRenderComponent *screen_capture = m_scene->GetEnvironment()->GetRenderComponent<ScreenCaptureRenderComponent>(HYP_NAME(StreamingCapture));

        if (screen_capture) {
            const GPUBufferRef &gpu_buffer_ref = screen_capture->GetBuffer();

            if (gpu_buffer_ref.IsValid()) {
                if (m_screen_buffer.Size() != gpu_buffer_ref->Size()) {
                    m_screen_buffer.SetSize(gpu_buffer_ref->Size());
                }

                gpu_buffer_ref->Read(Engine::GetInstance()->GetGPUDevice(), m_screen_buffer.Size(), m_screen_buffer.Data());
            }

            m_rtc_stream->GetEncoder()->PushData(std::move(m_screen_buffer));
        }
    }
}

// not overloading; just creating a method to handle camera movement
void SampleStreamer::HandleCameraMovement(GameCounter::TickUnit delta)
{
    if (m_input_manager->IsKeyDown(KeyCode::KEY_W)) {
        GetScene()->GetCamera()->GetCameraController()->PushCommand({
            CameraCommand::CAMERA_COMMAND_MOVEMENT,
            { .movement_data = { CameraCommand::CAMERA_MOVEMENT_FORWARD } }
        });
    }

    if (m_input_manager->IsKeyDown(KeyCode::KEY_S)) {
        GetScene()->GetCamera()->GetCameraController()->PushCommand({
            CameraCommand::CAMERA_COMMAND_MOVEMENT,
            { .movement_data = { CameraCommand::CAMERA_MOVEMENT_BACKWARD } }
        });
    }

    if (m_input_manager->IsKeyDown(KeyCode::KEY_A)) {
        GetScene()->GetCamera()->GetCameraController()->PushCommand({
            CameraCommand::CAMERA_COMMAND_MOVEMENT,
            { .movement_data = { CameraCommand::CAMERA_MOVEMENT_LEFT } }
        });
    }

    if (m_input_manager->IsKeyDown(KeyCode::KEY_D)) {
        GetScene()->GetCamera()->GetCameraController()->PushCommand({
            CameraCommand::CAMERA_COMMAND_MOVEMENT,
            { .movement_data = { CameraCommand::CAMERA_MOVEMENT_RIGHT } }
        });
    }

    // if (auto character = GetScene()->GetRoot().Select("zombie")) {
    //     character.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetDirection(), GetScene()->GetCamera()->GetUpVector()));
    // }
}
