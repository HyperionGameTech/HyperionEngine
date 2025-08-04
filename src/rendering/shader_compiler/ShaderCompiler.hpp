/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HeapArray.hpp>

#include <core/math/Vertex.hpp>

#include <rendering/RenderShader.hpp>

#include <util/ini/INIFile.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

class Engine;

using ShaderPropertyFlags = uint32;

enum ShaderPropertyFlagBits : ShaderPropertyFlags
{
    SHADER_PROPERTY_FLAG_NONE = 0x0,
    SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE = 0x1
};

enum class ShaderLanguage
{
    GLSL,
    HLSL
};

enum class ProcessShaderSourcePhase
{
    BEFORE_PREPROCESS,
    AFTER_PREPROCESS
};

struct VertexAttributeDefinition
{
    String name;
    String typeClass;
    int location = -1;
    Optional<String> condition;
};

struct ShaderProperty
{
    using Value = Variant<String, int, float>;

    Name name;
    bool isPermutation;
    ShaderPropertyFlags flags;
    Value currentValue;
    Array<String, InlineAllocator<2>> enumValues;

    ShaderProperty()
        : isPermutation(false),
          flags(SHADER_PROPERTY_FLAG_NONE)
    {
    }

    ShaderProperty(Name name) = delete;

    ShaderProperty(Name name, bool isPermutation, ShaderPropertyFlags flags = SHADER_PROPERTY_FLAG_NONE)
        : name(name),
          isPermutation(isPermutation),
          flags(flags)
    {
    }

    ShaderProperty(Name name, bool isPermutation, const Value& currentValue, ShaderPropertyFlags flags = SHADER_PROPERTY_FLAG_NONE)
        : name(name),
          isPermutation(isPermutation),
          flags(flags),
          currentValue(currentValue)
    {
    }

    explicit ShaderProperty(VertexAttribute::Type vertexAttribute)
        : name(CreateNameFromDynamicString(ANSIString("HYP_ATTRIBUTE_") + VertexAttribute::mapping.Get(vertexAttribute).name)),
          isPermutation(false),
          flags(SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE),
          currentValue(Value(String(VertexAttribute::mapping.Get(vertexAttribute).name)))
    {
    }

    ShaderProperty(const ShaderProperty& other)
        : name(other.name),
          isPermutation(other.isPermutation),
          flags(other.flags),
          currentValue(other.currentValue),
          enumValues(other.enumValues)
    {
    }

    ShaderProperty& operator=(const ShaderProperty& other)
    {
        if (this == &other)
        {
            return *this;
        }

        name = other.name;
        isPermutation = other.isPermutation;
        flags = other.flags;
        currentValue = other.currentValue;
        enumValues = other.enumValues;

        return *this;
    }

    ShaderProperty(ShaderProperty&& other) noexcept
        : name(other.name),
          isPermutation(other.isPermutation),
          flags(other.flags),
          currentValue(std::move(other.currentValue)),
          enumValues(std::move(other.enumValues))
    {
        other.name = Name();
        other.isPermutation = false;
        other.flags = SHADER_PROPERTY_FLAG_NONE;
    }

    ShaderProperty& operator=(ShaderProperty&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        name = other.name;
        flags = other.flags;
        currentValue = std::move(other.currentValue);
        enumValues = std::move(other.enumValues);

        other.name = Name();
        other.isPermutation = false;
        other.flags = SHADER_PROPERTY_FLAG_NONE;

        return *this;
    }

    HYP_FORCE_INLINE bool operator==(const ShaderProperty& other) const
    {
        return name == other.name;
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderProperty& other) const
    {
        return name != other.name;
    }

    HYP_FORCE_INLINE bool operator==(const String& str) const
    {
        return name == str;
    }

    HYP_FORCE_INLINE bool operator!=(const String& str) const
    {
        return name != str;
    }

    HYP_FORCE_INLINE bool operator<(const ShaderProperty& other) const
    {
        if (name == other.name)
        {
            return false;
        }

        return std::strcmp(*name, *other.name) < 0;
    }

