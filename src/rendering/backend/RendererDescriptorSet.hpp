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
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

class RenderResourceBase;

template <class T>
struct ShaderDataOffset
{
    static_assert(IsPODType<T>, "T must be POD to use with ShaderDataOffset");

    static constexpr uint32 invalid_index = ~0u;

    ShaderDataOffset(uint32 index)
        : index(index)
    {
    }

    template <class RenderResourceType>
    ShaderDataOffset(const RenderResourceType *render_resource, uint32 index_if_null = invalid_index)
        : index(render_resource != nullptr ? render_resource->GetBufferIndex() : index_if_null)
    {
    }

    template <class RenderResourceType, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<RenderResourceType>> && !std::is_integral_v<NormalizedType<RenderResourceType>>>>
    ShaderDataOffset(const RenderResourceType &render_resource)
        : index(render_resource.GetBufferIndex())
    {
    }

    HYP_FORCE_INLINE operator uint32() const
    {
        AssertDebugMsg(index != invalid_index, "Index was ~0u when converting to uint32");

        return uint32(sizeof(T) * index);
    }

    uint32  index;
};

namespace renderer {

struct DescriptorSetDeclaration;

enum class DescriptorSetElementType : uint32
{
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    STORAGE_BUFFER,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    IMAGE_STORAGE,
    SAMPLER,
    TLAS,
    MAX
};

constexpr uint32 descriptor_set_element_type_to_buffer_type[uint32(DescriptorSetElementType::MAX)] = {
    0,                                                          // UNSET
    (1u << uint32(GPUBufferType::CONSTANT_BUFFER)),               // UNIFORM_BUFFER
    (1u << uint32(GPUBufferType::CONSTANT_BUFFER)),               // UNIFORM_BUFFER_DYNAMIC
    (1u << uint32(GPUBufferType::STORAGE_BUFFER))
        | (1u << uint32(GPUBufferType::ATOMIC_COUNTER))
        | (1u << uint32(GPUBufferType::STAGING_BUFFER))
        | (1u << uint32(GPUBufferType::INDIRECT_ARGS_BUFFER)),    // STORAGE_BUFFER
    
    (1u << uint32(GPUBufferType::STORAGE_BUFFER))
        | (1u << uint32(GPUBufferType::ATOMIC_COUNTER))
        | (1u << uint32(GPUBufferType::STAGING_BUFFER))
        | (1u << uint32(GPUBufferType::INDIRECT_ARGS_BUFFER)),    // STORAGE_BUFFER_DYNAMIC
    0,                                                          // IMAGE
    0,                                                          // IMAGE_STORAGE
    0,                                                          // SAMPLER
    (1u << uint32(GPUBufferType::ACCELERATION_STRUCTURE_BUFFER))  // ACCELERATION_STRUCTURE
};

struct DescriptorSetLayoutElement
{
    DescriptorSetElementType    type = DescriptorSetElementType::UNSET;
    uint32                      binding = ~0u; // has to be set
    uint32                      count = 1; // Set to -1 for bindless
    uint32                      size = ~0u;

    HYP_FORCE_INLINE bool IsBindless() const
        { return count == ~0u; }

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

struct DescriptorDeclaration
{
    using ConditionFunction = bool (*)();

    DescriptorSlot      slot = DESCRIPTOR_SLOT_NONE;
    Name                name;
    ConditionFunction   cond = nullptr;
    uint32              count = 1;
    uint32              size = uint32(-1);
    bool                is_dynamic = false;
    uint32              index = ~0u;

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

struct DescriptorSetDeclaration
{
    uint32                                                          set_index = ~0u;
    Name                                                            name = Name::Invalid();
    FixedArray<Array<DescriptorDeclaration>, DESCRIPTOR_SLOT_MAX>   slots = { };

    // is this a reference to a global descriptor set declaration?
    bool                                                            is_reference = false;
    // is this descriptor set intended to be used as a template for other sets? (e.g material textures)
    bool                                                            is_template = false;

    DescriptorSetDeclaration()                                                      = default;

    DescriptorSetDeclaration(uint32 set_index, Name name, bool is_reference = false, bool is_template = false)
        : set_index(set_index),
          name(name),
          is_reference(is_reference),
          is_template(is_template)
    {
    }

