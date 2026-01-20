/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>

#include <core/utilities/Optional.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/reflection/ObjectFwd.hpp>

#include <core/utilities/Range.hpp>

#include <core/Defines.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <rendering/util/SafeDeleter.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <core/Types.hpp>
#include <core/HashCode.hpp>

namespace Hyperion {

// #define HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE

class RenderResourceBase;

enum class GpuBufferType : uint8;

class IRenderProxy;
class ObjectBase;

constexpr uint32 ElementTypeToBufferType[uint32(DescriptorSetElementType::MAX)] = {
    0,                                    // UNSET
    (1u << uint32(GpuBufferType::CBUFF)), // UNIFORM_BUFFER
    (1u << uint32(GpuBufferType::CBUFF)), // UNIFORM_BUFFER_DYNAMIC
    (1u << uint32(GpuBufferType::SSBO))
        | (1u << uint32(GpuBufferType::ATOMIC_COUNTER))
        | (1u << uint32(GpuBufferType::STAGING_BUFFER))
        | (1u << uint32(GpuBufferType::INDIRECT_ARGS_BUFFER)), // SSBO

    (1u << uint32(GpuBufferType::SSBO))
        | (1u << uint32(GpuBufferType::ATOMIC_COUNTER))
        | (1u << uint32(GpuBufferType::STAGING_BUFFER))
        | (1u << uint32(GpuBufferType::INDIRECT_ARGS_BUFFER)),   // STORAGE_BUFFER_DYNAMIC
    0,                                                           // IMAGE
    0,                                                           // IMAGE_STORAGE
    0,                                                           // SAMPLER
    (1u << uint32(GpuBufferType::ACCELERATION_STRUCTURE_BUFFER)) // ACCELERATION_STRUCTURE
};

template <class T>
struct DescriptorSetElementTypeInfo;

template <>
struct DescriptorSetElementTypeInfo<GpuBuffer>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER))
        | (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC))
        | (1u << uint32(DescriptorSetElementType::SSBO))
        | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC));
};

template <>
struct DescriptorSetElementTypeInfo<GpuImageView>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::IMAGE))
        | (1u << uint32(DescriptorSetElementType::IMAGE_STORAGE));
};

template <>
struct DescriptorSetElementTypeInfo<Sampler>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::SAMPLER));
};

template <>
struct DescriptorSetElementTypeInfo<GpuTlas>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::TLAS));
};

HYP_STRUCT()
struct DescriptorSetLayoutElement
{
    HYP_STRUCT_BODY(DescriptorSetLayoutElement);

    HYP_FIELD()
    DescriptorSetElementType type = DescriptorSetElementType::UNSET;

    HYP_FIELD()
    uint32 binding = ~0u; // has to be set

    HYP_FIELD()
    uint32 count = 1; // Set to -1 for bindless

    HYP_FIELD()
    uint32 size = ~0u;

    HYP_FORCE_INLINE bool IsBindless() const
    {
        return count == uint32(-1);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(type);
        hc.Add(binding);
        hc.Add(count);
        hc.Add(size);

        return hc;
    }
};

extern DescriptorTableDeclaration& GetStaticDescriptorTableDeclaration();

class DescriptorSetLayout
{
public:
    DescriptorSetLayout(const DescriptorSetDeclaration* decl);

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

    HYP_FORCE_INLINE const DescriptorSetDeclaration* GetDeclaration() const
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

    HYP_FORCE_INLINE const HashMap<Name, DescriptorSetLayoutElement>& GetElements() const
    {
        return m_elements;
    }

    HYP_FORCE_INLINE void AddElement(Name name, DescriptorSetElementType type, uint32 binding, uint32 count, uint32 size = ~0u)
    {
        m_elements.Insert(name, DescriptorSetLayoutElement { type, binding, count, size });
    }

    HYP_FORCE_INLINE const DescriptorSetLayoutElement* GetElement(StringHash name) const
    {
        const auto it = m_elements.FindAs(name);

        if (it == m_elements.End())
        {
            return nullptr;
        }

        return &it->second;
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
    const DescriptorSetDeclaration* m_decl;
    bool m_isTemplate : 1 = false;  // is this descriptor set a template for other sets? (e.g material textures)
    bool m_isReference : 1 = false; // is this descriptor set a reference to a global set? (e.g global material textures)
    HashMap<Name, DescriptorSetLayoutElement> m_elements;
    Array<Name> m_dynamicElements;
    HashCode m_cachedHashCode;
};

struct DescriptorSetElement
{
    Range<uint32> dirtyRange;
    Array<ObjectBase*> values;
    Bitset occupiedArrayElems;

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
    virtual ~DescriptorSetBase() override;

    HYP_FORCE_INLINE const DescriptorSetLayout& GetLayout() const
    {
        return m_layout;
    }

    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    HYP_FORCE_INLINE HashSet<FrameWeakRef>& GetCurrentFrames()
    {
        return m_currentFrames;
    }

    HYP_FORCE_INLINE const HashSet<FrameWeakRef>& GetCurrentFrames() const
    {
        return m_currentFrames;
    }
#endif

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

    virtual void UpdateDirtyState(bool* outIsDirty = nullptr) = 0;
    virtual void Update(bool force = false) = 0;
    virtual DescriptorSetRef Clone() const = 0;

    bool HasElement(StringHash name) const;

    void SetElement(StringHash name, uint32 index, uint32 bufferSize, GpuBuffer* ref);
    void SetElement(StringHash name, uint32 index, GpuBuffer* ref);
    void SetElement(StringHash name, GpuBuffer* ref);

    void SetElement(StringHash name, uint32 index, GpuImageView* ref);
    void SetElement(StringHash name, GpuImageView* ref);