    HYP_FORCE_INLINE bool IsValueGroup() const
    {
        return enumValues.Any();
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return currentValue.IsValid();
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE ShaderPropertyFlags GetFlags() const
    {
        return flags;
    }

    HYP_FORCE_INLINE bool IsPermutable() const
    {
        return isPermutation;
    }

    HYP_FORCE_INLINE bool IsVertexAttribute() const
    {
        return flags & SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE;
    }

    HYP_FORCE_INLINE bool IsOptionalVertexAttribute() const
    {
        return IsVertexAttribute() && IsPermutable();
    }

    String GetValueString() const;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        // hashcode does not depend on any other properties than name.
        return name.GetHashCode();
    }

    HYP_FORCE_INLINE String ToString() const
    {
        return *name;
    }
};

class ShaderProperties
{
    friend class ShaderCompiler;

public:
    using Iterator = typename HashSet<ShaderProperty>::Iterator;
    using ConstIterator = typename HashSet<ShaderProperty>::ConstIterator;

    ShaderProperties()
        : m_needsHashCodeRecalculation(true)
    {
    }

    explicit ShaderProperties(const HashSet<ShaderProperty>& props)
        : m_needsHashCodeRecalculation(true)
    {
        for (const ShaderProperty& property : props)
        {
            Set(property);
        }
    }

    template <SizeType Sz>
    ShaderProperties(Name const (&props)[Sz])
        : m_needsHashCodeRecalculation(true)
    {
        for (Name propKey : props)
        {
            Set(ShaderProperty(propKey, true)); // default to permutable
        }
    }

    template <SizeType Sz>
    ShaderProperties(ShaderProperty const (&props)[Sz])
        : m_needsHashCodeRecalculation(true)
    {
        for (const ShaderProperty& property : props)
        {
            Set(property);
        }
    }

    template <SizeType Sz>
    ShaderProperties(const VertexAttributeSet& vertexAttributes, Name const (&props)[Sz])
        : m_requiredVertexAttributes(vertexAttributes),
          m_needsHashCodeRecalculation(true)
    {
        for (Name propKey : props)
        {
            m_props.Insert(ShaderProperty(propKey, true)); // default to permutable
        }
    }

    explicit ShaderProperties(const VertexAttributeSet& vertexAttributes)
        : m_requiredVertexAttributes(vertexAttributes),
          m_needsHashCodeRecalculation(true)
    {
    }

    ShaderProperties(const ShaderProperties& other) = default;
    ShaderProperties& operator=(const ShaderProperties& other) = default;
    ShaderProperties(ShaderProperties&& other) noexcept = default;
    ShaderProperties& operator=(ShaderProperties&& other) = default;
    ~ShaderProperties() = default;

    // HYP_FORCE_INLINE bool operator==(const ShaderProperties& other) const
    // {
    //     return (m_requiredVertexAttributes == other.m_requiredVertexAttributes) && (m_props == other.m_props);
    // }

    // HYP_FORCE_INLINE bool operator!=(const ShaderProperties& other) const
    // {
    //     return m_requiredVertexAttributes != other.m_requiredVertexAttributes || m_props != other.m_props;
    // }