    DescriptorSetDeclaration(const DescriptorSetDeclaration &other)                 = default;
    DescriptorSetDeclaration &operator=(const DescriptorSetDeclaration &other)      = default;
    DescriptorSetDeclaration(DescriptorSetDeclaration &&other) noexcept             = default;
    DescriptorSetDeclaration &operator=(DescriptorSetDeclaration &&other) noexcept  = default;
    ~DescriptorSetDeclaration()                                                     = default;

    HYP_FORCE_INLINE Array<DescriptorDeclaration> &GetSlot(DescriptorSlot slot)
    {
        AssertThrow(slot < DESCRIPTOR_SLOT_MAX && slot > DESCRIPTOR_SLOT_NONE);

        return slots[uint32(slot) - 1];
    }

    HYP_FORCE_INLINE const Array<DescriptorDeclaration> &GetSlot(DescriptorSlot slot) const
    {
        AssertThrow(slot < DESCRIPTOR_SLOT_MAX && slot > DESCRIPTOR_SLOT_NONE);

        return slots[uint32(slot) - 1];
    }

    HYP_FORCE_INLINE void AddDescriptorDeclaration(renderer::DescriptorDeclaration decl)
    {
        AssertThrow(decl.slot != DESCRIPTOR_SLOT_NONE && decl.slot < DESCRIPTOR_SLOT_MAX);

        decl.index = uint32(slots[uint32(decl.slot) - 1].Size());
        slots[uint32(decl.slot) - 1].PushBack(std::move(decl));
    }

    /*! \brief Calculate a flat index for a Descriptor that is part of this set.
        Returns -1 if not found */
    uint32 CalculateFlatIndex(DescriptorSlot slot, Name name) const;

    DescriptorDeclaration *FindDescriptorDeclaration(Name name) const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(set_index);
        hc.Add(name);
        hc.Add(is_reference);
        hc.Add(is_template);

        for (const auto &slot : slots) {
            for (const auto &decl : slot) {
                hc.Add(decl.GetHashCode());
            }
        }

        return hc;
    }
};

class DescriptorTableDeclaration
{
public:
    DescriptorTableDeclaration()                                                    = default;
    DescriptorTableDeclaration(const DescriptorTableDeclaration &)                  = default;
    DescriptorTableDeclaration &operator=(const DescriptorTableDeclaration &)       = default;
    DescriptorTableDeclaration(DescriptorTableDeclaration &&) noexcept              = default;
    DescriptorTableDeclaration &operator=(DescriptorTableDeclaration &&) noexcept   = default;
    ~DescriptorTableDeclaration()                                                   = default;

    DescriptorSetDeclaration *FindDescriptorSetDeclaration(Name name) const;
    DescriptorSetDeclaration *AddDescriptorSetDeclaration(DescriptorSetDeclaration descriptor_set);

    HYP_FORCE_INLINE Array<DescriptorSetDeclaration> &GetElements()
        { return m_elements; }

    HYP_FORCE_INLINE const Array<DescriptorSetDeclaration> &GetElements() const
        { return m_elements; }
        
    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE uint32 GetDescriptorSetIndex(Name name) const
    {
        for (const auto &it : m_elements) {
            if (it.name == name) {
                return it.set_index;
            }
        }

        return ~0u;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (const DescriptorSetDeclaration &decl : m_elements) {
            hc.Add(decl.GetHashCode());
        }

        return hc;
    }

private:
    Array<DescriptorSetDeclaration> m_elements;

public:
    struct DeclareSet
    {
        DeclareSet(DescriptorTableDeclaration *table, uint32 set_index, Name name)
        {
            AssertThrow(table != nullptr);

            if (table->m_elements.Size() <= set_index) {
                table->m_elements.Resize(set_index + 1);
            }

            DescriptorSetDeclaration decl { };
            decl.set_index = set_index;
            decl.name = name;
            table->m_elements[set_index] = std::move(decl);
        }
    };

