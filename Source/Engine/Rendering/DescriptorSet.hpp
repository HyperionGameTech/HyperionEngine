/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Optional.hpp>

#include <Core/Memory/RefCountedPtr.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/ArrayMap.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/Utilities/Range.hpp>

#include <Core/Defines.hpp>

#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderCompiler.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion {

// #define DECLARE_SET_TRACK_FRAME_USAGE

class RenderResourceBase;

enum class GpuBufferType : uint8;

class ObjectBase;

template <class T>
struct DescriptorSetElementTypeInfo;

template <>
struct DescriptorSetElementTypeInfo<GpuBuffer>
{
    static constexpr uint32 mask = (1u << uint32(ShaderInputType::CBV))
        | (1u << uint32(ShaderInputType::CBV_Dynamic))
        | (1u << uint32(ShaderInputType::SRV))
        | (1u << uint32(ShaderInputType::SRV_Dynamic))
        | (1u << uint32(ShaderInputType::UAV))
        | (1u << uint32(ShaderInputType::UAV_Dynamic));
};

template <>
struct DescriptorSetElementTypeInfo<GpuImageView>
{
    static constexpr uint32 mask = (1u << uint32(ShaderInputType::SRV))
        | (1u << uint32(ShaderInputType::UAV));
};

template <>
struct DescriptorSetElementTypeInfo<Sampler>
{
    static constexpr uint32 mask = (1u << uint32(ShaderInputType::Sampler));
};

template <>
struct DescriptorSetElementTypeInfo<GpuTlas>
{
    static constexpr uint32 mask = (1u << uint32(ShaderInputType::SRV));
};

extern ShaderInputGroup& GetStaticDescriptorTableDeclaration();

struct ShaderInputWithBinding : ShaderInput
{
    uint32 binding = ~0u;
};

class DescriptorSetLayout
{
public:
    using InputMap = THashTable<ShaderInputWithBinding, &ShaderInputWithBinding::name, RHIAllocator>;

    explicit DescriptorSetLayout(const ShaderInputSet* decl);

    DescriptorSetLayout(const DescriptorSetLayout& other)
        : m_decl(other.m_decl),
          m_isTemplate(other.m_isTemplate),
          m_isReference(other.m_isReference),
          m_elements(other.m_elements),
          m_dynamicElements(other.m_dynamicElements),
          m_cachedHashCode(other.m_cachedHashCode)
    {
    }

    DescriptorSetLayout& operator=(const DescriptorSetLayout& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_decl = other.m_decl;
        m_isTemplate = other.m_isTemplate;
        m_isReference = other.m_isReference;
        m_elements = other.m_elements;
        m_dynamicElements = other.m_dynamicElements;
        m_cachedHashCode = other.m_cachedHashCode;

        return *this;
    }

    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
        : m_decl(other.m_decl),
          m_isTemplate(other.m_isTemplate),
          m_isReference(other.m_isReference),
          m_elements(std::move(other.m_elements)),
          m_dynamicElements(std::move(other.m_dynamicElements)),
          m_cachedHashCode(other.m_cachedHashCode)
    {
        other.m_decl = nullptr;
        other.m_isTemplate = false;
        other.m_isReference = false;
        other.m_cachedHashCode = HashCode();
    }

    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_decl = other.m_decl;
        m_isTemplate = other.m_isTemplate;
        m_isReference = other.m_isReference;
        m_elements = std::move(other.m_elements);
        m_dynamicElements = std::move(other.m_dynamicElements);
        m_cachedHashCode = other.m_cachedHashCode;

        other.m_decl = nullptr;
        other.m_isTemplate = false;
        other.m_isReference = false;
        other.m_cachedHashCode = HashCode();

