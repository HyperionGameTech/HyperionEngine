/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>

#include <core/utilities/Optional.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/Range.hpp>

#include <core/Defines.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderGpuImageView.hpp>
#include <rendering/RenderSampler.hpp>

#include <rendering/rt/RenderAccelerationStructure.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/Types.hpp>
#include <core/HashCode.hpp>

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

struct DescriptorSetOffsetMap
{
    static constexpr uint32 g_maxOffsets = 8;

    WeakName keys[g_maxOffsets];
    uint32 values[g_maxOffsets];
    uint32 count;

    DescriptorSetOffsetMap()
        : keys(),
          values(),
          count(0)
    {
    }

    DescriptorSetOffsetMap(std::initializer_list<Pair<const char*, uint32>> v)
        : keys(),
          values(),
          count(v.size())
    {
        AssertDebug(v.size() <= g_maxOffsets, "too many values provided to constructor!");

        for (auto it = v.begin(); it != v.end(); ++it)
        {
            keys[it - v.begin()] = it->first;
            values[it - v.begin()] = it->second;
        }
    }

    template <SizeType Count>
    DescriptorSetOffsetMap(Pair<const char*, uint32> const (&kv)[Count])
        : keys(),
          values(),
          count(Count)
    {
        static_assert(Count <= g_maxOffsets, "too many values provided to constructor!");

        for (uint32 i = 0; i < Count; i++)
        {
            keys[i] = kv[i].first;
            values[i] = kv[i].second;
        }
    }

    HYP_FORCE_INLINE void Add(WeakName key, uint32 value)
    {
        uint32 idx = count++;
        AssertDebug(idx < g_maxOffsets, "too many offsets!");

        keys[idx] = key;
        values[idx] = value;
    }
};

struct DescriptorTableOffsetMap
{
    static constexpr uint32 g_maxSets = 4;
    
    WeakName setNames[g_maxSets];
    DescriptorSetOffsetMap setOffsets[g_maxSets];
    uint32 count;

    DescriptorTableOffsetMap()
        : setNames(),
          setOffsets(),
          count(0)
    {
    }

    DescriptorTableOffsetMap(std::initializer_list<Pair<const char*, DescriptorSetOffsetMap>> v)
        : setNames(),
          setOffsets(),
          count(v.size())
    {
        AssertDebug(v.size() <= g_maxSets, "too many values provided to constructor!");

        for (auto it = v.begin(); it != v.end(); ++it)
        {
            setNames[it - v.begin()] = it->first;
            setOffsets[it - v.begin()] = it->second;
        }
    }

    template <SizeType Count>
    DescriptorTableOffsetMap(Pair<const char*, DescriptorSetOffsetMap> const (&v)[Count])
        : setNames(),
          setOffsets(),
          count(Count)
    {
        static_assert(Count <= g_maxSets, "too many values provided to constructor!");

        for (uint32 i = 0; i < Count; i++)
        {
            setNames[i] = v[i].first;
            setOffsets[i] = v[i].second;
        }
    }

    HYP_FORCE_INLINE DescriptorSetOffsetMap& Add(WeakName setName)
    {
        uint32 idx = count++;
        AssertDebug(idx < g_maxSets, "too many offsets!");

        setNames[idx] = setName;

        return setOffsets[count];
    }
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
struct DescriptorSetElementTypeInfo<GpuImageViewBase>
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
    bool isDynamic = false;

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
        hc.Add(isDynamic);
        hc.Add(index);

        // cond excluded intentionally

        return hc;
    }
};

HYP_STRUCT()
struct DescriptorSetDeclaration
{
    HYP_FIELD(Property = "SetIndex", Serialize = true)
    uint32 setIndex = ~0u;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name name = Name::Invalid();

    HYP_FIELD(Property = "Slots", Serialize = true)
    FixedArray<Array<DescriptorDeclaration, DynamicAllocator>, DESCRIPTOR_SLOT_MAX> slots = {};

    HYP_FIELD(Property = "Flags", Serialize = true)
    EnumFlags<DescriptorSetDeclarationFlags> flags = DescriptorSetDeclarationFlags::NONE;

