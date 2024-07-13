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

constexpr uint descriptor_set_element_type_to_buffer_type[uint(DescriptorSetElementType::MAX)] = {
    0,                                                          // UNSET
    (1u << uint(GPUBufferType::CONSTANT_BUFFER)),               // UNIFORM_BUFFER
    (1u << uint(GPUBufferType::CONSTANT_BUFFER)),               // UNIFORM_BUFFER_DYNAMIC
    (1u << uint(GPUBufferType::STORAGE_BUFFER))
        | (1u << uint(GPUBufferType::ATOMIC_COUNTER))
        | (1u << uint(GPUBufferType::STAGING_BUFFER))
        | (1u << uint(GPUBufferType::INDIRECT_ARGS_BUFFER)),    // STORAGE_BUFFER
    
    (1u << uint(GPUBufferType::STORAGE_BUFFER))
        | (1u << uint(GPUBufferType::ATOMIC_COUNTER))
        | (1u << uint(GPUBufferType::STAGING_BUFFER))
        | (1u << uint(GPUBufferType::INDIRECT_ARGS_BUFFER)),    // STORAGE_BUFFER_DYNAMIC
    0,                                                          // IMAGE
    0,                                                          // IMAGE_STORAGE
    0,                                                          // SAMPLER
    (1u << uint(GPUBufferType::ACCELERATION_STRUCTURE_BUFFER))  // ACCELERATION_STRUCTURE
};

struct DescriptorSetLayoutElement
{
    DescriptorSetElementType    type = DescriptorSetElementType::UNSET;
    uint                        binding = -1; // has to be set
    uint                        count = 1; // Set to -1 for bindless
    uint                        size = -1;

    bool IsBindless() const
        { return count == ~0u; }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(type);
        hc.Add(binding);
        hc.Add(count);
        hc.Add(size);

        return hc;
    }
};

template <SizeType Sz>
static inline uint32 GetDescriptorSetElementTypeMask(FixedArray<DescriptorSetElementType, Sz> types)
{
    uint32 mask = 0;

    for (const auto &type : types) {
        mask |= 1 << uint32(type);
    }

    return mask;
}

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
    uint                count = 1;
    uint                size = uint(-1);
    bool                is_dynamic = false;
    uint                index = ~0u;

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
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
    uint                                                            set_index = ~0u;
    Name                                                            name = Name::Invalid();
    FixedArray<Array<DescriptorDeclaration>, DESCRIPTOR_SLOT_MAX>   slots = { };

    // is this a reference to a global descriptor set declaration?
    bool                                                            is_reference = false;
    // is this descriptor set intended to be used as a template for other sets? (e.g material textures)
    bool                                                            is_template = false;

    DescriptorSetDeclaration()                                                      = default;

    DescriptorSetDeclaration(uint set_index, Name name, bool is_reference = false, bool is_template = false)
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

    Array<DescriptorDeclaration> &GetSlot(DescriptorSlot slot)
    {
        AssertThrow(slot < DESCRIPTOR_SLOT_MAX && slot > DESCRIPTOR_SLOT_NONE);

        return slots[uint(slot) - 1];
    }

    const Array<DescriptorDeclaration> &GetSlot(DescriptorSlot slot) const
    {
        AssertThrow(slot < DESCRIPTOR_SLOT_MAX && slot > DESCRIPTOR_SLOT_NONE);

        return slots[uint(slot) - 1];
    }

    void AddDescriptorDeclaration(renderer::DescriptorDeclaration decl)
    {
        AssertThrow(decl.slot != DESCRIPTOR_SLOT_NONE && decl.slot < DESCRIPTOR_SLOT_MAX);

        decl.index = uint(slots[uint(decl.slot) - 1].Size());
        slots[uint(decl.slot) - 1].PushBack(std::move(decl));
    }

    /*! \brief Calculate a flat index for a Descriptor that is part of this set.
        Returns -1 if not found */
    uint CalculateFlatIndex(DescriptorSlot slot, Name name) const;

    DescriptorDeclaration *FindDescriptorDeclaration(Name name) const;

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
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

    Array<DescriptorSetDeclaration> &GetElements()
        { return m_elements; }

    const Array<DescriptorSetDeclaration> &GetElements() const
        { return m_elements; }
        
    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE
    uint GetDescriptorSetIndex(Name name) const
    {
        for (const auto &it : m_elements) {
            if (it.name == name) {
                return it.set_index;
            }
        }

        return -1;
    }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &decl : m_elements) {
            hc.Add(decl.GetHashCode());
        }

        return hc;
    }

private:
    Array<DescriptorSetDeclaration> m_elements;