    struct DeclareDescriptor
    {
        DeclareDescriptor(DescriptorTableDeclaration *table, Name set_name, DescriptorSlot slot_type, Name descriptor_name, DescriptorDeclaration::ConditionFunction cond = nullptr, uint32 count = 1, uint32 size = ~0u, bool is_dynamic = false)
        {
            AssertThrow(table != nullptr);

            uint32 set_index = ~0u;

            for (SizeType i = 0; i < table->m_elements.Size(); ++i) {
                if (table->m_elements[i].name == set_name) {
                    set_index = uint32(i);
                    break;
                }
            }

            AssertThrowMsg(set_index != ~0u, "Descriptor set %s not found", set_name.LookupString());

            DescriptorSetDeclaration &decl = table->m_elements[set_index];
            AssertThrow(decl.set_index == set_index);
            AssertThrow(slot_type > 0 && slot_type < decl.slots.Size());

            const uint32 slot_index = uint32(decl.slots[uint32(slot_type) - 1].Size());

            DescriptorDeclaration descriptor_decl { };
            descriptor_decl.index = slot_index;
            descriptor_decl.slot = slot_type;
            descriptor_decl.name = descriptor_name;
            descriptor_decl.cond = cond;
            descriptor_decl.size = size;
            descriptor_decl.count = count;
            descriptor_decl.is_dynamic = is_dynamic;
            decl.slots[uint32(slot_type) - 1].PushBack(descriptor_decl);
        }
    };
};

extern DescriptorTableDeclaration &GetStaticDescriptorTableDeclaration();

namespace platform {

template <PlatformType PLATFORM>
class Device;
    
template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class DescriptorSet;

template <PlatformType PLATFORM>
class ImageView;

template <PlatformType PLATFORM>
class Sampler;

template <PlatformType PLATFORM>
class TopLevelAccelerationStructure;

template <PlatformType PLATFORM>
class Frame;

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class ComputePipeline;

template <PlatformType PLATFORM>
class RaytracingPipeline;

template <PlatformType PLATFORM, class T>
struct DescriptorSetElementTypeInfo;

template <PlatformType PLATFORM>
class DescriptorSetLayout
{
public:
    DescriptorSetLayout(const DescriptorSetDeclaration &decl);

    DescriptorSetLayout(const DescriptorSetLayout &other)
        : m_decl(other.m_decl),
          m_elements(other.m_elements),
          m_dynamic_elements(other.m_dynamic_elements)
    {
    }

    DescriptorSetLayout &operator=(const DescriptorSetLayout &other)
    {
        if (this == &other) {
            return *this;
        }

        m_decl = other.m_decl;
        m_elements = other.m_elements;
        m_dynamic_elements = other.m_dynamic_elements;

        return *this;
    }

    DescriptorSetLayout(DescriptorSetLayout &&other) noexcept
        : m_decl(std::move(other.m_decl)),
          m_elements(std::move(other.m_elements)),
          m_dynamic_elements(std::move(other.m_dynamic_elements))
    {
    }

    DescriptorSetLayout &operator=(DescriptorSetLayout &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        m_decl = std::move(other.m_decl);
        m_elements = std::move(other.m_elements);
        m_dynamic_elements = std::move(other.m_dynamic_elements);

        return *this;
    }
    
    ~DescriptorSetLayout() = default;

    DescriptorSetRef<PLATFORM> CreateDescriptorSet() const;

    HYP_FORCE_INLINE Name GetName() const
        { return m_decl.name; }

    HYP_FORCE_INLINE const DescriptorSetDeclaration &GetDeclaration() const
        { return m_decl; }

    HYP_FORCE_INLINE const HashMap<Name, DescriptorSetLayoutElement> &GetElements() const
        { return m_elements; }

    HYP_FORCE_INLINE void AddElement(Name name, DescriptorSetElementType type, uint32 binding, uint32 count, uint32 size = ~0u)
        { m_elements.Insert(name, DescriptorSetLayoutElement { type, binding, count, size }); }

    HYP_FORCE_INLINE const DescriptorSetLayoutElement *GetElement(Name name) const
    {
        const auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            return nullptr;
        }

        return &it->second;
    }

    HYP_FORCE_INLINE const Array<Name> &GetDynamicElements() const
        { return m_dynamic_elements; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_decl.GetHashCode());

        for (const auto &it : m_elements) {
            hc.Add(it.first.GetHashCode());
            hc.Add(it.second.GetHashCode());
        }

        return hc;
    }

private:
    DescriptorSetDeclaration                    m_decl;
    HashMap<Name, DescriptorSetLayoutElement>   m_elements;
    Array<Name>                                 m_dynamic_elements;
};

template <PlatformType PLATFORM>
struct DescriptorSetElement
{
    static constexpr PlatformType platform = PLATFORM;
    
    using ValueType = Variant<GPUBufferRef<PLATFORM>, ImageViewRef<PLATFORM>, SamplerRef<PLATFORM>, TLASRef<PLATFORM>>;