    void SetElement(StringHash name, uint32 index, Sampler* ref);
    void SetElement(StringHash name, Sampler* ref);

    void SetElement(StringHash name, uint32 index, GpuTlas* ref);
    void SetElement(StringHash name, GpuTlas* ref);

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
    DescriptorSetElement& SetElementT(StringHash name, uint32 index, T* ref);

    template <class T>
    void PrefillElements(Name name, uint32 count, T* placeholder = nullptr)
    {
        bool isBindless = false;

        if (count == ~0u)
        {
            count = MaxBindlessResources;
            isBindless = true;
        }

        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
        AssertDebug(layoutElement != nullptr, "Invalid element: No item with name {} found", name);

        if (isBindless)
        {
            AssertDebug(layoutElement->IsBindless(), "-1 given as count to prefill elements, yet {} is not specified as bindless in layout", name);
        }

        auto it = m_elements.FindAs(name);

        if (it == m_elements.End())
        {
            it = m_elements.Emplace(name).first;
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
    HashMap<Name, DescriptorSetElement> m_elements;

    Name m_debugName;

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    HashSet<FrameWeakRef> m_currentFrames; // frames that are currently using this descriptor set
#endif
};

HYP_CLASS(Abstract, NoScriptBindings)
class DescriptorTableBase : public ObjectBase
{
    HYP_OBJECT_BODY(DescriptorTableBase);

public:
    explicit DescriptorTableBase(const DescriptorTableDeclaration* decl);

    virtual ~DescriptorTableBase() override
    {
        for (auto& it : m_sets)
        {
            SafeDelete(std::move(it));
        }
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

    HYP_FORCE_INLINE Name GetDebugName() const
    {
        return m_debugName;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_decl != nullptr;
    }

    HYP_FORCE_INLINE const DescriptorTableDeclaration* GetDeclaration() const
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

    /*! \brief Bind all descriptor sets in the table
        \param commandBuffer The command buffer to push the bind commands to
        \param frameIndex The index of the frame to bind the descriptor sets for
        \param pipeline The pipeline to bind the descriptor sets to
        \param offsets The offsets to bind dynamic descriptor sets with */
    template <class PipelineRef>
    void Bind(CommandBuffer* commandBuffer, uint32 frameIndex, const PipelineRef& pipeline, const DescriptorTableOffsetMap& offsets) const
    {
        for (const DescriptorSetRef& set : m_sets[frameIndex])
        {
            DescriptorSetBase* setBase = static_cast<DescriptorSetBase*>(set.ptr);

            if (!setBase->GetLayout().IsValid() || setBase->GetLayout().IsTemplate())
            {
                continue;
            }

            const Name descriptorSetName = setBase->GetLayout().GetName();

            const uint32 setIndex = GetDescriptorSetIndex(descriptorSetName);

            if (setBase->GetLayout().GetDynamicElements().Any() && offsets.count != 0)
            {
                int offsetIdx = -1;

                for (uint32 i = 0; i < offsets.count; i++)
                {
                    if (offsets.setNames[i] == descriptorSetName)
                    {
                        offsetIdx = i;
                        break;
                    }
                }

                if (offsetIdx != -1)
                {
                    setBase->Bind(commandBuffer, pipeline, offsets.setOffsets[offsetIdx], setIndex);

                    continue;
                }
            }

            setBase->Bind(commandBuffer, pipeline, setIndex);
        }
    }

protected:
    const DescriptorTableDeclaration* m_decl;
    FixedArray<Array<DescriptorSetRef>, NumFramesInFlight> m_sets;

    Name m_debugName;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12DescriptorSet.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif

#define HYP_DESCRIPTOR_SET(index, name) \
    static DescriptorTableDeclaration::DeclareSet HYP_UNIQUE_NAME(DescriptorSet_##name)(&GetStaticDescriptorTableDeclaration(), index, HYP_NAME_UNSAFE(name))

#define HYP_DESCRIPTOR_SRV_COND(setName, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), DESCRIPTOR_SLOT_SRV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_UAV_COND(setName, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), DESCRIPTOR_SLOT_UAV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_CBUFF_COND(setName, name, count, size, isDynamic, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), DESCRIPTOR_SLOT_CBUFF, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, size, isDynamic)
#define HYP_DESCRIPTOR_SSBO_COND(setName, name, count, size, isDynamic, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), DESCRIPTOR_SLOT_SSBO, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, size, isDynamic)
#define HYP_DESCRIPTOR_ACCELERATION_STRUCTURE_COND(setName, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_SAMPLER_COND(setName, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), DESCRIPTOR_SLOT_SAMPLER, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)

#define HYP_DESCRIPTOR_SRV(setName, name, count) HYP_DESCRIPTOR_SRV_COND(setName, name, count, true)
#define HYP_DESCRIPTOR_UAV(setName, name, count) HYP_DESCRIPTOR_UAV_COND(setName, name, count, true)
#define HYP_DESCRIPTOR_CBUFF(setName, name, count, size, isDynamic) HYP_DESCRIPTOR_CBUFF_COND(setName, name, count, size, isDynamic, true)
#define HYP_DESCRIPTOR_SSBO(setName, name, count, size, isDynamic) HYP_DESCRIPTOR_SSBO_COND(setName, name, count, size, isDynamic, true)
#define HYP_DESCRIPTOR_ACCELERATION_STRUCTURE(setName, name, count) HYP_DESCRIPTOR_ACCELERATION_STRUCTURE_COND(setName, name, count, true)
#define HYP_DESCRIPTOR_SAMPLER(setName, name, count) HYP_DESCRIPTOR_SAMPLER_COND(setName, name, count, true)
