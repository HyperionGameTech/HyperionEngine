#include <Game.hpp>
#include <HyperionEngine.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/String.hpp>
#include <core/net/MessageQueue.hpp>
#include <core/net/Socket.hpp>
#include <core/system/SharedMemory.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/UIRenderer.hpp>
#include <rendering/render_components/ScreenCapture.hpp>
#include <scene/camera/FirstPersonCamera.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <system/CommandQueue.hpp>
#include <system/StackDump.hpp>

#include <sys/fcntl.h>
#include <sys/semaphore.h>
#include <unistd.h>

using namespace hyperion;

SharedMemory *framebuffer_shared = nullptr;
SharedMemory *command_queue_shared = nullptr;

sem_t *semaphore = nullptr;

class Editor : public Game {
public:
  Editor(RC<Application> application);
  virtual ~Editor();

  virtual void InitGame() override;
  virtual void InitRender() override;

  virtual void Teardown() override;
  virtual void Logic(GameCounter::TickUnit delta) override;
  virtual void OnInputEvent(const SystemEvent &event) override;

  virtual void OnFrameEnd(Frame *frame) override;

private:
  ByteBuffer m_screen_buffer;
  net::SocketServer m_socket_server;
  net::MessageQueue m_message_queue;
};

Editor::Editor(RC<Application> application)
    : Game(application), m_socket_server("hyperion_editor_serv1.sock") {}

Editor::~Editor() { m_socket_server.Stop(); }

void Editor::InitGame() {
  Game::InitGame();

  { // init server
    DebugLog(LogType::Info, "Starting editor socket server\n");

    m_socket_server.SetEventProc(
        HYP_NAME(OnServerStarted), [](Array<net::SocketProcArgument> &&args) {
          DebugLog(LogType::Info, "Socket server started\n");
        });

    m_socket_server.SetEventProc(
        HYP_NAME(OnError), [](Array<net::SocketProcArgument> &&args) {
          DebugLog(LogType::Error, "Socket error: %s\n",
                   args[0].Get<String>().Data());
        });

    m_socket_server.SetEventProc(
        HYP_NAME(OnClientConnected), [](Array<net::SocketProcArgument> &&args) {
          DebugLog(LogType::Info, "Socket client connected: %s\n",
                   args[0].Get<Name>().LookupString());
        });

    m_socket_server.SetEventProc(HYP_NAME(OnClientDisconnected),
                                 [](Array<net::SocketProcArgument> &&args) {
                                   DebugLog(LogType::Info,
                                            "Socket client disconnected: %s\n",
                                            args[0].Get<Name>().LookupString());
                                 });

    m_socket_server.SetEventProc(
        HYP_NAME(OnClientData), [](Array<net::SocketProcArgument> &&args) {
          DebugLog(LogType::Info, "Socket message received from %s\n",
                   args[0].Get<Name>().LookupString());
        });

    if (!m_socket_server.Start()) {
      HYP_THROW("Failed to start editor server");
    }
  }

  m_scene->SetCamera(CreateObject<Camera>(70.0f, 1024, 1024, 0.01f, 30000.0f));

  m_scene->GetCamera()->SetCameraController(
      RC<CameraController>(new FirstPersonCameraController()));

  m_scene->GetEnvironment()->AddRenderComponent<ScreenCaptureRenderComponent>(
      HYP_NAME(StreamingCapture), Extent2D{1024, 1024});

  auto batch = g_asset_manager->CreateBatch();
  batch->Add("test_model", "models/pica_pica/pica_pica.obj");
  batch->LoadAsync();
  auto results = batch->AwaitResults();

  if (results["test_model"]) {
    auto node = results["test_model"].ExtractAs<Node>();
    node.Scale(3.0f);
    node.SetName("test_model");

    GetScene()->GetRoot().AddChild(node);

    auto env_grid_entity = m_scene->GetEntityManager()->AddEntity();

    m_scene->GetEntityManager()->AddComponent(
        env_grid_entity, TransformComponent{node.GetWorldTransform()});

    m_scene->GetEntityManager()->AddComponent(
        env_grid_entity,
        BoundingBoxComponent{node.GetLocalAABB(), node.GetWorldAABB()});

    // Add env grid component
    m_scene->GetEntityManager()->AddComponent(
        env_grid_entity, EnvGridComponent{EnvGridType::ENV_GRID_TYPE_SH});
  }

  // Add sun
  {

    auto sun = CreateObject<Light>(DirectionalLight(
        Vec3f(-0.1f, 0.65f, 0.1f).Normalize(), Color(1.0f, 1.0f, 1.0f), 5.0f));

    InitObject(sun);

    auto sun_entity = m_scene->GetEntityManager()->AddEntity();

    m_scene->GetEntityManager()->AddComponent(
        sun_entity,
        TransformComponent{Transform(Vec3f(-0.1f, 0.65f, 0.1f).Normalized(),
                                     Vec3f::one, Quaternion::Identity())});

    m_scene->GetEntityManager()->AddComponent(sun_entity, LightComponent{sun});

    m_scene->GetEntityManager()->AddComponent(
        sun_entity,
        ShadowMapComponent{.radius = 12.0f, .resolution = {2048, 2048}});
  }

  // Add Skybox
  {
    auto skybox_entity = m_scene->GetEntityManager()->AddEntity();

    m_scene->GetEntityManager()->AddComponent(
        skybox_entity, TransformComponent{Transform(Vec3f::zero, Vec3f(10.0f),
                                                    Quaternion::Identity())});

    m_scene->GetEntityManager()->AddComponent(skybox_entity, SkyComponent{});
    m_scene->GetEntityManager()->AddComponent(
        skybox_entity,
        VisibilityStateComponent{VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE});
    m_scene->GetEntityManager()->AddComponent(
        skybox_entity,
        BoundingBoxComponent{BoundingBox(Vec3f(-100.0f), Vec3f(100.0f))});
  }

  auto test_button = GetUI().CreateButton(Vector2(0.0f, 0.0f),
                                          Vector2(0.2f, 0.5f), "Test Button");

  m_scene->GetEnvironment()->AddRenderComponent<UIRenderer>(
      HYP_NAME(UIRenderer0), GetUI().GetScene());
}