    FlatMap<uint32, ValueType>  values;
    Range<uint32>               dirty_range { };

    ~DescriptorSetElement()
    {
        if (values.Empty()) {
            return;
        }

        for (auto &it : values) {
            if (!it.second.IsValid()) {
                continue;
            }
            
            if (it.second.template Is<GPUBufferRef<PLATFORM>>()) {
                SafeRelease(std::move(it.second.template Get<GPUBufferRef<PLATFORM>>()));
            } else if (it.second.template Is<ImageViewRef<PLATFORM>>()) {
                SafeRelease(std::move(it.second.template Get<ImageViewRef<PLATFORM>>()));
            } else if (it.second.template Is<SamplerRef<PLATFORM>>()) {
                SafeRelease(std::move(it.second.template Get<SamplerRef<PLATFORM>>()));
            } else if (it.second.template Is<TLASRef<PLATFORM>>()) {
                SafeRelease(std::move(it.second.template Get<TLASRef<PLATFORM>>()));
            } else {
                DebugLog(LogType::Warn, "Unknown descriptor set element type when releasing.\n");
            }
        }
    }

    HYP_FORCE_INLINE bool IsDirty() const
        { return bool(dirty_range); }
};

template <PlatformType PLATFORM>
struct DescriptorSetPlatformImpl;

template <PlatformType PLATFORM>
class DescriptorSet
{
public:
    static constexpr PlatformType platform = PLATFORM;

    DescriptorSet(const DescriptorSetLayout<PLATFORM> &layout);
    DescriptorSet(const DescriptorSet &other)                 = delete;
    DescriptorSet &operator=(const DescriptorSet &other)      = delete;
    DescriptorSet(DescriptorSet &&other) noexcept             = delete;
    DescriptorSet &operator=(DescriptorSet &&other) noexcept  = delete;
    ~DescriptorSet();

    HYP_FORCE_INLINE DescriptorSetPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const DescriptorSetPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE const DescriptorSetLayout<PLATFORM> &GetLayout() const
        { return m_layout; }

    HYP_FORCE_INLINE const HashMap<Name, DescriptorSetElement<PLATFORM>> &GetElements() const
        { return m_elements; }

    HYP_API bool IsCreated() const;

    HYP_API RendererResult Create(Device<PLATFORM> *device);
    HYP_API RendererResult Destroy(Device<PLATFORM> *device);
    HYP_API void Update(Device<PLATFORM> *device);

    bool HasElement(Name name) const;

    void SetElement(Name name, uint32 index, uint32 buffer_size, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(Name name, uint32 index, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(Name name, const GPUBufferRef<PLATFORM> &ref);
    
    void SetElement(Name name, uint32 index, const ImageViewRef<PLATFORM> &ref);
    void SetElement(Name name, const ImageViewRef<PLATFORM> &ref);
    
    void SetElement(Name name, uint32 index, const SamplerRef<PLATFORM> &ref);
    void SetElement(Name name, const SamplerRef<PLATFORM> &ref);
    
    void SetElement(Name name, uint32 index, const TLASRef<PLATFORM> &ref);
    void SetElement(Name name, const TLASRef<PLATFORM> &ref);

    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const GraphicsPipeline<PLATFORM> *pipeline, uint32 bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const GraphicsPipeline<PLATFORM> *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const ComputePipeline<PLATFORM> *pipeline, uint32 bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const ComputePipeline<PLATFORM> *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const RaytracingPipeline<PLATFORM> *pipeline, uint32 bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const RaytracingPipeline<PLATFORM> *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const;

    DescriptorSetRef<PLATFORM> Clone() const;

private:
    template <class T>
    DescriptorSetElement<PLATFORM> &SetElement(Name name, uint32 index, const T &ref)
    {
        const DescriptorSetLayoutElement *layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.LookupString());

        // Type check
        static const uint32 mask = DescriptorSetElementTypeInfo<PLATFORM, typename T::Type>::mask;
        AssertThrowMsg(mask & (1u << uint32(layout_element->type)), "Layout type for %s does not match given type", name.LookupString());

        // Range check
        AssertThrowMsg(
            index < layout_element->count,
            "Index %u out of range for element %s with count %u",
            index,
            name.LookupString(),
            layout_element->count
        );

        // Buffer type check, to make sure the buffer type is allowed for the given element
        if constexpr (std::is_same_v<typename T::Type, GPUBuffer<PLATFORM>>) {
            if (ref != nullptr) {
                const GPUBufferType buffer_type = ref->GetBufferType();

                AssertThrowMsg(
                    (descriptor_set_element_type_to_buffer_type[uint32(layout_element->type)] & (1u << uint32(buffer_type))),
                    "Buffer type %u is not in the allowed types for element %s",
                    uint32(buffer_type),
                    name.LookupString()
                );

                if (layout_element->size != 0 && layout_element->size != ~0u) {
                    const uint32 remainder = ref->Size() % layout_element->size;

                    AssertThrowMsg(
                        remainder == 0,
                        "Buffer size (%llu) is not a multiplier of layout size (%llu) for element %s",
                        ref->Size(),
                        layout_element->size,
                        name.LookupString()
                    );
                }
            }
        }

        auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            it = m_elements.Insert({
                name,
                DescriptorSetElement<PLATFORM> { }
            }).first;
        }

        DescriptorSetElement<PLATFORM> &element = it->second;

        auto element_it = element.values.Find(index);

        if (element_it == element.values.End()) {
            element_it = element.values.Insert({
                index,
                NormalizedType<T>(ref)
            }).first;
        } else {
            T *current_value = element_it->second.template TryGet<T>();

            if (current_value) {
                SafeRelease(std::move(*current_value));
            }

            element_it->second.template Set<NormalizedType<T>>(ref);
        }

        // Mark the range as dirty so that it will be updated in the next update
        element.dirty_range |= { index, index + 1 };

        return element;
    }

