
#include "SampleStreamer.hpp"

#include "system/SdlSystem.hpp"
#include "system/Debug.hpp"

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <Engine.hpp>
#include <rendering/Atomics.hpp>
#include <scene/animation/Bone.hpp>
#include <rendering/rt/AccelerationStructureBuilder.hpp>
#include <rendering/rt/ProbeSystem.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/LightController.hpp>
#include <scene/controllers/ShadowMapController.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/AtomicVar.hpp>

#include <util/json/JSON.hpp>

#include <core/net/Socket.hpp>

#include <Game.hpp>

#include <rendering/rt/BlurRadiance.hpp>

#include <ui/UIText.hpp>

#include <asset/serialization/fbom/FBOM.hpp>

#include <scene/terrain/controllers/TerrainPagingController.hpp>

#include <rendering/vct/VoxelConeTracing.hpp>

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

class FramebufferCaptureRenderComponent : public RenderComponent<FramebufferCaptureRenderComponent>
{
    Extent2D        m_window_size;
    Handle<Texture> m_texture;
    GPUBufferRef    m_buffer;

public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_SLOT6;

    FramebufferCaptureRenderComponent(const Extent2D window_size)
        : RenderComponent(),
          m_window_size(window_size)
    {
    }

    virtual ~FramebufferCaptureRenderComponent() = default;

    const GPUBufferRef &GetBuffer() const
        { return m_buffer; }

    const Handle<Texture> &GetTexture() const
        { return m_texture; }

    void Init()
    {
        m_texture = CreateObject<Texture>(Texture2D(
            m_window_size,
            InternalFormat::RGBA8,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        InitObject(m_texture);

        m_buffer = RenderObjects::Make<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);
        HYPERION_ASSERT_RESULT(m_buffer->Create(g_engine->GetGPUDevice(), m_texture->GetImage()->GetByteSize()));
        m_buffer->SetResourceState(renderer::ResourceState::COPY_DST);
        m_buffer->GetMapping(g_engine->GetGPUDevice());
    }

    void InitGame()
    {
        
    }

    void OnRemoved()
    {
        SafeRelease(std::move(m_buffer));
    }
    
    void OnUpdate(GameCounter::TickUnit delta)
    {
        // Do nothing
    }

    void OnRender(Frame *frame)
    {
        auto &deferred_renderer = g_engine->GetDeferredRenderer();

        if (auto *attachment = deferred_renderer.GetCombinedResult()->GetAttachment()) {
            renderer::Result result = renderer::Result::OK;

            AssertThrow(attachment != nullptr);
            AssertThrow(attachment->GetImage() != nullptr);

            const auto previous_resource_state = attachment->GetImage()->GetGPUImage()->GetResourceState();

            auto *command_buffer = frame->GetCommandBuffer();

            attachment->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

            m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

            HYPERION_PASS_ERRORS(
                m_texture->GetImage()->Blit(
                    command_buffer,
                    attachment->GetImage()
                ),
                result
            );

            m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
            m_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

            m_texture->GetImage()->CopyToBuffer(
                command_buffer,
                m_buffer
            );

            
            attachment->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
            m_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        } else {
            DebugLog(LogType::Error, "Could not get attachment\n");
        }
    }

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { }
};

class WebSocketMessageQueue
{
public:
    struct Message
    {
        Weak<rtc::WebSocket>    ws;
        json::JSONValue         message;        
    };

    WebSocketMessageQueue() = default;
    WebSocketMessageQueue(const WebSocketMessageQueue &other) = delete;
    WebSocketMessageQueue &operator=(const WebSocketMessageQueue &other) = delete;
    WebSocketMessageQueue(WebSocketMessageQueue &&other) = delete;
    WebSocketMessageQueue &operator=(WebSocketMessageQueue &&other) = delete;
    ~WebSocketMessageQueue() = default;

    void Push(json::JSONValue &&message)
    {
        std::lock_guard guard(m_mutex);

        m_messages.Push(std::move(message));
        m_size.Increment(1, MemoryOrder::SEQUENTIAL);
    }

    json::JSONValue Pop()
    {
        AssertThrow(!Empty());

        std::lock_guard guard(m_mutex);

        m_size.Decrement(1, MemoryOrder::SEQUENTIAL);
        return m_messages.Pop();
    }

    UInt Size() const
        { return m_size.Get(MemoryOrder::SEQUENTIAL); }