    DescriptorSetDeclaration() = default;

    DescriptorSetDeclaration(uint32 setIndex, Name name)
        : setIndex(setIndex),
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
        AssertDebug(decl.slot != DESCRIPTOR_SLOT_NONE && decl.slot < DESCRIPTOR_SLOT_MAX);

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

        hc.Add(setIndex);
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
    DescriptorSetDeclaration* AddDescriptorSetDeclaration(DescriptorSetDeclaration&& descriptorSetDeclaration);

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE uint32 GetDescriptorSetIndex(WeakName name) const
    {
        for (const auto& it : elements)
        {
            if (it.name == name)
            {
                return it.setIndex;
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
        DeclareSet(DescriptorTableDeclaration* table, uint32 setIndex, Name name, bool isTemplate = false)
        {
            AssertDebug(table != nullptr);

            if (table->elements.Size() <= setIndex)
            {
                table->elements.Resize(setIndex + 1);
            }

            DescriptorSetDeclaration& decl = table->elements[setIndex];
            decl.setIndex = setIndex;
            decl.name = name;

            if (isTemplate)
            {
                decl.flags |= DescriptorSetDeclarationFlags::TEMPLATE;
            }
        }
    };

    struct DeclareDescriptor
    {
        DeclareDescriptor(DescriptorTableDeclaration* table, Name setName, DescriptorSlot slotType, Name descriptorName, DescriptorDeclaration::ConditionFunction cond = nullptr, uint32 count = 1, uint32 size = ~0u, bool isDynamic = false)
        {
            AssertDebug(table != nullptr);

            uint32 setIndex = ~0u;

            for (SizeType i = 0; i < table->elements.Size(); ++i)
            {
                if (table->elements[i].name == setName)
                {
                    setIndex = uint32(i);
                    break;
                }
            }

            AssertDebug(setIndex != ~0u, "Descriptor set {} not found", setName);

            DescriptorSetDeclaration& descriptorSetDecl = table->elements[setIndex];
            AssertDebug(descriptorSetDecl.setIndex == setIndex);
            AssertDebug(slotType > 0 && slotType < descriptorSetDecl.slots.Size());

            const uint32 slotTypeIndex = uint32(slotType) - 1;

            const uint32 slotIndex = uint32(descriptorSetDecl.slots[slotTypeIndex].Size());

            DescriptorDeclaration& descriptorDecl = descriptorSetDecl.slots[slotTypeIndex].EmplaceBack();
            descriptorDecl.index = slotIndex;
            descriptorDecl.slot = slotType;
            descriptorDecl.name = descriptorName;
            descriptorDecl.cond = cond;
            descriptorDecl.size = size;
            descriptorDecl.count = count;
            descriptorDecl.isDynamic = isDynamic;
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
          m_isTemplate(other.m_isTemplate),
          m_isReference(other.m_isReference),
          m_elements(other.m_elements),
          m_dynamicElements(other.m_dynamicElements)
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

        return *this;
    }

    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
        : m_decl(other.m_decl),
          m_isTemplate(other.m_isTemplate),
          m_isReference(other.m_isReference),
          m_elements(std::move(other.m_elements)),
          m_dynamicElements(std::move(other.m_dynamicElements))
    {
        other.m_decl = nullptr;
        other.m_isTemplate = false;
        other.m_isReference = false;
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

        other.m_decl = nullptr;
        other.m_isTemplate = false;
        other.m_isReference = false;

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

    HYP_FORCE_INLINE const DescriptorSetLayoutElement* GetElement(WeakName name) const
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
    bool m_isTemplate : 1 = false;  // is this descriptor set a template for other sets? (e.g material textures)
    bool m_isReference : 1 = false; // is this descriptor set a reference to a global set? (e.g global material textures)
    HashMap<Name, DescriptorSetLayoutElement> m_elements;
    Array<Name> m_dynamicElements;
};

struct DescriptorSetElement
{
    using ValueType = Variant<GpuBufferRef, GpuImageViewRef, SamplerRef, TLASRef>;

    FlatMap<uint32, ValueType> values;
    Range<uint32> dirtyRange {};

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
                    SafeDelete(std::move(ref));
                });
        }
    }

