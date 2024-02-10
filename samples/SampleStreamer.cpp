
#include "SampleStreamer.hpp"

#include "system/Application.hpp"
#include "system/Debug.hpp"

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <Engine.hpp>
#include <scene/controllers/FollowCameraController.hpp>
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
#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/PointLightShadowRenderer.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/AtomicVar.hpp>

#include <util/ArgParse.hpp>
#include <util/json/JSON.hpp>

#include <core/net/Socket.hpp>

#include <Game.hpp>

#include <ui/UIText.hpp>

#include <asset/serialization/fbom/FBOM.hpp>

#include <rendering/render_components/ScreenCapture.hpp>

#include <scene/camera/FirstPersonCamera.hpp>
#include <scene/camera/CameraTrackController.hpp>

#include <util/MeshBuilder.hpp>

#include <asset/model_loaders/PLYModelLoader.hpp>

#include "util/Profile.hpp"

/* Standard library */

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



SampleStreamer::SampleStreamer(RC<Application> application)
    : Game(application, ManagedGameInfo {
          "csharp/bin/Debug/net8.0/csharp.dll",
          "TestGame"
      })
{
}

void SampleStreamer::InitGame()
{
    Game::InitGame();


    ArgParse args;
    args.Add("SignallingServerIP", "s", ArgParse::ARG_FLAGS_REQUIRED, ArgParse::ArgumentType::ARGUMENT_TYPE_STRING);
    args.Add("SignallingServerPort", "p", ArgParse::ARG_FLAGS_REQUIRED, ArgParse::ArgumentType::ARGUMENT_TYPE_INT);

    auto arg_parse_result = args.Parse(GetApplication()->GetArguments());
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

    const Extent2D window_size = GetInputManager()->GetWindow()->GetExtent();

    m_texture = CreateObject<Texture>(Texture2D(
        window_size,
        InternalFormat::RGBA8,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));
    InitObject(m_texture);

    m_scene->SetCamera(CreateObject<Camera>(
        70.0f,
        window_size.width, window_size.height,
        0.01f, 30000.0f
    ));

    { // compare Proc<> to std::function
        struct TestObject
        {
            int i = 0;
            float f = 0.0f;
            String str = "hello world";
        };

        std::chrono::high_resolution_clock::time_point start, end;

        start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 1000000; i++) {
            TestObject test_object {
                .i = i,
                .f = float(i),
                .str = String::ToString(i)
            };

            constexpr auto x = sizeof(test_object);

            Proc<void, int> proc;
            proc = [t = test_object](int i) mutable
            {
                t.i += i;
                t.f += float(i);
                t.str = String::ToString(i);
            };

            proc(i);
        }

        end = std::chrono::high_resolution_clock::now();

        DebugLog(LogType::Debug, "Proc<>: %f\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f);


        start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 1000000; i++) {
            TestObject test_object {
                .i = i,
                .f = float(i),
                .str = String::ToString(i)
            };

            std::function<void(int)> func;
            func = [t = test_object](int i) mutable
            {
                t.i += i;
                t.f += float(i);
                t.str = String::ToString(i);
            };

            func(i);
        }

        end = std::chrono::high_resolution_clock::now();

        DebugLog(LogType::Debug, "std::function: %f\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f);
    }

    /*m_scene->GetCamera()->SetCameraController(UniquePtr<FollowCameraController>::Construct(
        Vector3(0.0f, 7.0f, 0.0f), Vector3(0.0f, 0.0f, 5.0f)
    ));*/
    m_scene->GetCamera()->SetCameraController(RC<CameraController>(new FirstPersonCameraController()));

    {
        auto gun = g_asset_manager->Load<Node>("models/gun/AK47NoSubdiv.obj");

        if (gun) {
            auto gun_parent = m_scene->GetRoot().AddChild();
            gun_parent.SetName("gun");

            gun.SetLocalScale(0.25f);
            gun.SetLocalRotation(Quaternion(Vec3f(0.0f, 1.0f, 0.0f), M_PI));
            gun_parent.AddChild(gun);
        }
    }

    if (true) {
        auto cube_node = m_scene->GetRoot().AddChild();
        cube_node.SetName("TestCube");

        auto entity_id = m_scene->GetEntityManager()->AddEntity();
        cube_node.SetEntity(entity_id);

        auto cube = MeshBuilder::Cube();
        InitObject(cube);

        // add physics
        m_scene->GetEntityManager()->AddComponent(entity_id, RigidBodyComponent {
            CreateObject<physics::RigidBody>(
                RC<physics::PhysicsShape>(new physics::BoxPhysicsShape(BoundingBox { Vec3f { -1.0f }, Vec3f { 1.0f } })),
                physics::PhysicsMaterial {
                    .mass = 0.1f // static
                }
            )
        });
        
        m_scene->GetEntityManager()->AddComponent(entity_id, MeshComponent {
            cube,
            g_material_system->GetOrCreate(
                {
                    .shader_definition = ShaderDefinition {
                        HYP_NAME(Forward),
                        ShaderProperties(renderer::static_mesh_vertex_attributes)
                    },
                    .bucket = Bucket::BUCKET_OPAQUE
                },
                {
                    { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f, 0.0f, 0.0f, 1.0f) },
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

    {

        auto cube_node = m_scene->GetRoot().AddChild();
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
            g_material_system->GetOrCreate(
                {
                    .shader_definition = ShaderDefinition {
                        HYP_NAME(Forward),
                        ShaderProperties(renderer::static_mesh_vertex_attributes)
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

    // // Add a reflection probe
    // TEMP: Commented out due to blending issues with multiple reflection probes
    // m_scene->GetEnvironment()->AddRenderComponent<ReflectionProbeRenderer>(
    //     HYP_NAME(ReflectionProbe0),
    //     Vec3f(0.0f)
    // );

    // m_scene->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(HYP_NAME(StreamingCapture), window_size);

    if (false) {
        auto terrain_node = m_scene->GetRoot().AddChild();
        auto terrain_entity = m_scene->GetEntityManager()->AddEntity();

        // MeshComponent
        m_scene->GetEntityManager()->AddComponent(terrain_entity, MeshComponent {
            Handle<Mesh> { },
            g_material_system->GetOrCreate({
                .shader_definition = ShaderDefinition {
                    HYP_NAME(Terrain),
                    ShaderProperties(renderer::static_mesh_vertex_attributes)
                },
                .bucket = Bucket::BUCKET_OPAQUE
            })
        });

        // TerrainComponent
        m_scene->GetEntityManager()->AddComponent(terrain_entity, TerrainComponent {
        });

        // // TransformComponent
        // m_scene->GetEntityManager()->AddComponent(terrain_entity, TransformComponent {
        //     Transform(
        //         Vec3f::zero,
        //         Vec3f::one,
        //         Quaternion::Identity()
        //     )
        // });

        terrain_node.SetEntity(terrain_entity);
        terrain_node.SetName("TerrainNode");
    }

    {
        auto sun = CreateObject<Light>(DirectionalLight(
            Vec3f(-0.1f, 0.65f, 0.1f).Normalize(),
            Color(1.0f, 0.7f, 0.4f),
            5.0f
        ));

        InitObject(sun);

        auto sun_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent(sun_entity, TransformComponent {
            Transform(
                Vec3f(-0.1f, 0.65f, 0.1f).Normalized(),
                Vec3f::one,
                Quaternion::Identity()
            )
        });

        m_scene->GetEntityManager()->AddComponent(sun_entity, LightComponent {
            sun
        });

        m_scene->GetEntityManager()->AddComponent(sun_entity, ShadowMapComponent {
            .radius = 12.0f,
            .resolution = { 2048, 2048 }
        });

        Array<Handle<Light>> point_lights;

        point_lights.PushBack(CreateObject<Light>(PointLight(
            Vector3(0.0f, 6.0f, 0.0f),
            Color(1.0f, 0.9f, 0.7f),
            40.0f,
            200.35f
        )));
        // point_lights.PushBack(CreateObject<Light>(PointLight(
        //     Vector3(0.0f, 10.0f, 12.0f),
        //     Color(1.0f, 0.0f, 0.0f),
        //     15.0f,
        //     200.0f
        // )));

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
    }
    

    // Add Skybox
    {
        auto skybox_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent(skybox_entity, TransformComponent {
            Transform(
                Vec3f::zero,
                Vec3f(10.0f),
                Quaternion::Identity()
            )
        });

        // m_scene->GetEntityManager()->AddComponent(skybox_entity, MeshComponent {
        //     MeshBuilder::Cube(),
        //     g_material_system->GetOrCreate({
        //         .shader_definition = ShaderDefinition {
        //             HYP_NAME(Skybox),
        //             ShaderProperties(renderer::static_mesh_vertex_attributes)
        //         },
        //         .bucket = Bucket::BUCKET_SKYBOX,
        //         .cull_faces = FaceCullMode::FRONT
        //     })
        // });

        m_scene->GetEntityManager()->AddComponent(skybox_entity, SkyComponent { });
        m_scene->GetEntityManager()->AddComponent(skybox_entity, MeshComponent { });
        m_scene->GetEntityManager()->AddComponent(skybox_entity, VisibilityStateComponent {
            VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
        });
        m_scene->GetEntityManager()->AddComponent(skybox_entity, BoundingBoxComponent {
            BoundingBox(Vec3f(-100.0f), Vec3f(100.0f))
        });
    }

    // add sample model
    {
        auto batch = g_asset_manager->CreateBatch();
        batch->Add("test_model", "models/sponza/sponza.obj");//living_room/living_room.obj");
        batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
        batch->LoadAsync();
        auto results = batch->AwaitResults();

        if (false) {
            auto plane_node = m_scene->GetRoot().AddChild();
            plane_node.Rotate(Quaternion(Vec3f(1.0f, 0.0f, 0.0f), -M_PI_2));
            plane_node.Scale(10.0f);
            plane_node.Translate(Vec3f(0.0f, 1.0f, 0.0f));

            auto plane_entity = m_scene->GetEntityManager()->AddEntity();
            plane_node.SetEntity(plane_entity);

            auto mesh = MeshBuilder::Quad();
            InitObject(mesh);

            m_scene->GetEntityManager()->AddComponent(plane_entity, MeshComponent {
                mesh,
                g_material_system->GetOrCreate(
                    {
                        .shader_definition = ShaderDefinition {
                            HYP_NAME(Forward),
                            ShaderProperties(renderer::static_mesh_vertex_attributes)
                        },
                        .bucket = Bucket::BUCKET_OPAQUE
                    },
                    {
                        { Material::MATERIAL_KEY_ALBEDO, Vec4f(1.0f, 1.0f, 1.0f, 1.0f) },
                        { Material::MATERIAL_KEY_METALNESS, 0.0f },
                        { Material::MATERIAL_KEY_ROUGHNESS, 0.0f }
                    }
                )
            });

            m_scene->GetEntityManager()->AddComponent(plane_entity, BoundingBoxComponent {
                mesh->GetAABB()
            });

            m_scene->GetEntityManager()->AddComponent(plane_entity, VisibilityStateComponent { });

            m_scene->GetEntityManager()->AddComponent(plane_entity, RigidBodyComponent {
                CreateObject<physics::RigidBody>(
                    RC<physics::PhysicsShape>(new physics::BoxPhysicsShape(
                        BoundingBox { Vec3f(-10.0f, -0.1f, -10.0f), Vec3f(10.0f, 0.1f, 10.0f) }
                    )),
                    physics::PhysicsMaterial {
                        .mass = 0.0f // static
                    }
                )
            });
        }

        if (auto zombie = results["zombie"].Get<Node>()) {
            zombie.Scale(0.5f);
            auto zombie_entity = zombie[0].GetEntity();

            m_scene->GetRoot().AddChild(zombie);

            if (zombie_entity.IsValid()) {
                // if (auto *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(zombie_entity)) {
                //     mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                //     mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.25f);
                //     mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
                // }

                m_scene->GetEntityManager()->AddComponent(zombie_entity, AudioComponent {
                    g_asset_manager->Load<AudioSource>("sounds/cartoon001.wav"),
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

        if (results["test_model"]) {
            auto node = results["test_model"].ExtractAs<Node>();
            // node.Scale(3.0f);
            node.Scale(0.0125f);
            node.SetName("test_model");
            
            GetScene()->GetRoot().AddChild(node);

            auto env_grid_entity = m_scene->GetEntityManager()->AddEntity();

            m_scene->GetEntityManager()->AddComponent(env_grid_entity, TransformComponent {
                node.GetWorldTransform()
            });

            m_scene->GetEntityManager()->AddComponent(env_grid_entity, BoundingBoxComponent {
                node.GetLocalAABB(),
                node.GetWorldAABB()
            });

            // Add env grid component
            m_scene->GetEntityManager()->AddComponent(env_grid_entity, EnvGridComponent {
                EnvGridType::ENV_GRID_TYPE_SH
            });
        }
    }

    m_scene->GetEnvironment()->AddRenderComponent<UIRenderer>(HYP_NAME(UIRenderer0), GetUI().GetScene());
}

void SampleStreamer::InitRender()
{
    Game::InitRender();
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
        auto cameras_json = loaded_assets["cameras json"].Get<json::JSONValue>();
        AssertThrow(loaded_assets["cameras json"].result.status == LoaderResult::Status::OK);

        struct GaussianSplattingCameraDefinition
        {
            String  id;
            String  img_name;
            uint32  width;
            uint32  height;
            Vector3 position;
            Matrix3 rotation;
            float   fx;
            float   fy;
        };

        Array<GaussianSplattingCameraDefinition> camera_definitions;

        if (cameras_json && cameras_json->IsArray()) {
            camera_definitions.Reserve(cameras_json->AsArray().Size());

            for (const json::JSONValue &item : cameras_json->AsArray()) {
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
        Vector3 up_direction = Vector3::UnitY();

        Array<Vector3> all_up_directions;
        all_up_directions.Reserve(camera_definitions.Size());

        for (const auto &camera_definition : camera_definitions) {
            const Vector3 camera_up = Matrix4(camera_definition.rotation) * Vector3::UnitY();

            all_up_directions.PushBack(camera_up);
        }

        if (all_up_directions.Size() != 0) {
            up_direction = Vector3::Zero();

            for (const Vector3 &camera_up_direction : all_up_directions) {
                up_direction += camera_up_direction;
            }

            up_direction /= float(all_up_directions.Size());
            up_direction.Normalize();

            const Vector3 axis = up_direction.Cross(Vector3::UnitY()).Normalize();

            const float cos_theta = up_direction.Dot(Vector3::UnitY());
            const float theta = MathUtil::Arccos(cos_theta);

            camera_offset_rotation = Quaternion(axis, theta).Invert();
        }

        DebugLog(LogType::Debug, "Up direction = %f, %f, %f\n", up_direction.x, up_direction.y, up_direction.z);

        //const auto camera_rotation = camera_definitions.Any() ? Quaternion(Matrix4(camera_definitions[0].rotation)).Invert() : Quaternion::identity;

        auto ply_model = loaded_assets["ply model"].Get<PLYModelLoader::PLYModel>();

        const SizeType num_points = ply_model->vertices.Size();

        RC<GaussianSplattingModelData> gaussian_splatting_model = RC<GaussianSplattingModelData>::Construct();
        gaussian_splatting_model->points.Resize(num_points);
        gaussian_splatting_model->transform.SetRotation(Quaternion(Vector3(1, 0, 0), M_PI));

        const bool has_rotations = ply_model->custom_data.Contains("rot_0")
            && ply_model->custom_data.Contains("rot_1")
            && ply_model->custom_data.Contains("rot_2")
            && ply_model->custom_data.Contains("rot_3");

        const bool has_scales = ply_model->custom_data.Contains("scale_0")
            && ply_model->custom_data.Contains("scale_1")
            && ply_model->custom_data.Contains("scale_2");

        const bool has_sh = ply_model->custom_data.Contains("f_dc_0")
            && ply_model->custom_data.Contains("f_dc_1")
            && ply_model->custom_data.Contains("f_dc_2");

        const bool has_opacity = ply_model->custom_data.Contains("opacity");

        for (SizeType index = 0; index < num_points; index++) {
            auto &out_point = gaussian_splatting_model->points[index];

            out_point.position = Vector4(ply_model->vertices[index].GetPosition(), 1.0f);

            if (has_rotations) {
                Quaternion rotation;

                ply_model->custom_data["rot_0"].Read(index * sizeof(float), &rotation.w);
                ply_model->custom_data["rot_1"].Read(index * sizeof(float), &rotation.x);
                ply_model->custom_data["rot_2"].Read(index * sizeof(float), &rotation.y);
                ply_model->custom_data["rot_3"].Read(index * sizeof(float), &rotation.z);
                
                rotation.Normalize();

                out_point.rotation = rotation;
            }

            if (has_scales) {
                Vector3 scale = Vector3::one;

                ply_model->custom_data["scale_0"].Read(index * sizeof(float), &scale.x);
                ply_model->custom_data["scale_1"].Read(index * sizeof(float), &scale.y);
                ply_model->custom_data["scale_2"].Read(index * sizeof(float), &scale.z);

                out_point.scale = Vector4(scale, 1.0f);
            }

            if (has_sh) {
                float f_dc_0 = 0.0f;
                float f_dc_1 = 0.0f;
                float f_dc_2 = 0.0f;
                float opacity = 1.0f;

                static constexpr float SH_C0 = 0.28209479177387814f;

                ply_model->custom_data["f_dc_0"].Read(index * sizeof(float), &f_dc_0);
                ply_model->custom_data["f_dc_1"].Read(index * sizeof(float), &f_dc_1);
                ply_model->custom_data["f_dc_2"].Read(index * sizeof(float), &f_dc_2);

                if (has_opacity) {
                    ply_model->custom_data["opacity"].Read(index * sizeof(float), &opacity);
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

        Vec3f gun_offset = Vec3f(-0.18f, -0.2f, -0.3f);
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

    auto env_grid_node = m_scene->FindNodeByName("EnvGridEntity");

    if (env_grid_node) {
        // env_grid_entity->SetTranslation(m_scene->GetCamera()->GetTranslation());
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

                client->GetCallbacks().On(RTCClientCallbackMessages::MESSAGE, [client_weak = Weak<RTCClient>(client)](RTCClientCallbackData data)
                {
                    if (!data.bytes.HasValue()) {
                        return;
                    }
                    
                    json::ParseResult json_parse_result = json::JSON::Parse(String(data.bytes.Get()));

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

    m_ui.Update(delta);

    HandleCameraMovement(delta);
}

void SampleStreamer::OnInputEvent(const SystemEvent &event)
{
    Game::OnInputEvent(event);

    const Extent2D window_size = GetInputManager()->GetWindow()->GetExtent();

    if (event.GetType() == SystemEventType::EVENT_MOUSEBUTTON_UP) {
        // shoot bullet on mouse button left
        if (event.GetMouseButton() == MOUSE_BUTTON_LEFT) {
#if 0
            const auto &mouse_position = GetInputManager()->GetMousePosition();

            const int mouse_x = mouse_position.GetX();
            const int mouse_y = mouse_position.GetY();

            Optional<Vec3f> world_ray = GetWorldRay({
                float(mouse_x) / float(GetInputManager()->GetWindow()->GetExtent().width),
                float(mouse_y) / float(GetInputManager()->GetWindow()->GetExtent().height)
            });

            if (world_ray.HasValue()) {
                // shot at this point in the world, create test entity at this point

                auto bullet_hole = m_scene->GetRoot().AddChild();
                bullet_hole.SetName("BulletHole");

                auto entity_id = m_scene->GetEntityManager()->AddEntity();
                bullet_hole.SetEntity(entity_id);

                auto bullet_hole_mesh = MeshBuilder::Cube();
                InitObject(bullet_hole_mesh);

                bullet_hole.Scale(0.2f);
                bullet_hole.SetWorldTranslation(world_ray.Get());

                m_scene->GetEntityManager()->AddComponent(entity_id, MeshComponent {
                    bullet_hole_mesh,
                    g_material_system->GetOrCreate(
                        {
                            .shader_definition = ShaderDefinition {
                                HYP_NAME(Forward),
                                ShaderProperties(renderer::static_mesh_vertex_attributes)
                            },
                            .bucket = Bucket::BUCKET_OPAQUE
                        },
                        {
                            { Material::MATERIAL_KEY_ALBEDO, Vec4f(0.0f, 1.0f, 0.0f, 1.0f) },
                            { Material::MATERIAL_KEY_METALNESS, 0.0f },
                            { Material::MATERIAL_KEY_ROUGHNESS, 0.01f }
                        }
                    )
                });

                m_scene->GetEntityManager()->AddComponent(entity_id, BoundingBoxComponent {
                    bullet_hole_mesh->GetAABB()
                });

                m_scene->GetEntityManager()->AddComponent(entity_id, VisibilityStateComponent { });
            }
#else
            const Vec3f &camera_position = m_scene->GetCamera()->GetTranslation();
            const Vec3f &camera_direction = m_scene->GetCamera()->GetDirection();

            const Quaternion rotation = Quaternion::LookAt(camera_direction, Vector3::UnitY());

            Vec3f gun_position = camera_position + (camera_direction * 0.5f);

            if (auto gun_node = m_scene->GetRoot().Select("gun")) {
                gun_position = gun_node[0].GetWorldTranslation();
            }

            Vec3f bullet_position = gun_position;

            auto bullet = m_scene->GetRoot().AddChild();
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
                g_material_system->GetOrCreate({
                    .shader_definition = ShaderDefinition {
                        HYP_NAME(Forward),
                        ShaderProperties(renderer::static_mesh_vertex_attributes)
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
        if (event.GetMouseButton() == MOUSE_BUTTON_LEFT) {
            const auto &mouse_position = GetInputManager()->GetMousePosition();

            const int mouse_x = mouse_position.GetX();
            const int mouse_y = mouse_position.GetY();

            const auto mouse_world = m_scene->GetCamera()->TransformScreenToWorld(
                Vector2(
                    mouse_x / float(GetInputManager()->GetWindow()->GetExtent().width),
                    mouse_y / float(GetInputManager()->GetWindow()->GetExtent().height)
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
                            ray.TestTriangleList(
                                mesh_component->mesh->GetVertices(),
                                mesh_component->mesh->GetIndices(),
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
                if (m_screen_buffer.Size() != gpu_buffer_ref->size) {
                    m_screen_buffer.SetSize(gpu_buffer_ref->size);
                }

                gpu_buffer_ref->Read(g_engine->GetGPUDevice(), m_screen_buffer.Size(), m_screen_buffer.Data());
            }

            m_rtc_stream->GetEncoder()->PushData(std::move(m_screen_buffer));
        }
    }
}

// not overloading; just creating a method to handle camera movement
void SampleStreamer::HandleCameraMovement(GameCounter::TickUnit delta)
{
    if (m_input_manager->IsKeyDown(KEY_W)) {
        GetScene()->GetCamera()->GetCameraController()->PushCommand({
            CameraCommand::CAMERA_COMMAND_MOVEMENT,
            { .movement_data = { CameraCommand::CAMERA_MOVEMENT_FORWARD } }
        });
    }

    if (m_input_manager->IsKeyDown(KEY_S)) {
        GetScene()->GetCamera()->GetCameraController()->PushCommand({
            CameraCommand::CAMERA_COMMAND_MOVEMENT,
            { .movement_data = { CameraCommand::CAMERA_MOVEMENT_BACKWARD } }
        });
    }

    if (m_input_manager->IsKeyDown(KEY_A)) {
        GetScene()->GetCamera()->GetCameraController()->PushCommand({
            CameraCommand::CAMERA_COMMAND_MOVEMENT,
            { .movement_data = { CameraCommand::CAMERA_MOVEMENT_LEFT } }
        });
    }

    if (m_input_manager->IsKeyDown(KEY_D)) {
        GetScene()->GetCamera()->GetCameraController()->PushCommand({
            CameraCommand::CAMERA_COMMAND_MOVEMENT,
            { .movement_data = { CameraCommand::CAMERA_MOVEMENT_RIGHT } }
        });
    }

    // if (auto character = GetScene()->GetRoot().Select("zombie")) {
    //     character.SetWorldRotation(Quaternion::LookAt(GetScene()->GetCamera()->GetDirection(), GetScene()->GetCamera()->GetUpVector()));
    // }
}