        return *this;
    }

    ~DescriptorSetLayout() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_decl != nullptr;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_decl ? m_decl->name : Name::Invalid();
    }

    HYP_FORCE_INLINE const ShaderInputSet* GetDeclaration() const
    {
        return m_decl;
    }

    HYP_FORCE_INLINE bool IsTemplate() const
    {
        return m_isTemplate;
    }

    HYP_FORCE_INLINE void SetIsTemplate(bool isTemplate)
    {
        m_isTemplate = isTemplate;
    }

    HYP_FORCE_INLINE bool IsReference() const
    {
        return m_isReference;
    }

    HYP_FORCE_INLINE void SetIsReference(bool isReference)
    {
        m_isReference = isReference;
    }

    HYP_FORCE_INLINE const InputMap& GetElements() const
    {
        return m_elements;
    }

    HYP_FORCE_INLINE ShaderInputWithBinding& AddElement(const ShaderInput& input)
    {
        ShaderInputWithBinding element = { input };
        element.binding = ~0u;
        return *m_elements.Set(element).first;
    }

    HYP_FORCE_INLINE const ShaderInputWithBinding* GetElement(StringHash name) const
    {
        auto it = m_elements.Find(Name(name));
        if (it != m_elements.End())
        {
            return &*it;
        }

        return nullptr;
    }

    HYP_FORCE_INLINE const Array<Name>& GetDynamicElements() const
    {
        return m_dynamicElements;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return m_cachedHashCode;
    }

private:
    const ShaderInputSet* m_decl;
    bool m_isTemplate : 1 = false;  // is this descriptor set a template for other sets? (e.g material textures)
    bool m_isReference : 1 = false; // is this descriptor set a reference to a global set? (e.g global material textures)
    InputMap m_elements;
    Array<Name> m_dynamicElements;
    HashCode m_cachedHashCode;
};

struct DescriptorSetElement
{
    Range<uint32> dirtyRange;
    Array<ObjectBase*, RHIAllocator> values;
    Bitset occupiedArrayElems;
    uint32 bufferStride = ~0u;

    HYP_FORCE_INLINE bool IsDirty() const
    {
        return bool(dirtyRange);
    }
};

HYP_CLASS(Abstract, NoScriptBindings)
class DescriptorSetBase : public ObjectBase
{
    HYP_OBJECT_BODY(DescriptorSetBase);

public:
    using ElementsMap = TMap<Name, DescriptorSetElement, RHIAllocator>;

    virtual ~DescriptorSetBase() override;

    static Pool* GetAllocator()
    {
        return g_rhiPool;
    }

    HYP_FORCE_INLINE const DescriptorSetLayout& GetLayout() const
    {
        return m_layout;
    }

#ifdef HYP_RHI_DEBUG_NAMES
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif // HYP_RHI_DEBUG_NAMES

#ifdef DECLARE_SET_TRACK_FRAME_USAGE
    HYP_FORCE_INLINE TSet<FrameWeakRef>& GetCurrentFrames()
    {
        return m_currentFrames;
    }

    HYP_FORCE_INLINE const TSet<FrameWeakRef>& GetCurrentFrames() const
    {
        return m_currentFrames;
    }
#endif // DECLARE_SET_TRACK_FRAME_USAGE

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

    virtual void UpdateDirtyState(bool* outIsDirty = nullptr) = 0;
    virtual void Update(bool force = false) = 0;
    virtual DescriptorSetRef Clone() const = 0;

    bool HasElement(StringHash name) const;

    void SetElement(StringHash name, uint32 index, uint32 bufferSize, GpuBuffer* ref, uint32 bufferStride = ~0u);
    void SetElement(StringHash name, uint32 index, GpuBuffer* ref, uint32 bufferStride = ~0u);
    void SetElement(StringHash name, GpuBuffer* ref, uint32 bufferStride = ~0u);

    void SetElement(StringHash name, uint32 index, GpuImageView* ref);
    void SetElement(StringHash name, GpuImageView* ref);

    void SetElement(StringHash name, uint32 index, Sampler* ref);
    void SetElement(StringHash name, Sampler* ref);

    void SetElement(StringHash name, uint32 index, GpuTlas* ref);
    void SetElement(StringHash name, GpuTlas* ref);

    /*! \brief Only for bindless descriptors; Marks the element at \p index as invalid */
    void DeleteElement(StringHash name, uint32 index);

