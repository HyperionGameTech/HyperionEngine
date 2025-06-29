/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_DESCRIPTOR_SET_HPP
#define HYPERION_BACKEND_RENDERER_DESCRIPTOR_SET_HPP

#include <core/Name.hpp>

#include <core/utilities/Optional.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/utilities/Range.hpp>

#include <core/Defines.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

// #define HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE

class RenderResourceBase;

enum class DescriptorSetDeclarationFlags : uint8
{
    NONE = 0x0,
    REFERENCE = 0x1, // is this a reference to a global descriptor set declaration?
    TEMPLATE = 0x2   // is this descriptor set intended to be used as a template for other sets? (e.g material textures)
};

HYP_MAKE_ENUM_FLAGS(DescriptorSetDeclarationFlags)

class IRenderProxy;

template <class T>
struct ShaderDataOffset
{
    static_assert(is_pod_type<T>, "T must be POD to use with ShaderDataOffset");

    static constexpr uint32 invalid_index = ~0u;

    ShaderDataOffset(uint32 index)
        : index(index)
    {
    }

    template <class RenderResourceType, typename = std::enable_if_t<!std::is_base_of_v<IRenderProxy, NormalizedType<RenderResourceType>>>>
    HYP_DEPRECATED ShaderDataOffset(const RenderResourceType* render_resource, uint32 index_if_null = invalid_index)
        : index(render_resource != nullptr ? render_resource->GetBufferIndex() : index_if_null)
    {
    }

    template <class RenderResourceType, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<RenderResourceType>> && !std::is_integral_v<NormalizedType<RenderResourceType>>>>
    HYP_DEPRECATED ShaderDataOffset(const RenderResourceType& render_resource)
        : index(render_resource.GetBufferIndex())
    {
    }

    template <class RenderProxyType, typename = std::enable_if_t<std::is_base_of_v<IRenderProxy, NormalizedType<RenderProxyType>>>>
    ShaderDataOffset(const RenderProxyType* proxy)
        : index(proxy != nullptr ? proxy->bound_index : ~0u)
    {
    }

    HYP_FORCE_INLINE operator uint32() const
    {
        AssertDebugMsg(index != invalid_index, "Index was ~0u when converting to uint32 for ShaderDataOffset<%s>", TypeName<T>().Data());

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

constexpr uint32 descriptor_set_element_type_to_buffer_type[uint32(DescriptorSetElementType::MAX)] = {
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

enum DescriptorSlot : uint32
{
    DESCRIPTOR_SLOT_NONE,
    DESCRIPTOR_SLOT_SRV,
    DESCRIPTOR_SLOT_UAV,
    DESCRIPTOR_SLOT_CBUFF,
    DESCRIPTOR_SLOT_SSBO,
    DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE,
    DESCRIPTOR_SLOT_SAMPLER,
    DESCRIPTOR_SLOT_MAX
};

HYP_STRUCT()
struct DescriptorDeclaration
{
    using ConditionFunction = bool (*)();

    HYP_FIELD(Property = "Slot", Serialize = true)
    DescriptorSlot slot = DESCRIPTOR_SLOT_NONE;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name;

    HYP_FIELD(Property = "Count", Serialize = true)
    uint32 count = 1;

    HYP_FIELD(Property = "Size", Serialize = true)
    uint32 size = uint32(-1);

    HYP_FIELD(Property = "IsDynamic", Serialize = true)
    bool is_dynamic = false;

    HYP_FIELD(Property = "Index", Transient = true, Serialize = false)
    uint32 index = ~0u;

    ConditionFunction cond = nullptr;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(slot);
        hc.Add(name);
        hc.Add(count);
        hc.Add(size);
        hc.Add(is_dynamic);
        hc.Add(index);

        // cond excluded intentionally

        return hc;
    }
};

HYP_STRUCT()
struct DescriptorSetDeclaration
{
    HYP_FIELD(Property = "SetIndex", Serialize = true)
    uint32 set_index = ~0u;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name = Name::Invalid();

    HYP_FIELD(Property = "Slots", Serialize = true)
    FixedArray<Array<DescriptorDeclaration, DynamicAllocator>, DESCRIPTOR_SLOT_MAX> slots = {};

    HYP_FIELD(Property = "Flags", Serialize = true)
    EnumFlags<DescriptorSetDeclarationFlags> flags = DescriptorSetDeclarationFlags::NONE;

    DescriptorSetDeclaration() = default;

    DescriptorSetDeclaration(uint32 set_index, Name name)
        : set_index(set_index),
          name(name)
    {
    }

    DescriptorSetDeclaration(const DescriptorSetDeclaration& other) = default;
    DescriptorSetDeclaration& operator=(const DescriptorSetDeclaration& other) = default;
    DescriptorSetDeclaration(DescriptorSetDeclaration&& other) noexcept = default;
    DescriptorSetDeclaration& operator=(DescriptorSetDeclaration&& other) noexcept = default;
    ~DescriptorSetDeclaration() = default;

    HYP_FORCE_INLINE void AddDescriptorDeclaration(DescriptorDeclaration decl)
    {
        AssertThrow(decl.slot != DESCRIPTOR_SLOT_NONE && decl.slot < DESCRIPTOR_SLOT_MAX);

        decl.index = uint32(slots[uint32(decl.slot) - 1].Size());
        slots[uint32(decl.slot) - 1].PushBack(std::move(decl));
    }

    /*! \brief Calculate a flat index for a Descriptor that is part of this set.
        Returns -1 if not found */
    uint32 CalculateFlatIndex(DescriptorSlot slot, WeakName name) const;

    DescriptorDeclaration* FindDescriptorDeclaration(WeakName name) const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(set_index);
        hc.Add(name);
        hc.Add(flags);

        for (const auto& slot : slots)
        {
            for (const auto& decl : slot)
            {
                hc.Add(decl.GetHashCode());
            }
        }

        return hc;
    }
};

HYP_STRUCT()
struct DescriptorTableDeclaration
{
    HYP_FIELD(Property = "Elements", Serialize = true)
    Array<DescriptorSetDeclaration> elements;