void Editor::InitRender() { Game::InitRender(); }

void Editor::Teardown() { Game::Teardown(); }

void Editor::Logic(GameCounter::TickUnit delta) {
  // Rewrite command queue
#if 0
    if (command_queue_shared != nullptr) {
        sem_wait(semaphore);
        CommandQueue command_queue;

        UInt32 num_commands = CommandQueue::ReadCommandQueue(
            static_cast<UByte *>(command_queue_shared->GetAddress()),
            command_queue_shared->GetSize(),
            command_queue
        );

        sem_post(semaphore);

        // respond to commands, write each as read
        DebugLog(LogType::Debug, "Server has %u commands \n", command_queue.GetEntries().Size());

        while (command_queue.GetEntries().Any()) {
            auto entry = command_queue.GetEntries().PopFront();

            for (auto &arg : entry.args) {
                DebugLog(
                    LogType::Debug,
                    "\tArgument: %s\n",
                    arg.Data()
                );
            }
        }

        DebugLog(LogType::Debug, "(server) Read %u commands\n", num_commands);

        sem_wait(semaphore);
        command_queue.Write(*command_queue_shared);
        sem_post(semaphore);
    }
#endif
}

void Editor::OnInputEvent(const SystemEvent &event) {}

void Editor::OnFrameEnd(Frame *frame) {
  const ScreenCaptureRenderComponent *screen_capture =
      m_scene->GetEnvironment()
          ->GetRenderComponent<ScreenCaptureRenderComponent>(
              HYP_NAME(StreamingCapture));

  if (screen_capture) {
    const GPUBufferRef &gpu_buffer_ref = screen_capture->GetBuffer();

    if (gpu_buffer_ref.IsValid()) {
      if (m_screen_buffer.Size() != gpu_buffer_ref->size) {
        m_screen_buffer.SetSize(gpu_buffer_ref->size);
      }

      gpu_buffer_ref->Read(g_engine->GetGPUDevice(), m_screen_buffer.Size(),
                           m_screen_buffer.Data());
    }

    AssertThrow(m_screen_buffer.Size() == 1024 * 1024 * 4);

    AssertThrow(framebuffer_shared != nullptr);
    AssertThrow(framebuffer_shared->GetSize() == m_screen_buffer.Size());

    // test - write red
    // for (SizeType i = 0; i < m_screen_buffer.Size(); i += 4) {
    //     m_screen_buffer.Data()[i] = 255;
    //     m_screen_buffer.Data()[i + 1] = 0;
    //     m_screen_buffer.Data()[i + 2] = 0;
    //     m_screen_buffer.Data()[i + 3] = 255;
    // }

    framebuffer_shared->Write(m_screen_buffer.Data(), m_screen_buffer.Size());

    // m_rtc_stream->GetEncoder()->PushData(std::move(m_screen_buffer));
  }
}