    HYP_FORCE_INLINE bool operator==(const ShaderProperties& other) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ShaderProperties& other) const = delete;

    HYP_FORCE_INLINE bool Any() const
    {
        return m_props.Any();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_props.Empty();
    }

    HYP_FORCE_INLINE bool HasRequiredVertexAttributes(VertexAttributeSet vertexAttributes) const
    {
        return (m_requiredVertexAttributes & vertexAttributes) == vertexAttributes;
    }

    HYP_FORCE_INLINE bool HasRequiredVertexAttribute(VertexAttribute::Type vertexAttribute) const
    {
        return m_requiredVertexAttributes.Has(vertexAttribute);
    }

    HYP_FORCE_INLINE bool HasOptionalVertexAttributes(VertexAttributeSet vertexAttributes) const
    {
        return (m_optionalVertexAttributes & vertexAttributes) == vertexAttributes;
    }

    HYP_FORCE_INLINE bool HasOptionalVertexAttribute(VertexAttribute::Type vertexAttribute) const
    {
        return m_optionalVertexAttributes.Has(vertexAttribute);
    }

    HYP_FORCE_INLINE bool Has(WeakName name) const
    {
        // HashCode for WeakName("foo") MUST be equal to ShaderProperty's hashcode if the ShaderProperty's name is "foo"
        AssertDebug(ShaderProperty(NAME("Foo"), false).GetHashCode() == WeakName("Foo").GetHashCode());

        return m_props.FindByHashCode(name.GetHashCode()) != m_props.End();
    }

    HYP_API ShaderProperties& Set(const ShaderProperty& property, bool enabled = true);

    HYP_FORCE_INLINE ShaderProperties& Set(Name name, bool enabled = true)
    {
        return Set(ShaderProperty(name, true), enabled);
    }

    /*! \brief Applies \ref{other} properties onto this set */
    HYP_FORCE_INLINE void Merge(const ShaderProperties& other)
    {
        for (const ShaderProperty& property : other.m_props)
        {
            Set(property);
        }

        m_requiredVertexAttributes |= other.m_requiredVertexAttributes;
        m_optionalVertexAttributes |= other.m_optionalVertexAttributes;

        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE static ShaderProperties Merge(const ShaderProperties& a, const ShaderProperties& b)
    {
        ShaderProperties result(a);
        result.Merge(b);

        return result;
    }

    HYP_FORCE_INLINE const HashSet<ShaderProperty>& GetPropertySet() const
    {
        return m_props;
    }

    HYP_FORCE_INLINE VertexAttributeSet GetRequiredVertexAttributes() const
    {
        return m_requiredVertexAttributes;
    }

    HYP_FORCE_INLINE VertexAttributeSet GetOptionalVertexAttributes() const
    {
        return m_optionalVertexAttributes;
    }

    HYP_FORCE_INLINE void SetRequiredVertexAttributes(VertexAttributeSet vertexAttributes)
    {
        m_requiredVertexAttributes = vertexAttributes;
        m_optionalVertexAttributes = m_optionalVertexAttributes & ~m_requiredVertexAttributes;

        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE void SetOptionalVertexAttributes(VertexAttributeSet vertexAttributes)
    {
        m_optionalVertexAttributes = vertexAttributes & ~m_requiredVertexAttributes;
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_props.Size();
    }

    HYP_FORCE_INLINE Array<ShaderProperty> ToArray() const
    {
        return m_props.ToArray();
    }

    HYP_API String ToString(bool includeVertexAttributes = true) const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (m_needsHashCodeRecalculation)
        {
            RecalculateHashCode();

            m_needsHashCodeRecalculation = false;
        }

        return m_cachedHashCode;
    }

    HYP_FORCE_INLINE HashCode GetPropertySetHashCode() const
    {
        if (m_needsHashCodeRecalculation)
        {
            RecalculateHashCode();

            m_needsHashCodeRecalculation = false;
        }

        return m_cachedPropertySetHashCode;
    }

    HYP_DEF_STL_BEGIN_END(m_props.Begin(), m_props.End());

private:
    void RecalculateHashCode() const
    {
        HashCode hc;

        // NOTE: Intentionally left out m_optionalVertexAttributes,
        // as they do not impact the final instantiated version of the shader properties.

        m_cachedPropertySetHashCode = HashCode();

        Array<const ShaderProperty*> propsPtrs;
        propsPtrs.Reserve(m_props.Size());

        for (const ShaderProperty& property : m_props)
        {
            propsPtrs.PushBack(&property);
        }

        std::sort(propsPtrs.Begin(), propsPtrs.End(), [](const ShaderProperty* a, const ShaderProperty* b)
            {
                return *a < *b;
            });

        for (const ShaderProperty* pShaderProperty : propsPtrs)
        {
            m_cachedPropertySetHashCode.Add(pShaderProperty->GetHashCode());
        }

        hc.Add(m_requiredVertexAttributes.GetHashCode());
        hc.Add(m_cachedPropertySetHashCode);

        m_cachedHashCode = hc;
    }

    HYP_FORCE_INLINE ShaderProperties& AddPermutation(Name key)
    {
        const ShaderProperty shaderProperty(key, true);

        const auto it = m_props.Find(shaderProperty);

        if (it == m_props.End())
        {
            m_props.Insert(shaderProperty);
        }
        else
        {
            *it = shaderProperty;
        }

        m_needsHashCodeRecalculation = true;

        return *this;
    }

    HYP_FORCE_INLINE ShaderProperties& AddValueGroup(Name key, Span<const String> enumValues)
    {
        ShaderProperty shaderProperty(key, false);

        if (enumValues)
        {
            shaderProperty.enumValues = enumValues;
        }

        const auto it = m_props.Find(shaderProperty);

        if (it == m_props.End())
        {
            m_props.Insert(std::move(shaderProperty));
        }
        else
        {
            *it = std::move(shaderProperty);
        }

        m_needsHashCodeRecalculation = true;

        return *this;
    }

    HashSet<ShaderProperty> m_props;

    VertexAttributeSet m_requiredVertexAttributes;
    VertexAttributeSet m_optionalVertexAttributes;

    mutable HashCode m_cachedHashCode;
    mutable HashCode m_cachedPropertySetHashCode;
    mutable bool m_needsHashCodeRecalculation;
};