    DescriptorSetDeclaration* FindDescriptorSetDeclaration(WeakName name) const;
    DescriptorSetDeclaration* AddDescriptorSetDeclaration(DescriptorSetDeclaration&& descriptor_set_declaration);

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE uint32 GetDescriptorSetIndex(WeakName name) const
    {
        for (const auto& it : elements)
        {
            if (it.name == name)
            {
                return it.set_index;
            }
        }

        return ~0u;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (const DescriptorSetDeclaration& decl : elements)
        {
            hc.Add(decl.GetHashCode());
        }

        return hc;
    }

    struct DeclareSet
    {
        DeclareSet(DescriptorTableDeclaration* table, uint32 set_index, Name name, bool is_template = false)
        {
            AssertThrow(table != nullptr);

            if (table->elements.Size() <= set_index)
            {
                table->elements.Resize(set_index + 1);
            }

            DescriptorSetDeclaration& decl = table->elements[set_index];
            decl.set_index = set_index;
            decl.name = name;

            if (is_template)
            {
                decl.flags |= DescriptorSetDeclarationFlags::TEMPLATE;
            }
        }
    };

    struct DeclareDescriptor
    {
        DeclareDescriptor(DescriptorTableDeclaration* table, Name set_name, DescriptorSlot slot_type, Name descriptor_name, DescriptorDeclaration::ConditionFunction cond = nullptr, uint32 count = 1, uint32 size = ~0u, bool is_dynamic = false)
        {
            AssertThrow(table != nullptr);

            uint32 set_index = ~0u;

            for (SizeType i = 0; i < table->elements.Size(); ++i)
            {
                if (table->elements[i].name == set_name)
                {
                    set_index = uint32(i);
                    break;
                }
            }

            AssertThrowMsg(set_index != ~0u, "Descriptor set %s not found", set_name.LookupString());

            DescriptorSetDeclaration& descriptor_set_decl = table->elements[set_index];
            AssertThrow(descriptor_set_decl.set_index == set_index);
            AssertThrow(slot_type > 0 && slot_type < descriptor_set_decl.slots.Size());

            const uint32 slot_type_index = uint32(slot_type) - 1;

            const uint32 slot_index = uint32(descriptor_set_decl.slots[slot_type_index].Size());

            DescriptorDeclaration& descriptor_decl = descriptor_set_decl.slots[slot_type_index].EmplaceBack();
            descriptor_decl.index = slot_index;
            descriptor_decl.slot = slot_type;
            descriptor_decl.name = descriptor_name;
            descriptor_decl.cond = cond;
            descriptor_decl.size = size;
            descriptor_decl.count = count;
            descriptor_decl.is_dynamic = is_dynamic;
        }
    };
};

extern DescriptorTableDeclaration& GetStaticDescriptorTableDeclaration();

class DescriptorSetLayout
{
public:
    DescriptorSetLayout(const DescriptorSetDeclaration* decl);