    HYP_FORCE_INLINE bool IsDirty() const
    {
        return bool(dirtyRange);
    }
};

HYP_CLASS(Abstract, NoScriptBindings)
class DescriptorSetBase : public HypObjectBase
{
    HYP_OBJECT_BODY(DescriptorSetBase);

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

    bool HasElement(WeakName name) const;

    void SetElement(WeakName name, uint32 index, uint32 bufferSize, const GpuBufferRef& ref);
    void SetElement(WeakName name, uint32 index, const GpuBufferRef& ref);
    void SetElement(WeakName name, const GpuBufferRef& ref);

    void SetElement(WeakName name, uint32 index, const GpuImageViewRef& ref);
    void SetElement(WeakName name, const GpuImageViewRef& ref);

    void SetElement(WeakName name, uint32 index, const SamplerRef& ref);
    void SetElement(WeakName name, const SamplerRef& ref);

    void SetElement(WeakName name, uint32 index, const TLASRef& ref);
    void SetElement(WeakName name, const TLASRef& ref);

    virtual void Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, uint32 bindIndex) const = 0;
    virtual void Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const = 0;

protected:
    DescriptorSetBase(const DescriptorSetLayout& layout)
        : m_layout(layout)
    {
    }

    template <class T>
    DescriptorSetElement& SetElementT(WeakName name, uint32 index, const Handle<T>& ref)
    {
        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
        AssertDebug(layoutElement != nullptr, "Invalid element: No item with name {} found", Name(name));

        // Range check
        AssertDebug(index < layoutElement->count, "Index {} out of range for element {} with count {}",
            index, Name(name), layoutElement->count);

        if constexpr (std::is_base_of_v<GpuBufferBase, T>)
        {
            static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER))
                | (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC))
                | (1u << uint32(DescriptorSetElementType::SSBO))
                | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC));

            AssertDebug(mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", Name(name));

            if (ref != nullptr)
            {
                // Buffer type check, to make sure the buffer type is allowed for the given element
                const GpuBufferType bufferType = ref->GetBufferType();

                AssertDebug(
                    (descriptorSetElementTypeToBufferType[uint32(layoutElement->type)] & (1u << uint32(bufferType))),
                    "Buffer type {} is not in the allowed types for element {}",
                    uint32(bufferType), Name(name));

                if (layoutElement->size != 0 && layoutElement->size != ~0u)
                {
                    const uint32 remainder = ref->Size() % layoutElement->size;

                    AssertDebug(
                        remainder == 0,
                        "Buffer size ({}) is not a multiplier of layout size ({}) for element {}",
                        ref->Size(), layoutElement->size, Name(name));
                }
            }
        }
        else if constexpr (std::is_base_of_v<GpuImageViewBase, T>)
        {
            static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::IMAGE))
                | (1u << uint32(DescriptorSetElementType::IMAGE_STORAGE));

            AssertDebug(mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", Name(name));
        }
        else if constexpr (std::is_base_of_v<SamplerBase, T>)
        {
            static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::SAMPLER));

            AssertDebug(mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", Name(name));
        }
        else if constexpr (std::is_base_of_v<TLASBase, T>)
        {
            static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::TLAS));

            AssertDebug(mask & (1u << uint32(layoutElement->type)), "Layout type for {} does not match given type", Name(name));
        }
        else
        {
            static_assert(resolutionFailure<T>, "Unsupported type for descriptor set element");
        }

        auto it = m_elements.FindAs(name);
        AssertDebug(it != m_elements.End());

        if (it == m_elements.End())
        {
            it = m_elements.Emplace(Name(name)).first;
        }

        DescriptorSetElement& element = it->second;

        auto elementIt = element.values.Find(index);

        if (elementIt == element.values.End())
        {
            elementIt = element.values.Emplace(index, Handle<T>(ref)).first;
        }
        else
        {
            Handle<T>* currentValue = elementIt->second.template TryGet<Handle<T>>();

            if (currentValue)
            {
                SafeDelete(std::move(*currentValue));
            }

            elementIt->second.template Set<Handle<T>>(ref);
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

        auto it = m_elements.FindAs(name);

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

    Name m_debugName;

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    HashSet<FrameWeakRef> m_currentFrames; // frames that are currently using this descriptor set
#endif
};