struct HashedShaderDefinition
{
    Name name;
    HashCode propertySetHash;
    VertexAttributeSet requiredVertexAttributes;

    HYP_FORCE_INLINE bool operator==(const HashedShaderDefinition& other) const
    {
        return name == other.name
            && propertySetHash == other.propertySetHash
            && requiredVertexAttributes == other.requiredVertexAttributes;
    }

    HYP_FORCE_INLINE bool operator!=(const HashedShaderDefinition& other) const
    {
        return name != other.name
            || propertySetHash != other.propertySetHash
            || requiredVertexAttributes != other.requiredVertexAttributes;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name.GetHashCode());
        hc.Add(requiredVertexAttributes.GetHashCode());
        hc.Add(propertySetHash);

        return hc;
    }
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

enum class DescriptorSetDeclarationFlags : uint8
{
    NONE = 0x0,
    REFERENCE = 0x1, // is this a reference to a global descriptor set declaration?
    TEMPLATE = 0x2   // is this descriptor set intended to be used as a template for other sets? (e.g material textures)
};

HYP_MAKE_ENUM_FLAGS(DescriptorSetDeclarationFlags)

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

using DescriptorUsageFlags = uint32;

enum DescriptorUsageFlagBits : DescriptorUsageFlags
{
    DESCRIPTOR_USAGE_FLAG_NONE = 0x0,
    DESCRIPTOR_USAGE_FLAG_DYNAMIC = 0x1
};

HYP_STRUCT()
struct DescriptorUsageType
{
    HYP_FIELD(Property = "Name", Serialize = true)
    Name name;

    HYP_FIELD(Property = "Size", Serialize = true)
    uint32 size = ~0u;

    HYP_FIELD(Property = "FieldNames", Serialize = true)
    Array<Name> fieldNames;

    HYP_FIELD(Property = "FieldTypes", Serialize = true)
    Array<DescriptorUsageType, DynamicAllocator> fieldTypes;

    DescriptorUsageType() = default;

    DescriptorUsageType(Name name, uint32 size = ~0u)
        : name(name),
          size(size)
    {
    }

