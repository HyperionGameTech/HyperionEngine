/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/TypeMap.hpp>
#include <core/threading/Threads.hpp>

#include <rendering/FullScreenPass.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <core/Types.hpp>

#include <climits>

namespace hyperion {

class GBuffer;

struct alignas(16) PostProcessingUniforms
{
    Vec2u effectCounts;       // pre, post
    Vec2u lastEnabledIndices; // pre, post
    Vec2u masks;              // pre, post
};

static_assert(sizeof(PostProcessingUniforms) == 32);

enum PostProcessingStage
{
    POST_PROCESSING_STAGE_PRE_SHADING,
    POST_PROCESSING_STAGE_POST_SHADING
};

HYP_CLASS(NoScriptBindings)
class HYP_API PostFXPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(PostFXPass);

public:
    PostFXPass(
        TextureFormat imageFormat,
        GBuffer* gbuffer);

    PostFXPass(
        const ShaderRef& shader,
        TextureFormat imageFormat,
        GBuffer* gbuffer);

    PostFXPass(
        const ShaderRef& shader,
        PostProcessingStage stage,
        uint32 effectIndex,
        TextureFormat imageFormat,
        GBuffer* gbuffer);

    PostFXPass(const PostFXPass&) = delete;
    PostFXPass& operator=(const PostFXPass&) = delete;

    virtual ~PostFXPass() override;

    virtual void CreateDescriptors() override;

    HYP_FORCE_INLINE PostProcessingStage GetStage() const
    {
        return m_stage;
    }

    HYP_FORCE_INLINE uint32 GetEffectIndex() const
    {
        return m_effectIndex;
    }

protected:
    PostProcessingStage m_stage;
    uint32 m_effectIndex;
};

class HYP_API PostProcessingEffect
{
public:
    PostProcessingEffect(
        PostProcessingStage stage,
        uint32 effectIndex,
        TextureFormat imageFormat,
        GBuffer* gbuffer);
    PostProcessingEffect(const PostProcessingEffect& other) = delete;
    PostProcessingEffect& operator=(const PostProcessingEffect& other) = delete;
    virtual ~PostProcessingEffect();

    PostFXPass& GetPass()
    {
        return m_pass;
    }

    const PostFXPass& GetPass() const
    {
        return m_pass;
    }

    const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    PostProcessingStage GetStage() const
    {
        return m_pass.GetStage();
    }

    uint32 GetEffectIndex() const
    {
        return m_pass.GetEffectIndex();
    }

    bool IsEnabled() const
    {
        return m_isEnabled;
    }

    void SetIsEnabled(bool isEnabled)
    {
        m_isEnabled = isEnabled;
    }

    void Init();

    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;

    virtual void RenderEffect(FrameBase* frame, const RenderSetup& renderSetup, uint32 slot);

protected:
    virtual ShaderRef CreateShader() = 0;

    PostFXPass m_pass;

private:
    ShaderRef m_shader;
    bool m_isEnabled;
};

class HYP_API PostProcessing
{
public:
    static constexpr uint32 maxEffectsPerStage = sizeof(uint32) * CHAR_BIT;

    enum DefaultEffectIndices
    {
        DEFAULT_EFFECT_INDEX_SSAO,
        DEFAULT_EFFECT_INDEX_FXAA
    };

    PostProcessing();
    PostProcessing(const PostProcessing&) = delete;
    PostProcessing& operator=(const PostProcessing&) = delete;
    ~PostProcessing();

    HYP_FORCE_INLINE const GpuBufferRef& GetUniformBuffer() const
    {
        return m_uniformBuffer;
    }

    /*! \brief Add an effect to the stack to be processed BEFORE deferred rendering happens.
     * Note, cannot add new filters after pipeline construction, currently
     * @param effect A UniquePtr to the class, derived from PostProcessingEffect.
     */
    template <class EffectClass>
    void AddEffect(UniquePtr<EffectClass>&& effect)
    {
        const PostProcessingStage stage = EffectClass::stage;

        AddEffectInternal(stage, std::move(effect));
    }

    /*! \brief Add an effect to the stack to be processed BEFORE deferred rendering happens, constructed by the given arguments.
     * Note, cannot add new filters after pipeline construction, currently
     */
    template <class EffectClass, class... Args>
    void AddEffect(Args&&... args)
    {
        const PostProcessingStage stage = EffectClass::stage;

        AddEffectInternal(stage, MakeUnique<EffectClass>(std::forward<Args>(args)...));
    }

    /*! \brief Get an effect added to the list of effects to be applied BEFORE deferred rendering happens */
    template <class EffectClass>
    EffectClass* GetPass() const
    {
        const PostProcessingStage stage = EffectClass::stage;

        return GetEffectInternal<EffectClass>(stage);
    }

    void Create();
    void Destroy();
    void PerformUpdates();
    void RenderPre(FrameBase* frame, const RenderSetup& renderSetup) const;
    void RenderPost(FrameBase* frame, const RenderSetup& renderSetup) const;

private:
    PostProcessingUniforms GetUniforms() const;
    void CreateUniformBuffer();

    template <class EffectClass>
    void AddEffectInternal(PostProcessingStage stage, UniquePtr<EffectClass>&& effect)
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        std::lock_guard guard(m_effectsMutex);

        const auto it = m_effectsPendingAddition[uint32(stage)].Find<EffectClass>();

        if (it != m_effectsPendingAddition[uint32(stage)].End())
        {
            it->second = std::move(effect);
        }
        else
        {
            m_effectsPendingAddition[uint32(stage)].Set<EffectClass>(std::move(effect));
        }

        m_effectsUpdated.Set(true, MemoryOrder::RELEASE);
    }

    template <class EffectClass>
    EffectClass* GetEffectInternal(PostProcessingStage stage) const
    {
        static_assert(std::is_base_of_v<PostProcessingEffect, EffectClass>, "Type must be a derived class of PostProcessingEffect.");

        AssertOnThread(g_renderThread);

        auto& effects = m_effects[uint32(stage)];

        auto it = effects.Find<EffectClass>();

        if (it == effects.End())
        {
            return nullptr;
        }

        return static_cast<EffectClass*>(it->second.Get());
    }

    FixedArray<TypeMap<UniquePtr<PostProcessingEffect>>, 2> m_effects; // only touch from render thread
    FixedArray<TypeMap<UniquePtr<PostProcessingEffect>>, 2> m_effectsPendingAddition;
    FixedArray<FlatSet<TypeId>, 2> m_effectsPendingRemoval;
    std::mutex m_effectsMutex;
    AtomicVar<bool> m_effectsUpdated { false };

    GpuBufferRef m_uniformBuffer;
};

} // namespace hyperion
