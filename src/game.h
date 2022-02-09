#ifndef GAME_H
#define GAME_H

#include "render_window.h"
#include "rendering/renderer.h"
#include "rendering/ui/ui_manager.h"
#include "rendering/camera/camera.h"
#include "input_manager.h"
#include "entity.h"

namespace hyperion {
class Game {
public:
    Game(const RenderWindow &window);
    virtual ~Game();

    inline InputManager *GetInputManager() const { return m_input_manager; }
    inline UIManager *GetUIManager() const { return m_ui_manager; }
    inline Renderer *GetRenderer() { return m_renderer; }
    inline const Renderer *GetRenderer() const { return m_renderer; }
    inline std::shared_ptr<Entity> &GetScene() { return m_scene; }
    inline const std::shared_ptr<Entity> &GetScene() const { return m_scene; }
    inline std::shared_ptr<Entity> &GetUI() { return m_ui; }
    inline const std::shared_ptr<Entity> &GetUI() const { return m_ui; }
    inline Camera *GetCamera() { return m_camera; }
    inline const Camera *GetCamera() const { return m_camera; }

    virtual void Initialize() = 0;
    void Update(double dt);
    void Render();
    virtual void Logic(double dt) = 0;
    virtual void OnRender() = 0;

protected:
    Camera *m_camera;
    std::shared_ptr<Entity> m_scene;
    std::shared_ptr<Entity> m_ui;
    InputManager *m_input_manager;
    UIManager *m_ui_manager;
    Renderer * const m_renderer;

private:
    void PreRender();
    void PostRender();
};
} // namespace hyperion

#endif