    template <class T>
    void PrefillElements(Name name, uint32 count, const Optional<T> &placeholder_value = { })
    {
        bool is_bindless = false;

        if (count == ~0u) {
            count = max_bindless_resources;
            is_bindless = true;
        }

        const DescriptorSetLayoutElement *layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.LookupString());

        if (is_bindless) {
            AssertThrowMsg(
                layout_element->IsBindless(),
                "-1 given as count to prefill elements, yet %s is not specified as bindless in layout",
                name.LookupString()
            );
        }

        auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            it = m_elements.Insert({
                name,
                DescriptorSetElement<PLATFORM> { }
            }).first;
        }

        DescriptorSetElement<PLATFORM> &element = it->second;

        // // Set buffer_size, only used in the case of buffer elements
        // element.buffer_size = layout_element->size;
        
        element.values.Clear();
        element.values.Reserve(count);

        for (uint32 i = 0; i < count; i++) {
            if (placeholder_value.HasValue()) {
                element.values.Set(i, placeholder_value.Get());
            } else {
                element.values.Set(i, T { });
            }
        }

        element.dirty_range = { 0, count };
    }

    DescriptorSetPlatformImpl<PLATFORM>             m_platform_impl;

    DescriptorSetLayout<PLATFORM>                   m_layout;
    HashMap<Name, DescriptorSetElement<PLATFORM>>   m_elements;
};