    DescriptorUsageType(const DescriptorUsageType& other) = default;
    DescriptorUsageType& operator=(const DescriptorUsageType& other) = default;
    DescriptorUsageType(DescriptorUsageType&& other) noexcept = default;
    DescriptorUsageType& operator=(DescriptorUsageType&& other) noexcept = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return name.IsValid();
    }

    HYP_FORCE_INLINE bool HasExplicitSize() const
    {
        return size != ~0u;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        return size;
    }

    HYP_FORCE_INLINE Pair<Name, DescriptorUsageType&> AddField(Name fieldName, const DescriptorUsageType& type)
    {
        return Pair<Name, DescriptorUsageType&> { fieldNames.PushBack(fieldName), fieldTypes.PushBack(type) };
    }

    HYP_FORCE_INLINE Pair<Name, DescriptorUsageType&> GetField(SizeType index)
    {
        return { fieldNames[index], fieldTypes[index] };
    }

    HYP_FORCE_INLINE const Pair<Name, const DescriptorUsageType&> GetField(SizeType index) const
    {
        return { fieldNames[index], fieldTypes[index] };
    }

    HYP_FORCE_INLINE Optional<Pair<Name, DescriptorUsageType&>> FindField(WeakName fieldName)
    {
        for (SizeType i = 0; i < fieldNames.Size(); i++)
        {
            if (fieldNames[i] == fieldName)
            {
                return Pair<Name, DescriptorUsageType&> { fieldNames[i], fieldTypes[i] };
            }
        }

        return {};
    }

    HYP_FORCE_INLINE Optional<Pair<Name, const DescriptorUsageType&>> FindField(WeakName fieldName) const
    {
        for (SizeType i = 0; i < fieldNames.Size(); i++)
        {
            if (fieldNames[i] == fieldName)
            {
                return Pair<Name, const DescriptorUsageType&> { fieldNames[i], fieldTypes[i] };
            }
        }

        return {};
    }

    HYP_FORCE_INLINE bool operator<(const DescriptorUsageType& other) const
    {
        if (size != other.size)
        {
            return size < other.size;
        }

        if (fieldTypes.Size() != other.fieldTypes.Size())
        {
            return fieldTypes.Size() < other.fieldTypes.Size();
        }

        for (SizeType i = 0; i < fieldTypes.Size(); i++)
        {
            if (fieldTypes[i] != other.fieldTypes[i])
            {
                return fieldTypes[i] < other.fieldTypes[i];
            }
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const DescriptorUsageType& other) const
    {
        return name == other.name
            && size == other.size
            && fieldNames == other.fieldNames
            && fieldTypes == other.fieldTypes;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsageType& other) const
    {
        return name != other.name
            || size != other.size
            || fieldNames != other.fieldNames
            || fieldTypes != other.fieldTypes;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(size);
        hc.Add(fieldNames);
        hc.Add(fieldTypes);

        return hc;
    }
};

HYP_STRUCT()
struct DescriptorUsage
{
    HYP_FIELD(Property = "Slot", Serialize = true)
    DescriptorSlot slot;

    HYP_FIELD(Property = "SetName", Serialize = true)
    Name setName;

    HYP_FIELD(Property = "DescriptorName", Serialize = true)
    Name descriptorName;

    HYP_FIELD(Property = "Type", Serialize = true)
    DescriptorUsageType type;

    HYP_FIELD(Property = "Flags", Serialize = true)
    DescriptorUsageFlags flags;

    HYP_FIELD(Property = "Params", Serialize = true)
    HashMap<String, String> params;

    DescriptorUsage()
        : slot(DESCRIPTOR_SLOT_NONE),
          setName(Name::Invalid()),
          flags(DESCRIPTOR_USAGE_FLAG_NONE)
    {
    }

    DescriptorUsage(DescriptorSlot slot, Name setName, Name descriptorName, DescriptorUsageFlags flags = DESCRIPTOR_USAGE_FLAG_NONE, HashMap<String, String> params = {})
        : slot(slot),
          setName(setName),
          descriptorName(descriptorName),
          flags(flags),
          params(std::move(params))
    {
    }

    DescriptorUsage(const DescriptorUsage& other)
        : slot(other.slot),
          setName(other.setName),
          descriptorName(other.descriptorName),
          type(other.type),
          flags(other.flags),
          params(other.params)
    {
    }

    DescriptorUsage& operator=(const DescriptorUsage& other)
    {
        if (this == &other)
        {
            return *this;
        }

        slot = other.slot;
        setName = other.setName;
        descriptorName = other.descriptorName;
        type = other.type;
        flags = other.flags;
        params = other.params;

        return *this;
    }

    DescriptorUsage(DescriptorUsage&& other) noexcept
        : slot(other.slot),
          setName(std::move(other.setName)),
          descriptorName(std::move(other.descriptorName)),
          type(std::move(other.type)),
          flags(other.flags),
          params(std::move(other.params))
    {
    }

    DescriptorUsage& operator=(DescriptorUsage&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        slot = other.slot;
        setName = std::move(other.setName);
        descriptorName = std::move(other.descriptorName);
        type = std::move(other.type);
        flags = other.flags;
        params = std::move(other.params);

        return *this;
    }

    ~DescriptorUsage() = default;

