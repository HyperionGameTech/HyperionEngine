
#include "SampleStreamer.hpp"

#include "system/Application.hpp"
#include "system/Debug.hpp"

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <Engine.hpp>
#include <rendering/Atomics.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/AabbDebugController.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/AudioComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
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

#include <rendering/rt/BlurRadiance.hpp>

#include <ui/UIText.hpp>

#include <asset/serialization/fbom/FBOM.hpp>

#include <scene/terrain/controllers/TerrainPagingController.hpp>

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

#include <mutex>
#include <ui/controllers/UIButtonController.hpp>

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
    : Game(application)
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
            } else if (type_id == TypeID::ForType<Int>()) {
                DebugLog(LogType::Debug, "Argument %s = %d\n", arg.first.Data(), arg.second.Get<Int>());
            } else if (type_id == TypeID::ForType<Float>()) {
                DebugLog(LogType::Debug, "Argument %s = %f\n", arg.first.Data(), arg.second.Get<Float>());
            } else if (type_id == TypeID::ForType<Bool>()) {
                DebugLog(LogType::Debug, "Argument %s = %s\n", arg.first.Data(), arg.second.Get<Bool>() ? "true" : "false");
            } else {
                DebugLog(LogType::Debug, "Argument %s = <unknown>\n", arg.first.Data());
            }
        }

        const String signalling_server_ip = arg_parse_result["SignallingServerIP"].Get<String>();
        const UInt16 signalling_server_port = UInt16(arg_parse_result["SignallingServerPort"].Get<Int>());

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
            server->GetCallbacks().On(RTCServerCallbackMessages::ERROR, [](RTCServerCallbackData data) {
                DebugLog(LogType::Error, "Server error: %s\n", data.error.HasValue() ? data.error.Get().message.Data() : "<unknown>");
            });

            server->GetCallbacks().On(RTCServerCallbackMessages::CONNECTED, [](RTCServerCallbackData) {
                DebugLog(LogType::Debug, "Server started\n");
            });

            server->GetCallbacks().On(RTCServerCallbackMessages::DISCONNECTED, [](RTCServerCallbackData) {
                DebugLog(LogType::Debug, "Server stopped\n");
            });

            server->GetCallbacks().On(RTCServerCallbackMessages::MESSAGE, [this](RTCServerCallbackData data) {
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

    /*m_scene->GetCamera()->SetCameraController(UniquePtr<FollowCameraController>::Construct(
        Vector3(0.0f, 7.0f, 0.0f), Vector3(0.0f, 0.0f, 5.0f)
    ));*/
    m_scene->GetCamera()->SetCameraController(RC<CameraController>(new FirstPersonCameraController()));

    {
        auto entity_id = m_scene->GetEntityManager()->AddEntity();

        auto cube = MeshBuilder::Cube();
        InitObject(cube);
        
        m_scene->GetEntityManager()->AddComponent(entity_id, MeshComponent {
            cube,
            g_material_system->GetOrCreate({
                .shader_definition = ShaderDefinition {
                    HYP_NAME(Forward),
                    ShaderProperties(renderer::static_mesh_vertex_attributes)
                },
                .bucket = Bucket::BUCKET_OPAQUE
            })
        });
        m_scene->GetEntityManager()->AddComponent(entity_id, BoundingBoxComponent {
            cube->GetAABB()
        });
        m_scene->GetEntityManager()->AddComponent(entity_id, TransformComponent {
            Transform(
                Vec3f(5.0f, 5.0f, 5.0f),
                Vec3f::one,
                Quaternion::Identity()
            )
        });
        m_scene->GetEntityManager()->AddComponent(entity_id, VisibilityStateComponent {

        });
        // TEMP
        // g_engine->GetWorld()->GetOctree().Insert(Handle<Entity>(entity_id).Get());

        // if (auto *controller = g_engine->GetComponents().Add<UIButtonController>(btn_node.GetEntity(), UniquePtr<UIButtonController>::Construct())) {
        //     controller->SetScript(g_asset_manager->Load<Script>("scripts/examples/ui_controller.hypscript"));
        // }

    }

    // // Add a reflection probe
    // TEMP: Commented out due to blending issues with multiple reflection probes
    // m_scene->GetEnvironment()->AddRenderComponent<ReflectionProbeRenderer>(
    //     HYP_NAME(ReflectionProbe0),
    //     Vec3f(0.0f)
    // );

    // m_scene->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(HYP_NAME(StreamingCapture), window_size);

    {
        auto sun = CreateObject<Light>(DirectionalLight(
            Vec3f(-0.1f, 0.65f, 0.1f).Normalize(),
            Color(1.0f, 1.0f, 1.0f),
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
            Color(1.0f, 1.0f, 1.0f),
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
        batch->Add<Node>("test_model", "models/sponza/sponza.obj");//living_room/living_room.obj");
        batch->Add<Node>("zombie", "models/ogrexml/dragger_Body.mesh.xml");
        batch->LoadAsync();
        auto results = batch->AwaitResults();

#if 0
        if (auto zombie = results["zombie"].Get<Node>()) {
            auto zombie_entity = zombie[0].GetEntity();

            m_scene->GetRoot().AddChild(zombie);

            // if (auto *animation_controller = g_engine->GetComponents().Add<AnimationController>(zombie_entity, UniquePtr<AnimationController>::Construct(zombie_entity->GetSkeleton()))) {
            //     animation_controller->Play(1.0f, LoopMode::REPEAT);
            // }

            if (zombie_entity.IsValid()) {
                if (auto *mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(zombie_entity)) {
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.25f);
                    mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
                }

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
#endif

        if (results["test_model"]) {
            auto node = results["test_model"].ExtractAs<Node>();
            node.Scale(0.01f);
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
    
    // Test gaussian splatting
    if (false) {
        auto batch = g_asset_manager->CreateBatch();
        batch->Add<json::JSONValue>("cameras json", "models/gaussian_splatting/cameras.json");
        batch->Add<PLYModelLoader::PLYModel>("ply model", "models/gaussian_splatting/point_cloud.ply");

        batch->GetCallbacks().On(ASSET_BATCH_ITEM_COMPLETE, [](AssetBatchCallbackData data)
        {
            const String &key = data.GetAssetKey();
            
            DebugLog(LogType::Debug, "Asset %s loaded\n", key.Data());
        });

        batch->LoadAsync();

        m_asset_batches.Insert(HYP_NAME(GaussianSplatting), std::move(batch));
    }

    // Test voxelizing model
    // if (true) {
    //     auto batch = g_asset_manager->CreateBatch();
    //     batch->Add<Node>("test_voxelizer_model", "models/suzanne.obj");

    //     batch->GetCallbacks().On(ASSET_BATCH_ITEM_COMPLETE, [](AssetBatchCallbackData data)
    //     {
    //         const String &key = data.GetAssetKey();
            
    //         DebugLog(LogType::Debug, "Asset %s loaded\n", key.Data());
    //     });

    //     batch->LoadAsync();

    //     m_asset_batches.Insert(HYP_NAME(TestVoxelizerModel), std::move(batch));
    // }

    auto test_button = GetUI().CreateButton(
        Vector2(0.0f, 0.0f),
        Vector2(0.2f, 0.5f),
        "Test Button"
    );

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
            UInt32  width;
            UInt32  height;
            Vector3 position;
            Matrix3 rotation;
            Float   fx;
            Float   fy;
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
                definition.fx       = Float(item["fx"].ToNumber());
                definition.fy       = Float(item["fy"].ToNumber());

                if (item["position"].IsArray()) {
                    definition.position = Vector3(
                        Float(item["position"][0].ToNumber()),
                        Float(item["position"][1].ToNumber()),
                        Float(item["position"][2].ToNumber())
                    );
                }

                if (item["rotation"].IsArray()) {
                    Float v[9] = {
                        Float(item["rotation"][0][0].ToNumber()),
                        Float(item["rotation"][0][1].ToNumber()),
                        Float(item["rotation"][0][2].ToNumber()),
                        Float(item["rotation"][1][0].ToNumber()),
                        Float(item["rotation"][1][1].ToNumber()),
                        Float(item["rotation"][1][2].ToNumber()),
                        Float(item["rotation"][2][0].ToNumber()),
                        Float(item["rotation"][2][1].ToNumber()),
                        Float(item["rotation"][2][2].ToNumber())
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

            up_direction /= Float(all_up_directions.Size());
            up_direction.Normalize();

            const Vector3 axis = up_direction.Cross(Vector3::UnitY()).Normalize();

            const Float cos_theta = up_direction.Dot(Vector3::UnitY());
            const Float theta = MathUtil::Arccos(cos_theta);

            camera_offset_rotation = Quaternion(axis, theta).Invert();
        }

        DebugLog(LogType::Debug, "Up direction = %f, %f, %f\n", up_direction.x, up_direction.y, up_direction.z);

        //const auto camera_rotation = camera_definitions.Any() ? Quaternion(Matrix4(camera_definitions[0].rotation)).Invert() : Quaternion::identity;

        auto ply_model = loaded_assets["ply model"].Get<PLYModelLoader::PLYModel>();

        const SizeType num_points = ply_model->vertices.Size();

        RC<GaussianSplattingModelData> gaussian_splatting_model = RC<GaussianSplattingModelData>::Construct();
        gaussian_splatting_model->points.Resize(num_points);
        gaussian_splatting_model->transform.SetRotation(Quaternion(Vector3(1, 0, 0), M_PI));

        const Bool has_rotations = ply_model->custom_data.Contains("rot_0")
            && ply_model->custom_data.Contains("rot_1")
            && ply_model->custom_data.Contains("rot_2")
            && ply_model->custom_data.Contains("rot_3");

        const Bool has_scales = ply_model->custom_data.Contains("scale_0")
            && ply_model->custom_data.Contains("scale_1")
            && ply_model->custom_data.Contains("scale_2");

        const Bool has_sh = ply_model->custom_data.Contains("f_dc_0")
            && ply_model->custom_data.Contains("f_dc_1")
            && ply_model->custom_data.Contains("f_dc_2");

        const Bool has_opacity = ply_model->custom_data.Contains("opacity");

        for (SizeType index = 0; index < num_points; index++) {
            auto &out_point = gaussian_splatting_model->points[index];

            out_point.position = Vector4(ply_model->vertices[index].GetPosition(), 1.0f);

            if (has_rotations) {
                Quaternion rotation;

                ply_model->custom_data["rot_0"].Read(index * sizeof(Float), &rotation.w);
                ply_model->custom_data["rot_1"].Read(index * sizeof(Float), &rotation.x);
                ply_model->custom_data["rot_2"].Read(index * sizeof(Float), &rotation.y);
                ply_model->custom_data["rot_3"].Read(index * sizeof(Float), &rotation.z);
                
                rotation.Normalize();

                out_point.rotation = rotation;
            }

            if (has_scales) {
                Vector3 scale = Vector3::one;

                ply_model->custom_data["scale_0"].Read(index * sizeof(Float), &scale.x);
                ply_model->custom_data["scale_1"].Read(index * sizeof(Float), &scale.y);
                ply_model->custom_data["scale_2"].Read(index * sizeof(Float), &scale.z);

                out_point.scale = Vector4(scale, 1.0f);
            }

            if (has_sh) {
                Float f_dc_0 = 0.0f;
                Float f_dc_1 = 0.0f;
                Float f_dc_2 = 0.0f;
                Float opacity = 1.0f;

                static constexpr Float SH_C0 = 0.28209479177387814f;

                ply_model->custom_data["f_dc_0"].Read(index * sizeof(Float), &f_dc_0);
                ply_model->custom_data["f_dc_1"].Read(index * sizeof(Float), &f_dc_1);
                ply_model->custom_data["f_dc_2"].Read(index * sizeof(Float), &f_dc_2);

                if (has_opacity) {
                    ply_model->custom_data["opacity"].Read(index * sizeof(Float), &opacity);
                }

                out_point.color = Vector4(
                    0.5f + (SH_C0 * f_dc_0),
                    0.5f + (SH_C0 * f_dc_1),
                    0.5f + (SH_C0 * f_dc_2),
                    1.0f / (1.0f + MathUtil::Exp(-opacity))
                );
            }
        }
        
        UInt camera_definition_index = 0;
        
        RC<CameraTrack> camera_track = RC<CameraTrack>::Construct();
        camera_track->SetDuration(60.0);

        //m_scene->GetCamera()->SetCameraController(UniquePtr<CameraTrackController>::Construct(camera_track));

        for (const auto &camera_definition : camera_definitions) {
            camera_track->AddPivot({
                Double(camera_definition_index) / Double(camera_definitions.Size()),
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