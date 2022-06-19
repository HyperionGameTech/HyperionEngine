#ifndef HYPERION_V2_POST_FX_H
#define HYPERION_V2_POST_FX_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"
#include "graphics.h"
#include "mesh.h"
#include "full_screen_pass.h"

#include <core/lib/type_map.h>
#include <types.h>

#include <rendering/backend/renderer_frame.h>
#include <rendering/backend/renderer_structs.h>
#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_command_buffer.h>


#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::CommandBuffer;
using renderer::PerFrameData;
using renderer::VertexAttributeSet;
using renderer::UniformBuffer;

class Engine;

struct alignas(16) PostProcessingUniforms {
    UInt effect_counts[2]; // pre, post
    UInt last_enabled_indices[2]; // pre, post
    UInt masks[2]; // pre, post
};

class PostProcessingEffect : public EngineComponentBase<STUB_CLASS(PostProcessingEffect)> {
public:
    enum class Stage {
        PRE_SHADING,
        POST_SHADING
    };

    PostProcessingEffect(Stage stage, UInt index);
    PostProcessingEffect(const PostProcessingEffect &other) = delete;
    PostProcessingEffect &operator=(const PostProcessingEffect &other) = delete;
    virtual ~PostProcessingEffect();

    FullScreenPass &GetFullScreenPass()             { return m_full_screen_pass; }
    const FullScreenPass &GetFullScreenPass() const { return m_full_screen_pass; }

    Ref<Shader> &GetShader()                        { return m_shader; }
    const Ref<Shader> &GetShader() const            { return m_shader; }

    Stage GetStage() const                          { return m_stage; }
    UInt GetIndex() const                           { return m_full_screen_pass.GetSubDescriptorIndex(); }

    bool IsEnabled() const                          { return m_is_enabled; }
    void SetIsEnabled(bool is_enabled)              { m_is_enabled = is_enabled; }

    void Init(Engine *engine);

protected:
    virtual Ref<Shader> CreateShader(Engine *engine) = 0;

    FullScreenPass m_full_screen_pass;

private:
    Ref<Shader>    m_shader;
    Stage          m_stage;
    bool           m_is_enabled;
};

class PostProcessing {
public:
    static constexpr UInt max_effects_per_stage = sizeof(UInt) * 8;

    enum DefaultEffectIndices {
        DEFAULT_EFFECT_INDEX_SSAO,
        DEFAULT_EFFECT_INDEX_FXAA
    };

    PostProcessing();
    PostProcessing(const PostProcessing &) = delete;
    PostProcessing &operator=(const PostProcessing &) = delete;
    ~PostProcessing();

    /*! \brief Add an effect to the stack to be processed BEFORE deferred rendering happens.
    * Note, cannot add new filters after pipeline construction, currently
     * @param effect A unique_ptr to the class, derived from PostProcessingEffect.
     */
    template <class EffectClass>
    void AddEffect(std::unique_ptr<EffectClass> &&effect)
    {
        const PostProcessingEffect::Stage stage = EffectClass::stage;

        if constexpr (stage == PostProcessingEffect::Stage::PRE_SHADING) {
            AddEffectInternal(std::move(effect), m_pre_effects);
        } else {
            AddEffectInternal(std::move(effect), m_post_effects);
        }
    }

    /*! \brief Add an effect to the stack to be processed BEFORE deferred rendering happens, constructed by the given arguments.
     * Note, cannot add new filters after pipeline construction, currently
     */
    template <class EffectClass, class ...Args>
    void AddEffect(Args &&... args)
    {
        const PostProcessingEffect::Stage stage = EffectClass::stage;

        if constexpr (stage == PostProcessingEffect::Stage::PRE_SHADING) {
            AddEffectInternal(std::make_unique<EffectClass>(std::forward<Args>(args)...), m_pre_effects);
        } else {
            AddEffectInternal(std::make_unique<EffectClass>(std::forward<Args>(args)...), m_post_effects);
        }
    }

    /*! \brief Get an effect added to the list of effects to be applied BEFORE deferred rendering happens */
    template <class EffectClass>
    EffectClass *GetEffect() const
    {
        const PostProcessingEffect::Stage stage = EffectClass::stage;

        if constexpr (stage == PostProcessingEffect::Stage::PRE_SHADING) {
            return GetEffectInternal<EffectClass>(m_pre_effects);
        } else {
            return GetEffectInternal<EffectClass>(m_post_effects);
        }
    }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void RenderPre(Engine *engine, Frame *frame) const;
    void RenderPost(Engine *engine, Frame *frame) const;

private:
    void CreateUniformBuffer(Engine *engine);

    template <class EffectClass>
    void AddEffectInternal(std::unique_ptr<EffectClass> &&effect, TypeMap<std::unique_ptr<PostProcessingEffect>> &effects)
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        if (!effects.Contains<EffectClass>()) {
            AssertThrowMsg(effects.Size() < max_effects_per_stage, "Maximum number of effects in stage");
        }

        effects.Set<EffectClass>(std::move(effect));
    }

    template <class EffectClass>
    EffectClass *GetEffectInternal(TypeMap<std::unique_ptr<PostProcessingEffect>> &effects) const
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        auto it = effects.Find<EffectClass>();

        if (it == effects.End()) {
            return nullptr;
        }

        return static_cast<EffectClass *>(it->second.get());
    }

    TypeMap<std::unique_ptr<PostProcessingEffect>> m_pre_effects;
    TypeMap<std::unique_ptr<PostProcessingEffect>> m_post_effects;
    UniformBuffer                                  m_uniform_buffer;
};

} // namespace hyperion::v2

#endif // HYPERION_V2_POST_FX_H