    DescriptorSetLayout(const DescriptorSetLayout& other)
        : m_decl(other.m_decl),
          m_is_template(other.m_is_template),
          m_is_reference(other.m_is_reference),
          m_elements(other.m_elements),
          m_dynamic_elements(other.m_dynamic_elements)
    {
    }

    DescriptorSetLayout& operator=(const DescriptorSetLayout& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_decl = other.m_decl;
        m_is_template = other.m_is_template;
        m_is_reference = other.m_is_reference;
        m_elements = other.m_elements;
        m_dynamic_elements = other.m_dynamic_elements;

        return *this;
    }

    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
        : m_decl(other.m_decl),
          m_is_template(other.m_is_template),
          m_is_reference(other.m_is_reference),
          m_elements(std::move(other.m_elements)),
          m_dynamic_elements(std::move(other.m_dynamic_elements))
    {
        other.m_decl = nullptr;
        other.m_is_template = false;
        other.m_is_reference = false;
    }

    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_decl = other.m_decl;
        m_is_template = other.m_is_template;
        m_is_reference = other.m_is_reference;
        m_elements = std::move(other.m_elements);
        m_dynamic_elements = std::move(other.m_dynamic_elements);

        other.m_decl = nullptr;
        other.m_is_template = false;
        other.m_is_reference = false;

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
        return m_is_template;
    }

    HYP_FORCE_INLINE void SetIsTemplate(bool is_template)
    {
        m_is_template = is_template;
    }

    HYP_FORCE_INLINE bool IsReference() const
    {
        return m_is_reference;
    }

    HYP_FORCE_INLINE void SetIsReference(bool is_reference)
    {
        m_is_reference = is_reference;
    }

    HYP_FORCE_INLINE const HashMap<Name, DescriptorSetLayoutElement>& GetElements() const
    {
        return m_elements;
    }

    HYP_FORCE_INLINE void AddElement(Name name, DescriptorSetElementType type, uint32 binding, uint32 count, uint32 size = ~0u)
    {
        m_elements.Insert(name, DescriptorSetLayoutElement { type, binding, count, size });
    }

    HYP_FORCE_INLINE const DescriptorSetLayoutElement* GetElement(Name name) const
    {
        const auto it = m_elements.Find(name);

        if (it == m_elements.End())
        {
            return nullptr;
        }

        return &it->second;
    }

    HYP_FORCE_INLINE const Array<Name>& GetDynamicElements() const
    {
        return m_dynamic_elements;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        if (!m_decl)
        {
            return hc; // empty hash
        }

        hc.Add(m_decl->GetHashCode());

        for (const auto& it : m_elements)
        {
            hc.Add(it.first.GetHashCode());
            hc.Add(it.second.GetHashCode());
        }

        return hc;
    }

private:
    const DescriptorSetDeclaration* m_decl;
    bool m_is_template : 1 = false;  // is this descriptor set a template for other sets? (e.g material textures)
    bool m_is_reference : 1 = false; // is this descriptor set a reference to a global set? (e.g global material textures)
    HashMap<Name, DescriptorSetLayoutElement> m_elements;
    Array<Name> m_dynamic_elements;
};

struct DescriptorSetElement
{
    using ValueType = Variant<GpuBufferRef, ImageViewRef, SamplerRef, TLASRef>;

    FlatMap<uint32, ValueType> values;
    Range<uint32> dirty_range {};

    ~DescriptorSetElement()
    {
        if (values.Empty())
        {
            return;
        }

        for (auto& it : values)
        {
            if (!it.second.HasValue())
            {
                continue;
            }

            Visit(std::move(it.second), [](auto&& ref)
                {
                    SafeRelease(std::move(ref));
                });
        }
    }

