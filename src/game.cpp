#include "game.h"
#include "rendering/camera/fps_camera.h"

namespace hyperion {

Game::Game(const RenderWindow &window) 
    : m_input_manager(new InputManager),
      m_ui_manager(new UIManager(m_input_manager)),
      m_renderer(new Renderer(window)),
      m_scene(new Entity("Scene")),
      m_ui(new Entity("UI"))
{
    m_camera = new FpsCamera(
        GetInputManager(),
        &m_renderer->GetRenderWindow(),
        m_renderer->GetRenderWindow().GetScaledWidth(),
        m_renderer->GetRenderWindow().GetScaledHeight(),
        75.0f,
        0.5f,
        750.0f
    );
}

Game::~Game()
{
    delete m_camera;
    delete m_input_manager;
    delete m_ui_manager;
    delete m_renderer;
}

void Game::Update(double dt)
{
    m_ui_manager->Update(dt);

    Logic(dt);

    m_camera->Update(dt);

    m_scene->Update(dt);
    m_ui->Update(dt);
}

void Game::PreRender()
{
    m_renderer->Collect(m_camera, m_scene.get());
    m_renderer->Collect(m_camera, m_ui.get());
}

void Game::Render()
{
    PreRender();

    m_renderer->Begin(m_camera);

    OnRender();

    m_renderer->Render(m_camera);
    m_renderer->End(m_camera);

    PostRender();
}

void Game::PostRender()
{
    m_scene->ClearPendingRemoval();
    m_ui->ClearPendingRemoval();
}

} // namespace hyperion