    bool Empty() const
        { return Size() == 0; }

private:
    std::mutex              m_mutex;
    Queue<json::JSONValue>  m_messages;
    AtomicVar<UInt>         m_size;
};

SampleStreamer::SampleStreamer(RC<Application> application)
    : Game(application)
{
}

void SampleStreamer::InitGame()
{
    Game::InitGame();

    // g_engine->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();

    m_message_queue.Reset(new WebSocketMessageQueue());
    
    m_rtc_instance.Reset(new RTCInstance(
        RTCServerParams {
            RTCServerAddress { "ws://127.0.0.1", 9945, "/server" }
        }
    ));

    m_rtc_stream = m_rtc_instance->CreateStream(
        RTCStreamType::RTC_STREAM_TYPE_VIDEO,
        UniquePtr<RTCStreamEncoder>(new GStreamerRTCStreamVideoEncoder())
    );

    m_rtc_stream->Start();

    AssertThrow(m_rtc_instance->GetServer() != nullptr);

    if (const RC<RTCServer> &server = m_rtc_instance->GetServer()) {
        server->GetCallbacks().On(RTCServerCallbackMessages::SERVER_ERROR, [](RTCServerCallbackData data) {
            DebugLog(LogType::Error, "Server error: %s\n", data.error.HasValue() ? data.error.Get().message.Data() : "<unknown>");
        });

        server->GetCallbacks().On(RTCServerCallbackMessages::SERVER_STARTED, [](RTCServerCallbackData) {
            DebugLog(LogType::Debug, "Server started\n");
        });

        server->GetCallbacks().On(RTCServerCallbackMessages::SERVER_STOPPED, [](RTCServerCallbackData) {
            DebugLog(LogType::Debug, "Server stopped\n");
        });

        server->GetCallbacks().On(RTCServerCallbackMessages::CLIENT_CONNECTED, [](RTCServerCallbackData) {
            DebugLog(LogType::Debug, "Client connected\n");
        });

        server->GetCallbacks().On(RTCServerCallbackMessages::CLIENT_DISCONNECTED, [](RTCServerCallbackData) {
            DebugLog(LogType::Debug, "Client disconnected\n");
        });

        server->GetCallbacks().On(RTCServerCallbackMessages::CLIENT_MESSAGE, [this](RTCServerCallbackData data) {
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
            
            m_message_queue->Push(std::move(json_value));
        });

        server->Start();
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
    m_scene->GetCamera()->SetCameraController(UniquePtr<FirstPersonCameraController>::Construct());

    { // allow ui rendering
        auto btn_node = GetUI().GetScene()->GetRoot().AddChild();
        btn_node.SetEntity(CreateObject<Entity>());
        btn_node.GetEntity()->SetTranslation(Vector3(0.0f, 0.85f, 0.0f));
        btn_node.GetEntity()->AddController<UIButtonController>();

        if (UIButtonController *controller = btn_node.GetEntity()->GetController<UIButtonController>()) {
            controller->SetScript(g_asset_manager->Load<Script>("scripts/examples/ui_controller.hypscript"));
        }

        btn_node.Scale(0.01f);

        m_scene->GetEnvironment()->AddRenderComponent<UIRenderer>(HYP_NAME(UIRenderer0), GetUI().GetScene());
    }

    m_scene->GetEnvironment()->AddRenderComponent<FramebufferCaptureRenderComponent>(HYP_NAME(StreamingCapture), window_size);

    {
        auto sun = CreateObject<Entity>();
        sun->SetName(HYP_NAME(Sun));
        sun->AddController<LightController>(CreateObject<Light>(DirectionalLight(
            Vector3(-0.105425f, 0.988823f, 0.105425f).Normalize(),
            Color(1.0f, 1.0f, 1.0f),
            5.0f
        )));
        sun->SetTranslation(Vector3(-0.105425f, 0.988823f, 0.105425f));
        sun->AddController<ShadowMapController>();
        GetScene()->AddEntity(sun);
    }
    
    // Test gaussian splatting
    if (true) {
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
            DebugLog(LogType::Debug, "Handle completed asset batch %s\n", it->first.LookupString().Data());

            HandleCompletedAssetBatch(it->first, it->second);

            it = m_asset_batches.Erase(it);
        } else {
            ++it;
        }
    }

    while (!m_message_queue->Empty()) {
        const json::JSONValue message = m_message_queue->Pop();

        DebugLog(LogType::Debug, "Recv message:\t%s\n", message.ToString().Data());

        const String message_type = message["type"].ToString();
        const String id = message["id"].ToString();
        
        if (message_type == "request") {
            RC<RTCClient> client = m_rtc_instance->GetServer()->CreateClient(id);
            DebugLog(LogType::Debug, "Adding client with ID %s  %s\n", id.Data(), typeid(*client).name());

            client->AddTrack(m_rtc_instance->CreateTrack(RTCTrackType::RTC_TRACK_TYPE_VIDEO));
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
                tracks.PushBack(track);
            }
        }
        
        RTCStreamDestination dest;
        dest.tracks = std::move(tracks);

        m_rtc_stream->SendSample(dest);
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

    const FramebufferCaptureRenderComponent *framebuffer_capture = m_scene->GetEnvironment()->GetRenderComponent<FramebufferCaptureRenderComponent>(HYP_NAME(StreamingCapture));

    if (framebuffer_capture) {
        const GPUBufferRef &gpu_buffer_ref = framebuffer_capture->GetBuffer();

        if (gpu_buffer_ref.IsValid()) {
            if (m_screen_buffer.Size() != gpu_buffer_ref->size) {
                m_screen_buffer.SetSize(gpu_buffer_ref->size);
            }

            gpu_buffer_ref->Read(g_engine->GetGPUDevice(), m_screen_buffer.Size(), m_screen_buffer.Data());
        }
        

        m_rtc_stream->GetEncoder()->PushData(std::move(m_screen_buffer));
        
#if 0
        m_counter++;

        if (m_counter % 100 != 99) {
            return;
        }

        const Handle<Texture> texture = framebuffer_capture->GetTexture();


        // Fire and forget
        TaskSystem::GetInstance().ScheduleTask([screen_buffer = std::move(m_screen_buffer), format = texture->GetFormat(), extent = texture->GetExtent()]() mutable
        {
            Bitmap<3> bitmap(extent.width, extent.height);

            const UInt num_components = renderer::NumComponents(format);

            for (UInt pixel = 0; pixel < screen_buffer.Size() / num_components; pixel++) {
                UByte components[4] = { 0 };

                for (UInt comp = 0; comp < MathUtil::Min(num_components, std::size(components)); comp++) {
                    components[comp] = screen_buffer.Data()[(pixel * num_components) + comp];
                }

                bitmap.GetPixelAtIndex(pixel).SetRGB(Vector3(
                    static_cast<Float>(components[0]) / 255.0f,
                    static_cast<Float>(components[1]) / 255.0f,
                    static_cast<Float>(components[2]) / 255.0f
                ));
            }

            bitmap.FlipVertical();

            bitmap.Write("screencap.bmp");
        }, THREAD_POOL_BACKGROUND);

#endif
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
}