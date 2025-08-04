/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>

#include <core/utilities/Optional.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/utilities/Range.hpp>

#include <core/Defines.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

// #define HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE

class RenderResourceBase;

class IRenderProxy;
class HypObjectBase;

uint32 RenderApi_RetrieveResourceBinding(const HypObjectBase* resource);

template <class T>
struct ShaderDataOffset
{
    static_assert(isPodType<T>, "T must be POD to use with ShaderDataOffset");

    static constexpr uint32 invalidIndex = ~0u;

    explicit ShaderDataOffset(uint32 index)
        : index(index)
    {
    }

    explicit ShaderDataOffset(const HypObjectBase* resource, uint32 indexIfNull = invalidIndex)
        : index(indexIfNull)
    {
        if (uint32 idx = RenderApi_RetrieveResourceBinding(resource); idx != ~0u)
        {
            index = idx;
        }
    }

    HYP_FORCE_INLINE operator uint32() const
    {
        AssertDebug(index != invalidIndex, "Index was ~0u when converting to uint32 for ShaderDataOffset<{}>", TypeName<T>().Data());

        return uint32(sizeof(T) * index);
    }

    uint32 index;
};

struct DescriptorSetDeclaration;
struct DescriptorTableDeclaration;

enum class DescriptorSetElementType : uint32
{
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    SSBO,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    IMAGE_STORAGE,
    SAMPLER,
    TLAS,
    MAX
};

constexpr uint32 descriptorSetElementTypeToBufferType[uint32(DescriptorSetElementType::MAX)] = {
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
struct DescriptorSetElementTypeInfo<GpuBufferBase>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER))
        | (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC))
        | (1u << uint32(DescriptorSetElementType::SSBO))
        | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC));
};

template <>
struct DescriptorSetElementTypeInfo<ImageViewBase>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::IMAGE))
        | (1u << uint32(DescriptorSetElementType::IMAGE_STORAGE));
};

template <>
struct DescriptorSetElementTypeInfo<SamplerBase>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::SAMPLER));
};

template <>
struct DescriptorSetElementTypeInfo<TLASBase>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::TLAS));
};

struct DescriptorSetLayoutElement
{
    DescriptorSetElementType type = DescriptorSetElementType::UNSET;
    uint32 binding = ~0u; // has to be set
    uint32 count = 1;     // Set to -1 for bindless
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

class DescriptorSetLayout
{
public:
    DescriptorSetLayout(const DescriptorSetDeclaration* decl);

    DescriptorSetLayout(const DescriptorSetLayout& other)
        : name(other.name),
          decl(other.decl),
          isTemplate(other.isTemplate),
          isReference(other.isReference),
          elements(other.elements),
          dynamicElements(other.dynamicElements)
    {
    }

    DescriptorSetLayout& operator=(const DescriptorSetLayout& other)
    {
        if (this == &other)
        {
            return *this;
        }

        name = other.name;
        decl = other.decl;
        isTemplate = other.isTemplate;
        isReference = other.isReference;
        elements = other.elements;
        dynamicElements = other.dynamicElements;

        return *this;
    }

    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
        : name(other.name),
          decl(other.decl),
          isTemplate(other.isTemplate),
          isReference(other.isReference),
          elements(std::move(other.elements)),
          dynamicElements(std::move(other.dynamicElements))
    {
        other.name = Name::Invalid();
        other.decl = nullptr;
        other.isTemplate = false;
        other.isReference = false;
    }

    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        name = other.name;
        decl = other.decl;
        isTemplate = other.isTemplate;
        isReference = other.isReference;
        elements = std::move(other.elements);
        dynamicElements = std::move(other.dynamicElements);

        other.name = Name::Invalid();
        other.decl = nullptr;
        other.isTemplate = false;
        other.isReference = false;

        return *this;
    }