    HYP_FORCE_INLINE bool operator==(const DescriptorUsage& other) const
    {
        return slot == other.slot
            && setName == other.setName
            && descriptorName == other.descriptorName
            && type == other.type
            && flags == other.flags
            && params == other.params;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsage& other) const
    {
        return slot != other.slot
            || setName != other.setName
            || descriptorName != other.descriptorName
            || type != other.type
            || flags != other.flags
            || params != other.params;
    }

    HYP_FORCE_INLINE bool operator<(const DescriptorUsage& other) const
    {
        if (slot != other.slot)
        {
            return slot < other.slot;
        }

        if (setName != other.setName)
        {
            return setName < other.setName;
        }

        if (descriptorName != other.descriptorName)
        {
            return descriptorName < other.descriptorName;
        }

        if (type != other.type)
        {
            return type < other.type;
        }

        if (flags != other.flags)
        {
            return flags < other.flags;
        }

        return false;
    }

    HYP_FORCE_INLINE uint32 GetCount() const
    {
        uint32 value = 1;

        auto it = params.Find("count");

        if (it == params.End())
        {
            return value;
        }

        if (StringUtil::Parse(it->second, &value))
        {
            return value;
        }

        return 1;
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        if (type.HasExplicitSize())
        {
            return type.size;
        }

        uint32 value = ~0u;

        auto it = params.Find("size");

        if (it == params.End())
        {
            return value;
        }

        if (StringUtil::Parse(it->second, &value))
        {
            return value;
        }

        return uint32(-1);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(slot);
        hc.Add(setName.GetHashCode());
        hc.Add(descriptorName.GetHashCode());
        hc.Add(type);
        hc.Add(flags);
        hc.Add(params.GetHashCode());

        return hc;
    }
};

struct DescriptorUsageSet
{
    FlatSet<DescriptorUsage> elements;

    DescriptorTableDeclaration BuildDescriptorTableDeclaration() const;

    HYP_FORCE_INLINE DescriptorUsage& operator[](SizeType index)
    {
        return elements[index];
    }

    HYP_FORCE_INLINE const DescriptorUsage& operator[](SizeType index) const
    {
        return elements[index];
    }

    HYP_FORCE_INLINE bool operator==(const DescriptorUsageSet& other) const
    {
        return elements == other.elements;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsageSet& other) const
    {
        return elements != other.elements;
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return elements.Size();
    }

    HYP_FORCE_INLINE void Add(const DescriptorUsage& descriptorUsage)
    {
        elements.Insert(descriptorUsage);
    }

    HYP_FORCE_INLINE DescriptorUsage* Find(WeakName descriptorName)
    {
        auto it = elements.FindIf([descriptorName](const DescriptorUsage& descriptorUsage)
            {
                return descriptorUsage.descriptorName == descriptorName;
            });

        if (it == elements.End())
        {
            return nullptr;
        }

        return it;
    }

    HYP_FORCE_INLINE const DescriptorUsage* Find(WeakName descriptorName) const
    {
        return const_cast<const DescriptorUsageSet*>(this)->Find(descriptorName);
    }

    HYP_FORCE_INLINE void Merge(const Array<DescriptorUsage>& other)
    {
        elements.Merge(other);
    }

    HYP_FORCE_INLINE void Merge(Array<DescriptorUsage>&& other)
    {
        elements.Merge(std::move(other));
    }

    HYP_FORCE_INLINE void Merge(const DescriptorUsageSet& other)
    {
        elements.Merge(other.elements);
    }

    HYP_FORCE_INLINE void Merge(DescriptorUsageSet&& other)
    {
        elements.Merge(std::move(other.elements));
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return elements.GetHashCode();
    }
};

struct ShaderDefinition
{
    Name name;
    ShaderProperties properties;

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE ShaderProperties& GetProperties()
    {
        return properties;
    }