    virtual void Bind(CommandBuffer* commandBuffer, const GraphicsPipeline* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBuffer* commandBuffer, const GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const = 0;

    virtual void Bind(CommandBuffer* commandBuffer, const ComputePipeline* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBuffer* commandBuffer, const ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const = 0;

    virtual void Bind(CommandBuffer* commandBuffer, const RayTracingPipeline* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBuffer* commandBuffer, const RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const = 0;

    uint32 frameCounter; // last used

protected:
    DescriptorSetBase(const DescriptorSetLayout& layout)
        : frameCounter(0),
          m_layout(layout)
    {
    }

    template <class T>
    DescriptorSetElement& SetElementT(StringHash name, uint32 index, T* ref, uint32 bufferStride = ~0u);

    template <class T>
    void PrefillElements(Name name, uint32 count, T* placeholder = nullptr)
    {
        bool isBindless = false;

        if (count == ~0u)
        {
            isBindless = true;
        }

        const ShaderInput* shaderInput = m_layout.GetElement(name);
        AssertDebug(shaderInput != nullptr, "Invalid element: No item with name {} found", name);

        if (isBindless)
        {
            AssertDebug(shaderInput->count == ~0u, "-1 given as count to prefill elements, yet {} is not specified as bindless in layout", name);
        }

        auto it = m_elements.FindAs(name);

        if (it == m_elements.End())
        {
            it = m_elements.Emplace(name).first;
        }

        // if we are a bindless descriptor then we want to NOT have occupiedArrayElems set.
        if (isBindless)
        {
            return;
        }

        DescriptorSetElement& element = it->second;
        element.values.Resize(count);

        for (uint32 i = 0; i < count; i++)
        {
            element.values[i] = placeholder;
            element.occupiedArrayElems.Set(i, true);
        }

        element.dirtyRange = { 0, count };
    }

    DescriptorSetLayout m_layout;
    ElementsMap m_elements;

#ifdef HYP_RHI_DEBUG_NAMES
    Name m_debugName;
#endif

#ifdef DECLARE_SET_TRACK_FRAME_USAGE
    TSet<FrameWeakRef> m_currentFrames; // frames that are currently using this descriptor set
#endif
};

HYP_CLASS(Abstract, NoScriptBindings)
class DescriptorTableBase : public ObjectBase
{
    HYP_OBJECT_BODY(DescriptorTableBase);

public:
    explicit DescriptorTableBase(const ShaderInputGroup* decl);

    virtual ~DescriptorTableBase() override
    {
        for (auto& it : m_sets)
        {
            EnqueueDeletion(std::move(it));
        }
    }

#ifdef HYP_RHI_DEBUG_NAMES
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_decl != nullptr;
    }

    HYP_FORCE_INLINE const ShaderInputGroup* GetDeclaration() const
    {
        return m_decl;
    }

    HYP_FORCE_INLINE const FixedArray<Array<DescriptorSetRef>, NumFramesInFlight>& GetSets() const
    {
        return m_sets;
    }

    /*! \brief Get a descriptor set from the table
        \param name The name of the descriptor set
        \param frameIndex The index of the frame for the descriptor set
        \return The descriptor set, or an unset reference if not found */
    HYP_FORCE_INLINE const DescriptorSetRef& GetDescriptorSet(StringHash name, uint32 frameIndex) const
    {
        for (const DescriptorSetRef& set : m_sets[frameIndex])
        {
            DescriptorSetBase* setBase = static_cast<DescriptorSetBase*>(set.ptr);

            if (setBase->GetLayout().GetName() == name)
            {
                return set;
            }
        }

        return DescriptorSetRef::empty;
    }

    HYP_FORCE_INLINE const DescriptorSetRef& GetDescriptorSet(uint32 descriptorSetIndex, uint32 frameIndex) const
    {
        for (const DescriptorSetRef& set : m_sets[frameIndex])
        {
            DescriptorSetBase* setBase = static_cast<DescriptorSetBase*>(set.ptr);

            if (!setBase->GetLayout().IsValid())
            {
                continue;
            }

            if (setBase->GetLayout().GetDeclaration()->setIndex == descriptorSetIndex)
            {
                return set;
            }
        }

        return DescriptorSetRef::empty;
    }

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE uint32 GetDescriptorSetIndex(StringHash name) const
    {
        return m_decl ? m_decl->GetDescriptorSetIndex(name) : ~0u;
    }

    /*! \brief Create all descriptor sets in the table
        \param device The device to create the descriptor sets on
        \return The result of the operation */
    RendererResult Create();

    /*! \brief Apply updates to all descriptor sets in the table
        \param frameIndex The index of the frame to update the descriptor sets for
        \param force If true, will update descriptor sets even if they are not marked as dirty
        \return The result of the operation */
    void Update(uint32 frameIndex, bool force = false);

protected:
    const ShaderInputGroup* m_decl;
    FixedArray<Array<DescriptorSetRef>, NumFramesInFlight> m_sets;

#ifdef HYP_RHI_DEBUG_NAMES
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanDescriptorSet.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12DescriptorSet.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