    ~DescriptorSetLayout() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return decl != nullptr;
    }

    HYP_FORCE_INLINE const DescriptorSetDeclaration* GetDeclaration() const
    {
        return decl;
    }

    HYP_FORCE_INLINE bool IsTemplate() const
    {
        return isTemplate;
    }

    HYP_FORCE_INLINE void SetIsTemplate(bool isTemplate)
    {
        this->isTemplate = isTemplate;
    }

    HYP_FORCE_INLINE bool IsReference() const
    {
        return isReference;
    }

    HYP_FORCE_INLINE void SetIsReference(bool isReference)
    {
        this->isReference = isReference;
    }

    HYP_FORCE_INLINE const HashMap<Name, DescriptorSetLayoutElement>& GetElements() const
    {
        return elements;
    }

    HYP_FORCE_INLINE void AddElement(Name name, DescriptorSetElementType type, uint32 binding, uint32 count, uint32 size = ~0u)
    {
        elements.Insert(name, DescriptorSetLayoutElement { type, binding, count, size });
    }

    HYP_FORCE_INLINE const DescriptorSetLayoutElement* GetElement(Name name) const
    {
        const auto it = elements.Find(name);

        if (it == elements.End())
        {
            return nullptr;
        }

        return &it->second;
    }

    HYP_FORCE_INLINE const Array<Name>& GetDynamicElements() const
    {
        return dynamicElements;
    }

    HashCode GetHashCode() const;

    Name name;
    const DescriptorSetDeclaration* decl;
    bool isTemplate : 1 = false;  // is this descriptor set a template for other sets? (e.g material textures)
    bool isReference : 1 = false; // is this descriptor set a reference to a global set? (e.g global material textures)
    HashMap<Name, DescriptorSetLayoutElement> elements;
    Array<Name> dynamicElements;
};

struct DescriptorSetElement
{
    using ValueType = Variant<GpuBufferRef, ImageViewRef, SamplerRef, TLASRef>;

    FlatMap<uint32, ValueType> values;
    Range<uint32> dirtyRange {};

    ~DescriptorSetElement();

    HYP_FORCE_INLINE bool IsDirty() const
    {
        return bool(dirtyRange);
    }
};

class DescriptorSetBase : public RenderObject<DescriptorSetBase>
{
public:
    virtual ~DescriptorSetBase() override;

    HYP_FORCE_INLINE const DescriptorSetLayout& GetLayout() const
    {
        return m_layout;
    }

    HYP_FORCE_INLINE const HashMap<Name, DescriptorSetElement>& GetElements() const
    {
        return m_elements;
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
    virtual RendererResult Destroy() = 0;
    virtual void UpdateDirtyState(bool* outIsDirty = nullptr) = 0;
    virtual void Update(bool force = false) = 0;
    virtual DescriptorSetRef Clone() const = 0;

    bool HasElement(Name name) const;

    void SetElement(Name name, uint32 index, uint32 bufferSize, const GpuBufferRef& ref);
    void SetElement(Name name, uint32 index, const GpuBufferRef& ref);
    void SetElement(Name name, const GpuBufferRef& ref);

    void SetElement(Name name, uint32 index, const ImageViewRef& ref);
    void SetElement(Name name, const ImageViewRef& ref);

    void SetElement(Name name, uint32 index, const SamplerRef& ref);
    void SetElement(Name name, const SamplerRef& ref);

    void SetElement(Name name, uint32 index, const TLASRef& ref);
    void SetElement(Name name, const TLASRef& ref);

    virtual void Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex) const = 0;

protected:
    DescriptorSetBase(const DescriptorSetLayout& layout)
        : m_layout(layout)
    {
    }