    HYP_FORCE_INLINE bool IsDirty() const
    {
        return bool(dirty_range);
    }
};

template <PlatformType PLATFORM>
struct DescriptorSetPlatformImpl;

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
        return m_current_frames;
    }

    HYP_FORCE_INLINE const HashSet<FrameWeakRef>& GetCurrentFrames() const
    {
        return m_current_frames;
    }
#endif

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;
    HYP_API virtual void UpdateDirtyState(bool* out_is_dirty = nullptr) = 0;
    HYP_API virtual void Update(bool force = false) = 0;
    HYP_API virtual DescriptorSetRef Clone() const = 0;

    bool HasElement(Name name) const;

    void SetElement(Name name, uint32 index, uint32 buffer_size, const GpuBufferRef& ref);
    void SetElement(Name name, uint32 index, const GpuBufferRef& ref);
    void SetElement(Name name, const GpuBufferRef& ref);

    void SetElement(Name name, uint32 index, const ImageViewRef& ref);
    void SetElement(Name name, const ImageViewRef& ref);

    void SetElement(Name name, uint32 index, const SamplerRef& ref);
    void SetElement(Name name, const SamplerRef& ref);

    void SetElement(Name name, uint32 index, const TLASRef& ref);
    void SetElement(Name name, const TLASRef& ref);

    virtual void Bind(const CommandBufferBase* command_buffer, const GraphicsPipelineBase* pipeline, uint32 bind_index) const = 0;
    virtual void Bind(const CommandBufferBase* command_buffer, const GraphicsPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index) const = 0;
    virtual void Bind(const CommandBufferBase* command_buffer, const ComputePipelineBase* pipeline, uint32 bind_index) const = 0;
    virtual void Bind(const CommandBufferBase* command_buffer, const ComputePipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index) const = 0;
    virtual void Bind(const CommandBufferBase* command_buffer, const RaytracingPipelineBase* pipeline, uint32 bind_index) const = 0;
    virtual void Bind(const CommandBufferBase* command_buffer, const RaytracingPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index) const = 0;