void HandleSignal(int signum) {
  // Dump stack trace
  DebugLog(LogType::Warn, "Received signal %d\n", signum);

  if (signum == SIGSEGV || signum == SIGABRT || signum == SIGFPE ||
      signum == SIGTRAP || signum == SIGILL) {
    const StackDump stack_dump;

    fprintf(stderr, "Received signal %d\n", signum);
    fprintf(stderr, "%s\n", stack_dump.ToString().Data());

    fflush(stderr);

    std::abort();

    return;
  }

  if (g_engine->m_stop_requested.Get(MemoryOrder::RELAXED)) {
    DebugLog(LogType::Warn, "Forcing stop\n");

    fflush(stdout);

    exit(signum);

    return;
  }

  g_engine->RequestStop();

  while (g_engine->IsRenderLoopActive())
    ;

  exit(signum);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, HandleSignal);
  signal(SIGSEGV, HandleSignal);
  signal(SIGABRT, HandleSignal);
  signal(SIGFPE, HandleSignal);
  signal(SIGILL, HandleSignal);
  signal(SIGTRAP, HandleSignal);

  if (argc > 1) {
    framebuffer_shared = new SharedMemory(argv[1], 1024 * 1024 * 4,
                                          SharedMemory::Mode::READ_WRITE);
    AssertThrow(framebuffer_shared->Open());

    if (argc > 2) {
      command_queue_shared = new SharedMemory(argv[2], 1024 * 1024,
                                              SharedMemory::Mode::READ_WRITE);
      AssertThrow(command_queue_shared->Open());
    }

    if (argc > 3) {
      semaphore = sem_open(argv[3], O_RDWR);
      AssertThrowMsg(semaphore != SEM_FAILED,
                     "Failed to open semaphore with key %s!", argv[3]);
      DebugLog(LogType::Debug, "Opened server side semaphore\n");
      fflush(stdout);
    }
  }

  RC<Application> application(new SDLApplication("My Application", argc, argv));

  application->SetCurrentWindow(application->CreateSystemWindow(
      {"Hyperion Engine", 1024, 1024, WINDOW_FLAGS_HEADLESS}));

  hyperion::InitializeApplication(application);

  Editor editor(application);
  g_engine->InitializeGame(&editor);

  SystemEvent event;

  while (g_engine->IsRenderLoopActive()) {
    // input manager stuff
    while (application->PollEvent(event)) {
      editor.HandleEvent(std::move(event));
    }

    g_engine->RenderNextFrame(&editor);
  }

  DebugLog(LogType::Info, "Exiting editor\n");

  return 0;
}