public:
    struct DeclareSet
    {
        DeclareSet(DescriptorTableDeclaration *table, uint set_index, Name name)
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
        DeclareDescriptor(DescriptorTableDeclaration *table, Name set_name, DescriptorSlot slot_type, Name descriptor_name, DescriptorDeclaration::ConditionFunction cond = nullptr, uint count = 1, uint size = -1, bool is_dynamic = false)
        {
            AssertThrow(table != nullptr);

            uint set_index = ~0u;

            for (uint i = 0; i < table->m_elements.Size(); ++i) {
                if (table->m_elements[i].name == set_name) {
                    set_index = i;
                    break;
                }
            }

            AssertThrowMsg(set_index != ~0u, "Descriptor set %s not found", set_name.LookupString());

            DescriptorSetDeclaration &decl = table->m_elements[set_index];
            AssertThrow(decl.set_index == set_index);
            AssertThrow(slot_type > 0 && slot_type < decl.slots.Size());

            const uint slot_index = decl.slots[uint(slot_type) - 1].Size();

            DescriptorDeclaration descriptor_decl { };
            descriptor_decl.index = slot_index;
            descriptor_decl.slot = slot_type;
            descriptor_decl.name = descriptor_name;
            descriptor_decl.cond = cond;
            descriptor_decl.size = size;
            descriptor_decl.count = count;
            descriptor_decl.is_dynamic = is_dynamic;
            decl.slots[uint(slot_type) - 1].PushBack(descriptor_decl);
        }
    };
};