    HYP_FORCE_INLINE const ShaderProperties& GetProperties() const
    {
        return properties;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return name.IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return name.IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const ShaderDefinition& other) const
    {
        return GetHashCode() == other.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator!=(const ShaderDefinition& other) const
    {
        return GetHashCode() != other.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator<(const ShaderDefinition& other) const
    {
        return GetHashCode() < other.GetHashCode();
    }

    HYP_FORCE_INLINE explicit operator HashedShaderDefinition() const
    {
        return HashedShaderDefinition { name, properties.GetPropertySetHashCode(), properties.GetRequiredVertexAttributes() };
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        // ensure they return the same hash codes so they can be compared.
        return (operator HashedShaderDefinition()).GetHashCode();
    }
};

HYP_STRUCT()

struct CompiledShader
{
    HYP_FIELD(Property = "Definition", Serialize = false) // custom serialization used
    ShaderDefinition definition;

    HYP_FIELD(Property = "DescriptorTableDeclaration", Serialize = false, Transient = true) // built after load, not serialized
    DescriptorTableDeclaration descriptorTableDeclaration;

    HYP_FIELD(Property = "DescriptorUsageSet", Serialize = true)
    DescriptorUsageSet descriptorUsageSet;

    HYP_FIELD(Property = "EntryPointName", Serialize = true)
    String entryPointName = "main";

    HYP_FIELD(Property = "Modules", Serialize = false) // custom serialization used
    HeapArray<ByteBuffer, SMT_MAX> modules;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return definition.IsValid()
            && AnyOf(modules, &ByteBuffer::Any);
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return definition.name;
    }

    HYP_FORCE_INLINE ShaderDefinition& GetDefinition()
    {
        return definition;
    }

    HYP_FORCE_INLINE const ShaderDefinition& GetDefinition() const
    {
        return definition;
    }

    HYP_FORCE_INLINE const DescriptorTableDeclaration& GetDescriptorTableDeclaration() const
    {
        return descriptorTableDeclaration;
    }

    HYP_FORCE_INLINE const String& GetEntryPointName() const
    {
        return entryPointName;
    }

    HYP_FORCE_INLINE const ShaderProperties& GetProperties() const
    {
        return definition.properties;
    }

    HYP_FORCE_INLINE const HeapArray<ByteBuffer, SMT_MAX>& GetModules() const
    {
        return modules;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(definition.GetHashCode());
        hc.Add(modules.GetHashCode());

        return hc;
    }
};

struct CompiledShaderBatch
{
    Array<CompiledShader> compiledShaders;
    Array<String> errorMessages;

    CompiledShaderBatch() = default;

    CompiledShaderBatch(const CompiledShaderBatch& other)
        : compiledShaders(other.compiledShaders),
          errorMessages(other.errorMessages)
    {
    }

    CompiledShaderBatch& operator=(const CompiledShaderBatch& other)
    {
        if (this == &other)
        {
            return *this;
        }

        compiledShaders = other.compiledShaders;
        errorMessages = other.errorMessages;

        return *this;
    }

    CompiledShaderBatch(CompiledShaderBatch&& other) noexcept
        : compiledShaders(std::move(other.compiledShaders)),
          errorMessages(std::move(other.errorMessages))
    {
    }

    CompiledShaderBatch& operator=(CompiledShaderBatch&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        compiledShaders = std::move(other.compiledShaders);
        errorMessages = std::move(other.errorMessages);

        return *this;
    }

    ~CompiledShaderBatch() = default;

    HYP_FORCE_INLINE bool HasErrors() const
    {
        return errorMessages.Any();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return compiledShaders.GetHashCode();
    }
};

class ShaderCache
{
public:
    ShaderCache() = default;
    ShaderCache(const ShaderCache& other) = delete;
    ShaderCache& operator=(const ShaderCache& other) = delete;
    ShaderCache(ShaderCache&& other) noexcept = delete;
    ShaderCache& operator=(ShaderCache&& other) noexcept = delete;
    ~ShaderCache() = default;

    bool Get(Name name, CompiledShaderBatch& out) const
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_compiledShaders.Find(name);

        if (it == m_compiledShaders.End())
        {
            return false;
        }

        out = it->second;

        return true;
    }

    bool GetShaderInstance(Name name, uint64 versionHash, CompiledShader& out) const
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_compiledShaders.Find(name);

        if (it == m_compiledShaders.End())
        {
            return false;
        }

        const auto versionIt = it->second.compiledShaders.FindIf([versionHash](const CompiledShader& item)
            {
                return item.definition.properties.GetHashCode().Value() == versionHash;
            });

        if (versionIt == it->second.compiledShaders.End())
        {
            return false;
        }

