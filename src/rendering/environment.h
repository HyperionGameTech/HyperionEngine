#ifndef HYPERION_V2_ENVIRONMENT_H
#define HYPERION_V2_ENVIRONMENT_H

#include "base.h"
#include "shadows.h"
#include "light.h"

#include <core/lib/type_map.h>
#include <types.h>

#include <mutex>
#include <vector>

namespace hyperion::renderer {

class Frame;

} // namespace hyperion::renderer

namespace hyperion::v2 {

class Engine;

class Environment : public EngineComponentBase<STUB_CLASS(Environment)> {
    using ShadowRendererPtr = std::unique_ptr<ShadowRenderer>;

public:
    Environment();
    Environment(const Environment &other) = delete;
    Environment &operator=(const Environment &other) = delete;
    ~Environment();
    
    Ref<Light> &GetLight(size_t index)                    { return m_lights[index]; }
    const Ref<Light> &GetLight(size_t index) const        { return m_lights[index]; }
    void AddLight(Ref<Light> &&light);
    size_t NumLights() const                              { return m_lights.size(); }
    const std::vector<Ref<Light>> &GetLights() const      { return m_lights; }

    size_t NumShadowRenderers() const                     { return m_shadow_renderers.size(); }
    ShadowRenderer *GetShadowRenderer(size_t index) const { return m_shadow_renderers[index].get(); }

    void AddShadowRenderer(Engine *engine, std::unique_ptr<ShadowRenderer> &&shadow_renderer);
    void RemoveShadowRenderer(Engine *engine, size_t index);

    template <class T>
    void AddRenderComponent(std::unique_ptr<T> &&component)
    {
        m_render_components.Set<T>(std::move(component));
    }

    template <class T>
    auto &&GetRenderComponent()
    {
        if (!m_render_components.Contains<T>()) {
            return nullptr;
        }

        return m_render_components.At<T>();
    }

    template <class T>
    void RemoveRenderComponent()
    {
        m_render_components.Remove<T>();
    }

    float GetGlobalTimer() const { return m_global_timer; }

    void Init(Engine *engine);
    void Update(Engine *engine, GameCounter::TickUnit delta);

    void RenderComponents(Engine *engine, Frame *frame);

    void RenderShadows(Engine *engine, Frame *frame);

private:
    RenderComponentSet m_render_components;

    void UpdateShadows(Engine *engine, GameCounter::TickUnit delta);

    std::vector<Ref<Light>>                      m_lights;
    std::vector<ShadowRendererPtr>               m_shadow_renderers;

    float                                        m_global_timer;
};

} // namespace hyperion::v2

#endif