extern DescriptorTableDeclaration *g_static_descriptor_table_decl;

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

    Name GetName() const
        { return m_decl.name; }

    const DescriptorSetDeclaration &GetDeclaration() const
        { return m_decl; }

    const HashMap<Name, DescriptorSetLayoutElement> &GetElements() const
        { return m_elements; }

    void AddElement(Name name, DescriptorSetElementType type, uint binding, uint count, uint size = -1)
        { m_elements.Insert(name, DescriptorSetLayoutElement { type, binding, count, size }); }

    const DescriptorSetLayoutElement *GetElement(Name name) const
    {
        const auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            return nullptr;
        }

        return &it->second;
    }

    const Array<Name> &GetDynamicElements() const
        { return m_dynamic_elements; }

    HashCode GetHashCode() const
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

    FlatMap<uint, ValueType>    values;
    Range<uint>                 dirty_range { };

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

    HYP_FORCE_INLINE
    bool IsDirty() const
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

    [[nodiscard]]
    HYP_FORCE_INLINE
    DescriptorSetPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const DescriptorSetPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const DescriptorSetLayout<PLATFORM> &GetLayout() const
        { return m_layout; }

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);
    HYP_API Result Update(Device<PLATFORM> *device);

    bool HasElement(Name name) const;

    void SetElement(Name name, uint index, uint buffer_size, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(Name name, uint index, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(Name name, const GPUBufferRef<PLATFORM> &ref);
    
    void SetElement(Name name, uint index, const ImageViewRef<PLATFORM> &ref);
    void SetElement(Name name, const ImageViewRef<PLATFORM> &ref);
    
    void SetElement(Name name, uint index, const SamplerRef<PLATFORM> &ref);
    void SetElement(Name name, const SamplerRef<PLATFORM> &ref);
    
    void SetElement(Name name, uint index, const TLASRef<PLATFORM> &ref);
    void SetElement(Name name, const TLASRef<PLATFORM> &ref);

    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const GraphicsPipeline<PLATFORM> *pipeline, uint bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const GraphicsPipeline<PLATFORM> *pipeline, const ArrayMap<Name, uint> &offsets, uint bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const ComputePipeline<PLATFORM> *pipeline, uint bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const ComputePipeline<PLATFORM> *pipeline, const ArrayMap<Name, uint> &offsets, uint bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const RaytracingPipeline<PLATFORM> *pipeline, uint bind_index) const;
    void Bind(const CommandBuffer<PLATFORM> *command_buffer, const RaytracingPipeline<PLATFORM> *pipeline, const ArrayMap<Name, uint> &offsets, uint bind_index) const;

    DescriptorSetRef<PLATFORM> Clone() const;

private:
    template <class T>
    DescriptorSetElement<PLATFORM> &SetElement(Name name, uint index, const T &ref)
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
                    (descriptor_set_element_type_to_buffer_type[uint(layout_element->type)] & (1u << uint(buffer_type))),
                    "Buffer type %u is not in the allowed types for element %s",
                    uint(buffer_type),
                    name.LookupString()
                );

                if (layout_element->size != 0 && layout_element->size != ~0u) {
                    const uint remainder = ref->Size() % layout_element->size;

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
    void PrefillElements(Name name, uint count, const Optional<T> &placeholder_value = { })
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

        for (uint i = 0; i < count; i++) {
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

    const DescriptorTableDeclaration &GetDeclaration() const
        { return m_decl; }

    const FixedArray<Array<DescriptorSetRef<PLATFORM>>, max_frames_in_flight> &GetSets() const
        { return m_sets; }

    /*! \brief Get a descriptor set from the table
        \param name The name of the descriptor set
        \param frame_index The index of the frame for the descriptor set
        \return The descriptor set, or an unset reference if not found */
    const DescriptorSetRef<PLATFORM> &GetDescriptorSet(Name name, uint frame_index) const
    {
        for (const DescriptorSetRef<PLATFORM> &set : m_sets[frame_index]) {
            if (set->GetLayout().GetName() == name) {
                return set;
            }
        }

        return DescriptorSetRef<PLATFORM>::unset;
    }

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE
    uint GetDescriptorSetIndex(Name name) const
        { return m_decl.GetDescriptorSetIndex(name); }

    /*! \brief Create all descriptor sets in the table
        \param device The device to create the descriptor sets on
        \return The result of the operation */
    Result Create(Device<PLATFORM> *device)
    {
        Result result = Result { };

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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
    Result Destroy(Device<PLATFORM> *device)
    {
        for (auto &it : m_sets) {
            SafeRelease(std::move(it));
        }
        
        m_sets = { };

        return Result { };
    }

    /*! \brief Apply updates to all descriptor sets in the table
        \param device The device to update the descriptor sets on
        \param frame_index The index of the frame to update the descriptor sets for
        \return The result of the operation */
    Result Update(Device<PLATFORM> *device, uint frame_index)
    {
        for (const DescriptorSetRef<PLATFORM> &set : m_sets[frame_index]) {
            const Name descriptor_set_name = set->GetLayout().GetName();

            DescriptorSetDeclaration *decl = m_decl.FindDescriptorSetDeclaration(descriptor_set_name);
            AssertThrow(decl != nullptr);

            if (decl->is_reference) {
                // reference, updated elsewhere
                continue;
            }

            const Result set_result = set->Update(device);

            if (!set_result) {
                return set_result;
            }
        }

        return Result { };
    }

    HYP_FORCE_INLINE
    void Bind(Frame<PLATFORM> *frame, const GraphicsPipelineRef<PLATFORM> &pipeline, const ArrayMap<Name, ArrayMap<Name, uint>> &offsets)
    {
        Bind<GraphicsPipelineRef<PLATFORM>>(frame->GetCommandBuffer(), frame->GetFrameIndex(), pipeline, offsets);
    }

    HYP_FORCE_INLINE
    void Bind(Frame<PLATFORM> *frame, const ComputePipelineRef<PLATFORM> &pipeline, const ArrayMap<Name, ArrayMap<Name, uint>> &offsets) const
    {
        Bind<ComputePipelineRef<PLATFORM>>(frame->GetCommandBuffer(), frame->GetFrameIndex(), pipeline, offsets);
    }

    HYP_FORCE_INLINE
    void Bind(Frame<PLATFORM> *frame, const RaytracingPipelineRef<PLATFORM> &pipeline, const ArrayMap<Name, ArrayMap<Name, uint>> &offsets) const
    {
        Bind<RaytracingPipelineRef<PLATFORM>>(frame->GetCommandBuffer(), frame->GetFrameIndex(), pipeline, offsets);
    }

    /*! \brief Bind all descriptor sets in the table
        \param command_buffer The command buffer to push the bind commands to
        \param frame_index The index of the frame to bind the descriptor sets for
        \param pipeline The pipeline to bind the descriptor sets to
        \param offsets The offsets to bind dynamic descriptor sets with */
    template <class PipelineRef>
    void Bind(CommandBuffer<PLATFORM> *command_buffer, uint frame_index, const PipelineRef &pipeline, const ArrayMap<Name, ArrayMap<Name, uint>> &offsets) const
    {
        for (const DescriptorSetRef<PLATFORM> &set : m_sets[frame_index]) {
            const Name descriptor_set_name = set->GetLayout().GetName();

            const uint set_index = GetDescriptorSetIndex(descriptor_set_name);

            if (set->GetLayout().GetDynamicElements().Empty()) {
                set->Bind(command_buffer, pipeline, set_index);

                continue;
            }

            const auto offsets_it = offsets.Find(descriptor_set_name);
            AssertThrowMsg(offsets_it != offsets.End(), "No offsets given for descriptor set %s", descriptor_set_name.LookupString());

            set->Bind(command_buffer, pipeline, offsets_it->second, set_index);
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

#endif