protected:
    DescriptorSetBase(const DescriptorSetLayout& layout)
        : m_layout(layout)
    {
    }

    template <class T>
    DescriptorSetElement& SetElement(Name name, uint32 index, const T& ref)
    {
        const DescriptorSetLayoutElement* layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.LookupString());

        // Type check
        static const uint32 mask = DescriptorSetElementTypeInfo<typename T::Type>::mask;
        AssertThrowMsg(mask & (1u << uint32(layout_element->type)), "Layout type for %s does not match given type", name.LookupString());

        // Range check
        AssertThrowMsg(
            index < layout_element->count,
            "Index %u out of range for element %s with count %u",
            index,
            name.LookupString(),
            layout_element->count);

        // Buffer type check, to make sure the buffer type is allowed for the given element
        if constexpr (std::is_same_v<typename T::Type, GpuBufferBase>)
        {
            if (ref != nullptr)
            {
                const GpuBufferType buffer_type = ref->GetBufferType();

                AssertThrowMsg(
                    (descriptor_set_element_type_to_buffer_type[uint32(layout_element->type)] & (1u << uint32(buffer_type))),
                    "Buffer type %u is not in the allowed types for element %s",
                    uint32(buffer_type),
                    name.LookupString());

                if (layout_element->size != 0 && layout_element->size != ~0u)
                {
                    const uint32 remainder = ref->Size() % layout_element->size;

                    AssertThrowMsg(
                        remainder == 0,
                        "Buffer size (%llu) is not a multiplier of layout size (%llu) for element %s",
                        ref->Size(),
                        layout_element->size,
                        name.LookupString());
                }
            }
        }

        auto it = m_elements.Find(name);

        if (it == m_elements.End())
        {
            it = m_elements.Emplace(name).first;
        }

        DescriptorSetElement& element = it->second;

        auto element_it = element.values.Find(index);

        if (element_it == element.values.End())
        {
            element_it = element.values.Emplace(index, NormalizedType<T>(ref)).first;
        }
        else
        {
            T* current_value = element_it->second.template TryGet<T>();

            if (current_value)
            {
                SafeRelease(std::move(*current_value));
            }

            element_it->second.template Set<NormalizedType<T>>(ref);
        }

        // Mark the range as dirty so that it will be updated in the next update
        element.dirty_range |= { index, index + 1 };

        return element;
    }

    template <class T>
    void PrefillElements(Name name, uint32 count, const Optional<T>& placeholder_value = {})
    {
        bool is_bindless = false;

        if (count == ~0u)
        {
            count = max_bindless_resources;
            is_bindless = true;
        }

        const DescriptorSetLayoutElement* layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.LookupString());

        if (is_bindless)
        {
            AssertThrowMsg(
                layout_element->IsBindless(),
                "-1 given as count to prefill elements, yet %s is not specified as bindless in layout",
                name.LookupString());
        }

        auto it = m_elements.Find(name);

        if (it == m_elements.End())
        {
            it = m_elements.Emplace(name).first;
        }

        DescriptorSetElement& element = it->second;

        // // Set buffer_size, only used in the case of buffer elements
        // element.buffer_size = layout_element->size;

        element.values.Clear();
        element.values.Reserve(count);

        for (uint32 i = 0; i < count; i++)
        {
            if (placeholder_value.HasValue())
            {
                element.values.Set(i, placeholder_value.Get());
            }
            else
            {
                element.values.Set(i, T {});
            }
        }

        element.dirty_range = { 0, count };
    }

    DescriptorSetLayout m_layout;
    HashMap<Name, DescriptorSetElement> m_elements;

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    HashSet<FrameWeakRef> m_current_frames; // frames that are currently using this descriptor set
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

    HYP_FORCE_INLINE const FixedArray<Array<DescriptorSetRef>, max_frames_in_flight>& GetSets() const
    {
        return m_sets;
    }

    /*! \brief Get a descriptor set from the table
        \param name The name of the descriptor set
        \param frame_index The index of the frame for the descriptor set
        \return The descriptor set, or an unset reference if not found */
    HYP_FORCE_INLINE const DescriptorSetRef& GetDescriptorSet(Name name, uint32 frame_index) const
    {
        for (const DescriptorSetRef& set : m_sets[frame_index])
        {
            if (set->GetLayout().GetName() == name)
            {
                return set;
            }
        }

        return DescriptorSetRef::unset;
    }

    HYP_FORCE_INLINE const DescriptorSetRef& GetDescriptorSet(uint32 descriptor_set_index, uint32 frame_index) const
    {
        for (const DescriptorSetRef& set : m_sets[frame_index])
        {
            if (!set->GetLayout().IsValid())
            {
                continue;
            }

            if (set->GetLayout().GetDeclaration()->set_index == descriptor_set_index)
            {
                return set;
            }
        }

        return DescriptorSetRef::unset;
    }

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE uint32 GetDescriptorSetIndex(Name name) const
    {
        return m_decl ? m_decl->GetDescriptorSetIndex(name) : ~0u;
    }

    /*! \brief Create all descriptor sets in the table
        \param device The device to create the descriptor sets on
        \return The result of the operation */
    RendererResult Create()
    {
        if (!IsValid())
        {
            return HYP_MAKE_ERROR(RendererError, "Descriptor table declaration is not valid");
        }

        RendererResult result;

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            for (const DescriptorSetRef& set : m_sets[frame_index])
            {
                const Name descriptor_set_name = set->GetLayout().GetName();

                // use FindDescriptorSetDeclaration rather than `set->GetLayout().GetDeclaration()`, since we need to know
                // if the descriptor set is a reference to a global set
                DescriptorSetDeclaration* decl = m_decl->FindDescriptorSetDeclaration(descriptor_set_name);
                AssertThrow(decl != nullptr);

                if ((decl->flags & DescriptorSetDeclarationFlags::REFERENCE))
                {
                    // should be created elsewhere
                    continue;
                }

                result = set->Create();

                if (!result)
                {
                    return result;
                }
            }
        }

        return result;
    }

    /*! \brief Safely release all descriptor sets in the table
        \param device The device to destroy the descriptor sets on
        \return The result of the operation */
    RendererResult Destroy()
    {
        for (auto& it : m_sets)
        {
            SafeRelease(std::move(it));
        }

        m_sets = {};

        return {};
    }

    /*! \brief Apply updates to all descriptor sets in the table
        \param frame_index The index of the frame to update the descriptor sets for
        \param force If true, will update descriptor sets even if they are not marked as dirty
        \return The result of the operation */
    void Update(uint32 frame_index, bool force = false)
    {
        if (!IsValid())
        {
            return;
        }

        for (const DescriptorSetRef& set : m_sets[frame_index])
        {
            const DescriptorSetLayout& layout = set->GetLayout();

            if (layout.IsReference() || layout.IsTemplate())
            {
                // references are updated elsewhere
                // template descriptor sets are not updated (no handle to update)
                continue;
            }

            bool is_dirty = false;
            set->UpdateDirtyState(&is_dirty);

            if (!is_dirty & !force)
            {
                continue;
            }

            set->Update(force);
        }
    }

    /*! \brief Bind all descriptor sets in the table
        \param command_buffer The command buffer to push the bind commands to
        \param frame_index The index of the frame to bind the descriptor sets for
        \param pipeline The pipeline to bind the descriptor sets to
        \param offsets The offsets to bind dynamic descriptor sets with */
    template <class PipelineRef>
    void Bind(CommandBufferBase* command_buffer, uint32 frame_index, const PipelineRef& pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets) const
    {
        for (const DescriptorSetRef& set : m_sets[frame_index])
        {
            if (!set->GetLayout().IsValid())
            {
                continue;
            }

            if (set->GetLayout().IsTemplate())
            {
                continue;
            }

            const Name descriptor_set_name = set->GetLayout().GetName();

            const uint32 set_index = GetDescriptorSetIndex(descriptor_set_name);

            if (set->GetLayout().GetDynamicElements().Empty())
            {
                set->Bind(command_buffer, pipeline, set_index);

                continue;
            }

            const auto offsets_it = offsets.Find(descriptor_set_name);

            if (offsets_it != offsets.End())
            {
                set->Bind(command_buffer, pipeline, offsets_it->second, set_index);

                continue;
            }

            set->Bind(command_buffer, pipeline, {}, set_index);
        }
    }