        out = *versionIt;

        return true;
    }

    void Set(Name name, const CompiledShaderBatch& batch)
    {
        Mutex::Guard guard(m_mutex);

        m_compiledShaders.Set(name, batch);
    }

    void Set(Name name, CompiledShaderBatch&& batch)
    {
        Mutex::Guard guard(m_mutex);

        m_compiledShaders.Set(name, std::move(batch));
    }

    void Remove(Name name)
    {
        Mutex::Guard guard(m_mutex);

        m_compiledShaders.Erase(name);
    }

private:
    HashMap<Name, CompiledShaderBatch> m_compiledShaders;
    mutable Mutex m_mutex;
};

class ShaderCompiler
{
    static constexpr SizeType maxPermutations = 2048; // temporalily increased, until some properties are "always enabled"

    struct ProcessError
    {
        String errorMessage;
    };

    struct ProcessResult
    {
        String processedSource;
        Array<ProcessError> errors;
        Array<VertexAttributeDefinition> requiredAttributes;
        Array<VertexAttributeDefinition> optionalAttributes;
        Array<DescriptorUsage> descriptorUsages;

        ProcessResult() = default;
        ProcessResult(const ProcessResult& other) = default;
        ProcessResult& operator=(const ProcessResult& other) = default;
        ProcessResult(ProcessResult&& other) noexcept = default;
        ProcessResult& operator=(ProcessResult&& other) noexcept = default;
        ~ProcessResult() = default;
    };

public:
    struct SourceFile
    {
        String path;

        HashCode GetHashCode() const
        {
            HashCode hc;
            hc.Add(path);

            return hc;
        }
    };

    struct Bundle // combination of shader files, .frag, .vert etc. in .ini definitions file.
    {
        Name name;
        String entryPointName = "main";
        FlatMap<ShaderModuleType, SourceFile> sources;
        ShaderProperties versions; // permutations

        bool HasRTShaders() const
        {
            return AnyOf(sources, [](const KeyValuePair<ShaderModuleType, SourceFile>& item)
                {
                    return IsRaytracingShaderModule(item.first);
                });
        }

        bool IsComputeShader() const
        {
            return Every(sources, [](const KeyValuePair<ShaderModuleType, SourceFile>& item)
                {
                    return item.first == SMT_COMPUTE;
                });
        }

        bool HasVertexShader() const
        {
            return AnyOf(sources, [](const KeyValuePair<ShaderModuleType, SourceFile>& item)
                {
                    return item.first == SMT_VERTEX;
                });
        }
    };

    ShaderCompiler();
    ShaderCompiler(const ShaderCompiler& other) = delete;
    ShaderCompiler& operator=(const ShaderCompiler& other) = delete;
    ~ShaderCompiler();

    HYP_API bool CanCompileShaders() const;
    HYP_API bool LoadShaderDefinitions(bool precompileShaders = false);

    HYP_API CompiledShader GetCompiledShader(Name name);
    HYP_API CompiledShader GetCompiledShader(Name name, const ShaderProperties& properties);

    HYP_API bool GetCompiledShader(
        Name name,
        const ShaderProperties& properties,
        CompiledShader& out);

private:
    void GetPlatformSpecificProperties(
        ShaderProperties& out) const;

    ProcessResult ProcessShaderSource(
        ProcessShaderSourcePhase phase,
        ShaderModuleType type,
        ShaderLanguage language,
        const String& source,
        const String& filename,
        const ShaderProperties& properties);

    void ParseDefinitionSection(
        const INIFile::Section& section,
        Bundle& bundle);

    bool CompileBundle(
        Bundle& bundle,
        const ShaderProperties& additionalVersions,
        CompiledShaderBatch& out);

    bool HandleCompiledShaderBatch(
        Bundle& bundle,
        const ShaderProperties& additionalVersions,
        const FilePath& outputFilePath,
        CompiledShaderBatch& batch);

    bool LoadOrCompileBatch(
        Name name,
        const ShaderProperties& additionalVersions,
        CompiledShaderBatch& out);

    INIFile* m_definitions;
    ShaderCache m_cache;
    Array<Bundle> m_bundles;
};

} // namespace hyperion