template <PlatformType PLATFORM>
class DescriptorSetManager
{
public:
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class DescriptorTable
{
public:
    static constexpr PlatformType platform = PLATFORM;

    DescriptorTable(const DescriptorTableDeclaration &decl);
    DescriptorTable(const DescriptorTable &other)                 = default;
    DescriptorTable &operator=(const DescriptorTable &other)      = default;
    DescriptorTable(DescriptorTable &&other) noexcept             = default;
    DescriptorTable &operator=(DescriptorTable &&other) noexcept  = default;
    ~DescriptorTable()                                            = default;

    HYP_FORCE_INLINE const DescriptorTableDeclaration &GetDeclaration() const
        { return m_decl; }

    HYP_FORCE_INLINE const FixedArray<Array<DescriptorSetRef<PLATFORM>>, max_frames_in_flight> &GetSets() const
        { return m_sets; }

    /*! \brief Get a descriptor set from the table
        \param name The name of the descriptor set
        \param frame_index The index of the frame for the descriptor set
        \return The descriptor set, or an unset reference if not found */
    HYP_FORCE_INLINE const DescriptorSetRef<PLATFORM> &GetDescriptorSet(Name name, uint32 frame_index) const
    {
        for (const DescriptorSetRef<PLATFORM> &set : m_sets[frame_index]) {
            if (set->GetLayout().GetName() == name) {
                return set;
            }
        }

        return DescriptorSetRef<PLATFORM>::unset;
    }
    
    HYP_FORCE_INLINE const DescriptorSetRef<PLATFORM> &GetDescriptorSet(uint32 descriptor_set_index, uint32 frame_index) const
    {
        for (const DescriptorSetRef<PLATFORM> &set : m_sets[frame_index]) {
            if (set->GetLayout().GetDeclaration().set_index == descriptor_set_index) {
                return set;
            }
        }

        return DescriptorSetRef<PLATFORM>::unset;
    }

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE uint32 GetDescriptorSetIndex(Name name) const
        { return m_decl.GetDescriptorSetIndex(name); }

    /*! \brief Create all descriptor sets in the table
        \param device The device to create the descriptor sets on
        \return The result of the operation */
    RendererResult Create(Device<PLATFORM> *device)
    {
        RendererResult result;

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (const DescriptorSetRef<PLATFORM> &set : m_sets[frame_index]) {
                const Name descriptor_set_name = set->GetLayout().GetName();

                // use FindDescriptorSetDeclaration rather than `set->GetLayout().GetDeclaration()`, since we need to know
                // if the descriptor set is a reference to a global set
                DescriptorSetDeclaration *decl = m_decl.FindDescriptorSetDeclaration(descriptor_set_name);
                AssertThrow(decl != nullptr);

                if (decl->is_reference) {
                    // should already be created
                    continue;
                }

                result = set->Create(device);

                if (!result) {
                    return result;
                }
            }
        }

        return result;
    }

    /*! \brief Safely release all descriptor sets in the table
        \param device The device to destroy the descriptor sets on
        \return The result of the operation */
    RendererResult Destroy(Device<PLATFORM> *device)
    {
        for (auto &it : m_sets) {
            SafeRelease(std::move(it));
        }
        
        m_sets = { };

        return { };
    }

    /*! \brief Apply updates to all descriptor sets in the table
        \param device The device to update the descriptor sets on
        \param frame_index The index of the frame to update the descriptor sets for
        \return The result of the operation */
    void Update(Device<PLATFORM> *device, uint32 frame_index)
    {
        for (const DescriptorSetRef<PLATFORM> &set : m_sets[frame_index]) {
            const Name descriptor_set_name = set->GetLayout().GetName();

            DescriptorSetDeclaration *decl = m_decl.FindDescriptorSetDeclaration(descriptor_set_name);
            AssertThrow(decl != nullptr);

            if (decl->is_reference) {
                // reference, updated elsewhere
                continue;
            }

            set->Update(device);
        }
    }

    /*! \brief Bind all descriptor sets in the table
        \param command_buffer The command buffer to push the bind commands to
        \param frame_index The index of the frame to bind the descriptor sets for
        \param pipeline The pipeline to bind the descriptor sets to
        \param offsets The offsets to bind dynamic descriptor sets with */
    template <class PipelineRef>
    void Bind(CommandBuffer<PLATFORM> *command_buffer, uint32 frame_index, const PipelineRef &pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>> &offsets) const
    {
        for (const DescriptorSetRef<PLATFORM> &set : m_sets[frame_index]) {
            const Name descriptor_set_name = set->GetLayout().GetName();

            const uint32 set_index = GetDescriptorSetIndex(descriptor_set_name);

            if (set->GetLayout().GetDynamicElements().Empty()) {
                set->Bind(command_buffer, pipeline, set_index);

                continue;
            }

            const auto offsets_it = offsets.Find(descriptor_set_name);

            if (offsets_it != offsets.End()) {
                set->Bind(command_buffer, pipeline, offsets_it->second, set_index);

                continue;
            }

            set->Bind(command_buffer, pipeline, set_index);
        }
    }

private:
    DescriptorTableDeclaration                                              m_decl;
    FixedArray<Array<DescriptorSetRef<PLATFORM>>, max_frames_in_flight>    m_sets;
};

} // namespace platform

using DescriptorSet = platform::DescriptorSet<Platform::CURRENT>;
using DescriptorSetElement = platform::DescriptorSetElement<Platform::CURRENT>;
using DescriptorSetLayout = platform::DescriptorSetLayout<Platform::CURRENT>;
using DescriptorSetManager = platform::DescriptorSetManager<Platform::CURRENT>;
using DescriptorTable = platform::DescriptorTable<Platform::CURRENT>;

} // namespace renderer
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