HYP_CLASS(Abstract, NoScriptBindings)
class DescriptorTableBase : public HypObjectBase
{
    HYP_OBJECT_BODY(DescriptorTableBase);

public:
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

    HYP_FORCE_INLINE const FixedArray<Array<DescriptorSetRef>, g_framesInFlight>& GetSets() const
    {
        return m_sets;
    }

    /*! \brief Get a descriptor set from the table
        \param name The name of the descriptor set
        \param frameIndex The index of the frame for the descriptor set
        \return The descriptor set, or an unset reference if not found */
    HYP_FORCE_INLINE const DescriptorSetRef& GetDescriptorSet(WeakName name, uint32 frameIndex) const
    {
        for (const DescriptorSetRef& set : m_sets[frameIndex])
        {
            if (set->GetLayout().GetName() == name)
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
            if (!set->GetLayout().IsValid())
            {
                continue;
            }

            if (set->GetLayout().GetDeclaration()->setIndex == descriptorSetIndex)
            {
                return set;
            }
        }

        return DescriptorSetRef::empty;
    }

    /*! \brief Get the index of a descriptor set in the table
        \param name The name of the descriptor set
        \return The index of the descriptor set in the table, or -1 if not found */
    HYP_FORCE_INLINE uint32 GetDescriptorSetIndex(WeakName name) const
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

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            for (const DescriptorSetRef& set : m_sets[frameIndex])
            {
                const Name descriptorSetName = set->GetLayout().GetName();

                // use FindDescriptorSetDeclaration rather than `set->GetLayout().GetDeclaration()`, since we need to know
                // if the descriptor set is a reference to a global set
                DescriptorSetDeclaration* decl = m_decl->FindDescriptorSetDeclaration(descriptorSetName);
                AssertDebug(decl != nullptr);

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

    /*! \brief Apply updates to all descriptor sets in the table
        \param frameIndex The index of the frame to update the descriptor sets for
        \param force If true, will update descriptor sets even if they are not marked as dirty
        \return The result of the operation */
    void Update(uint32 frameIndex, bool force = false)
    {
        if (!IsValid())
        {
            return;
        }

        for (const DescriptorSetRef& set : m_sets[frameIndex])
        {
            const DescriptorSetLayout& layout = set->GetLayout();

            if (layout.IsReference() || layout.IsTemplate())
            {
                // references are updated elsewhere
                // template descriptor sets are not updated (no handle to update)
                continue;
            }

            bool isDirty = false;
            set->UpdateDirtyState(&isDirty);

            if (!isDirty && !force)
            {
                continue;
            }

            set->Update(force);
        }
    }

    /*! \brief Bind all descriptor sets in the table
        \param commandBuffer The command buffer to push the bind commands to
        \param frameIndex The index of the frame to bind the descriptor sets for
        \param pipeline The pipeline to bind the descriptor sets to
        \param offsets The offsets to bind dynamic descriptor sets with */
    template <class PipelineType>
    void Bind(CommandBufferBase* commandBuffer, uint32 frameIndex, PipelineType* pipeline, const DescriptorTableOffsetMap& offsets) const
    {
        HYP_GFX_ASSERT(commandBuffer != nullptr, "Command buffer is null");
        HYP_GFX_ASSERT(pipeline != nullptr && pipeline->IsCreated(), "Pipeline is null or not created");

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

            const Name descriptorSetName = set->GetLayout().GetName();

            const uint32 setIndex = GetDescriptorSetIndex(descriptorSetName);

            if (set->GetLayout().GetDynamicElements().Any() && offsets.count != 0)
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
                    set->Bind(commandBuffer, pipeline, offsets.setOffsets[offsetIdx], setIndex);

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

    Name m_debugName;
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
