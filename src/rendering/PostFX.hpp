#ifndef HYPERION_V2_POST_FX_H
#define HYPERION_V2_POST_FX_H

#include "Framebuffer.hpp"
#include "Shader.hpp"
#include "Renderer.hpp"
#include "Mesh.hpp"
#include "FullScreenPass.hpp"

#include <core/lib/TypeMap.hpp>
#include <core/ThreadSafeContainer.hpp>
#include <Threads.hpp>
#include <Types.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>


#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::CommandBuffer;
using renderer::PerFrameData;
using renderer::VertexAttributeSet;
using renderer::UniformBuffer;
using renderer::DescriptorSet;
using renderer::ShaderVec2;

class Engine;

struct alignas(16) PostProcessingUniforms
{
    ShaderVec2<UInt32> effect_counts; // pre, post
    ShaderVec2<UInt32> last_enabled_indices; // pre, post
    ShaderVec2<UInt32> masks; // pre, post
};

class PostFXPass
    : public FullScreenPass
{
public:
    PostFXPass(
        InternalFormat image_format = InternalFormat::RGB8_SRGB
    );
    PostFXPass(
        const Handle<Shader> &shader,
        InternalFormat image_format = InternalFormat::RGB8_SRGB
    );
    PostFXPass(
        const Handle<Shader> &shader,
        DescriptorKey descriptor_key,
        UInt sub_descriptor_index,
        InternalFormat image_format = InternalFormat::RGB8_SRGB
    );
    PostFXPass(const PostFXPass &) = delete;
    PostFXPass &operator=(const PostFXPass &) = delete;
    virtual ~PostFXPass();

    virtual void CreateDescriptors() override;
};

class PostProcessingEffect
    : public EngineComponentBase<STUB_CLASS(PostProcessingEffect)>
{
public:
    enum class Stage
    {
        PRE_SHADING,
        POST_SHADING
    };

    PostProcessingEffect(
        Stage stage,
        UInt index,
        InternalFormat image_format = InternalFormat::RGBA16F//RGBA8_SRGB
    );
    PostProcessingEffect(const PostProcessingEffect &other) = delete;
    PostProcessingEffect &operator=(const PostProcessingEffect &other) = delete;
    virtual ~PostProcessingEffect();

    PostFXPass &GetPass() { return m_pass; }
    const PostFXPass &GetPass() const { return m_pass; }

    Handle<Shader> &GetShader() { return m_shader; }
    const Handle<Shader> &GetShader() const { return m_shader; }

    Stage GetStage() const { return m_stage; }
    UInt GetIndex() const { return m_pass.GetSubDescriptorIndex(); }

    bool IsEnabled() const { return m_is_enabled; }
    void SetIsEnabled(bool is_enabled) { m_is_enabled = is_enabled; }

    void Init();

    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;

protected:
    virtual Handle<Shader> CreateShader() = 0;

    PostFXPass m_pass;

private:
    Handle<Shader> m_shader;
    Stage m_stage;
    bool m_is_enabled;
};

class PostProcessing
{
public:
    static constexpr UInt max_effects_per_stage = sizeof(UInt32) * CHAR_BIT;

    enum DefaultEffectIndices
    {
        DEFAULT_EFFECT_INDEX_SSAO,
        DEFAULT_EFFECT_INDEX_FXAA
    };

    PostProcessing();
    PostProcessing(const PostProcessing &) = delete;
    PostProcessing &operator=(const PostProcessing &) = delete;
    ~PostProcessing();

    /*! \brief Add an effect to the stack to be processed BEFORE deferred rendering happens.
    * Note, cannot add new filters after pipeline construction, currently
     * @param effect A UniquePtr to the class, derived from PostProcessingEffect.
     */
    template <class EffectClass>
    void AddEffect(UniquePtr<EffectClass> &&effect)
    {
        const PostProcessingEffect::Stage stage = EffectClass::stage;

        AddEffectInternal(stage, std::move(effect));
    }

    /*! \brief Add an effect to the stack to be processed BEFORE deferred rendering happens, constructed by the given arguments.
     * Note, cannot add new filters after pipeline construction, currently
     */
    template <class EffectClass, class ...Args>
    void AddEffect(Args &&... args)
    {
        const PostProcessingEffect::Stage stage = EffectClass::stage;

        AddEffectInternal(stage, UniquePtr<EffectClass>::Construct(std::forward<Args>(args)...));
    }

    /*! \brief Get an effect added to the list of effects to be applied BEFORE deferred rendering happens */
    template <class EffectClass>
    EffectClass *GetPass() const
    {
        const PostProcessingEffect::Stage stage = EffectClass::stage;

        return GetEffectInternal<EffectClass>(stage);
    }

    void Create();
    void Destroy();
    void PerformUpdates();
    void RenderPre(Frame *frame) const;
    void RenderPost(Frame *frame) const;

private:
    PostProcessingUniforms GetUniforms() const;
    void CreateUniformBuffer();

    template <class EffectClass>
    void AddEffectInternal(PostProcessingEffect::Stage stage, UniquePtr<EffectClass> &&effect)
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        std::lock_guard guard(m_effects_mutex);

        const auto it = m_effects_pending_addition[UInt(stage)].Find<EffectClass>();

        if (it != m_effects_pending_addition[UInt(stage)].End()) {
            it->second = std::move(effect);
        } else {
            m_effects_pending_addition[UInt(stage)].Set<EffectClass>(std::move(effect));
        }
        
        m_effects_updated.store(true);
    }

    template <class EffectClass>
    EffectClass *GetEffectInternal(PostProcessingEffect::Stage stage) const
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        Threads::AssertOnThread(THREAD_RENDER);

        auto &effects = m_effects[UInt(stage)];

        auto it = effects.Find<EffectClass>();

        if (it == effects.End()) {
            return nullptr;
        }

        return static_cast<EffectClass *>(it->second.Get());
    }

    FixedArray<TypeMap<UniquePtr<PostProcessingEffect>>, 2> m_effects; // only touch from render thread
    FixedArray<TypeMap<UniquePtr<PostProcessingEffect>>, 2> m_effects_pending_addition;
    FixedArray<FlatSet<TypeID>, 2> m_effects_pending_removal;
    std::mutex m_effects_mutex;
    std::atomic_bool m_effects_updated { false };

    GPUBufferRef m_uniform_buffer;
};

} // namespace hyperion::v2

#endif // HYPERION_V2_POST_FX_H