protected:
    DescriptorTableBase(const DescriptorTableDeclaration* decl)
        : m_decl(decl)
    {
    }

    const DescriptorTableDeclaration* m_decl;
    FixedArray<Array<DescriptorSetRef>, max_frames_in_flight> m_sets;
};

} // namespace hyperion

#define HYP_DESCRIPTOR_SET(index, name) \
    static DescriptorTableDeclaration::DeclareSet HYP_UNIQUE_NAME(DescriptorSet_##name)(&GetStaticDescriptorTableDeclaration(), index, HYP_NAME_UNSAFE(name))

#define HYP_DESCRIPTOR_SRV_COND(set_name, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SRV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_UAV_COND(set_name, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_UAV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_CBUFF_COND(set_name, name, count, size, is_dynamic, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_CBUFF, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, size, is_dynamic)
#define HYP_DESCRIPTOR_SSBO_COND(set_name, name, count, size, is_dynamic, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SSBO, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, size, is_dynamic)
#define HYP_DESCRIPTOR_ACCELERATION_STRUCTURE_COND(set_name, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_SAMPLER_COND(set_name, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SAMPLER, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)

#define HYP_DESCRIPTOR_SRV(set_name, name, count) HYP_DESCRIPTOR_SRV_COND(set_name, name, count, true)
#define HYP_DESCRIPTOR_UAV(set_name, name, count) HYP_DESCRIPTOR_UAV_COND(set_name, name, count, true)
#define HYP_DESCRIPTOR_CBUFF(set_name, name, count, size, is_dynamic) HYP_DESCRIPTOR_CBUFF_COND(set_name, name, count, size, is_dynamic, true)
#define HYP_DESCRIPTOR_SSBO(set_name, name, count, size, is_dynamic) HYP_DESCRIPTOR_SSBO_COND(set_name, name, count, size, is_dynamic, true)
#define HYP_DESCRIPTOR_ACCELERATION_STRUCTURE(set_name, name, count) HYP_DESCRIPTOR_ACCELERATION_STRUCTURE_COND(set_name, name, count, true)
#define HYP_DESCRIPTOR_SAMPLER(set_name, name, count) HYP_DESCRIPTOR_SAMPLER_COND(set_name, name, count, true)

#endif