    template <class T>
    DescriptorSetElement& SetElement(Name name, uint32 index, const T& ref)
    {
        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
        AssertDebug(layoutElement != nullptr, "Invalid element: No item with name {} found", name);

        // Type check
        static const uint32 mask = DescriptorSetElementTypeInfo<typename T::Type>::mask;
        AssertDebug(mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", name);

        // Range check
        AssertDebug(index < layoutElement->count, "Index {} out of range for element {} with count {}",
            index, name, layoutElement->count);

        // Buffer type check, to make sure the buffer type is allowed for the given element
        if constexpr (std::is_same_v<typename T::Type, GpuBufferBase>)
        {
            if (ref != nullptr)
            {
                const GpuBufferType bufferType = ref->GetBufferType();

                AssertDebug(
                    (descriptorSetElementTypeToBufferType[uint32(layoutElement->type)] & (1u << uint32(bufferType))),
                    "Buffer type {} is not in the allowed types for element {}",
                    uint32(bufferType), name);

                if (layoutElement->size != 0 && layoutElement->size != ~0u)
                {
                    const uint32 remainder = ref->Size() % layoutElement->size;

                    AssertDebug(
                        remainder == 0,
                        "Buffer size ({}) is not a multiplier of layout size ({}) for element {}",
                        ref->Size(), layoutElement->size, name);
                }
            }
        }

        auto it = m_elements.Find(name);

        if (it == m_elements.End())
        {
            it = m_elements.Emplace(name).first;
        }

        DescriptorSetElement& element = it->second;

        auto elementIt = element.values.Find(index);

        if (elementIt == element.values.End())
        {
            elementIt = element.values.Emplace(index, NormalizedType<T>(ref)).first;
        }
        else
        {
            T* currentValue = elementIt->second.template TryGet<T>();

            if (currentValue)
            {
                SafeRelease(std::move(*currentValue));
            }

            elementIt->second.template Set<NormalizedType<T>>(ref);
        }

        // Mark the range as dirty so that it will be updated in the next update
        element.dirtyRange |= { index, index + 1 };

        return element;
    }

    template <class T>
    void PrefillElements(Name name, uint32 count, const Optional<T>& placeholderValue = {})
    {
        bool isBindless = false;

        if (count == ~0u)
        {
            count = g_maxBindlessResources;
            isBindless = true;
        }

        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
        AssertDebug(layoutElement != nullptr, "Invalid element: No item with name {} found", name);

        if (isBindless)
        {
            AssertDebug(layoutElement->IsBindless(), "-1 given as count to prefill elements, yet {} is not specified as bindless in layout", name);
        }

        auto it = m_elements.Find(name);

        if (it == m_elements.End())
        {
            it = m_elements.Emplace(name).first;
        }

        DescriptorSetElement& element = it->second;

        // // Set bufferSize, only used in the case of buffer elements
        // element.bufferSize = layoutElement->size;

        element.values.Clear();
        element.values.Reserve(count);

        for (uint32 i = 0; i < count; i++)
        {
            if (placeholderValue.HasValue())
            {
                element.values.Set(i, placeholderValue.Get());
            }
            else
            {
                element.values.Set(i, T {});
            }
        }

        element.dirtyRange = { 0, count };
    }

    DescriptorSetLayout m_layout;
    HashMap<Name, DescriptorSetElement> m_elements;

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    HashSet<FrameWeakRef> m_currentFrames; // frames that are currently using this descriptor set
#endif
};

class DescriptorTableBase : public RenderObject<DescriptorTableBase>
{
public:
    virtual ~DescriptorTableBase() override = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_decl != nullptr;
    }

    HYP_FORCE_INLINE const DescriptorTableDeclaration* GetDeclaration() const
    {
        return m_decl;
    }

    HYP_FORCE_INLINE const FixedArray<Array<DescriptorSetRef>, g_framesInFlight>& GetSets() const
    {
        return m_sets;
    }

    /*! \brief Get a descriptor set from the table
        \param name The name of the descriptor set
        \param frameIndex The index of the frame for the descriptor set
        \return The descriptor set, or an unset reference if not found */
    const DescriptorSetRef& GetDescriptorSet(Name name, uint32 frameIndex) const;
    const DescriptorSetRef& GetDescriptorSet(uint32 descriptorSetIndex, uint32 frameIndex) const;

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    uint32 GetDescriptorSetIndex(Name name) const;

    /*! \brief Create all descriptor sets in the table
        \param device The device to create the descriptor sets on
        \return The result of the operation */
    RendererResult Create();

    /*! \brief Safely release all descriptor sets in the table
        \param device The device to destroy the descriptor sets on
        \return The result of the operation */
    RendererResult Destroy();

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
    void Bind(CommandBufferBase* commandBuffer, uint32 frameIndex, const PipelineRef& pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets) const
    {
        for (const DescriptorSetRef& set : m_sets[frameIndex])
        {
            if (!set->GetLayout().IsValid())
            {
                continue;
            }

            if (set->GetLayout().IsTemplate())
            {
                continue;
            }

            const Name descriptorSetName = set->GetLayout().name;

            const uint32 setIndex = GetDescriptorSetIndex(descriptorSetName);

            if (set->GetLayout().GetDynamicElements().Any() && offsets.Any())
            {
                const auto offsetsIt = offsets.Find(descriptorSetName);
                
                if (offsetsIt != offsets.End())
                {
                    set->Bind(commandBuffer, pipeline, offsetsIt->second, setIndex);
                    
                    continue;
                }
            }

            set->Bind(commandBuffer, pipeline, setIndex);
        }
    }

protected:
    DescriptorTableBase(const DescriptorTableDeclaration* decl)
        : m_decl(decl)
    {
    }

    const DescriptorTableDeclaration* m_decl;
    FixedArray<Array<DescriptorSetRef>, g_framesInFlight> m_sets;
};

} // namespace hyperion

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
