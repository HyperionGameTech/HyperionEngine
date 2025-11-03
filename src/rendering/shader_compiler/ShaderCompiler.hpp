/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HashSet.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/Variant.hpp>

#include <core/math/Vertex.hpp>

#include <rendering/RenderShader.hpp>

#include <util/ini/INIFile.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace hyperion {

struct DescriptorTableDeclaration;

enum DescriptorSlot : uint32;

HYP_ENUM()
enum ShaderPropertyFlags : uint32
{
    SPF_NONE = 0x0,
    SPF_VERTEX_ATTRIBUTE = 0x1,
    SPF_PERMUTATION = 0x2
};

HYP_ENUM()
enum class ShaderLanguage : uint32
{
    GLSL,
    HLSL
};

HYP_ENUM()
enum class ProcessShaderSourcePhase : uint32
{
    BEFORE_PREPROCESS,
    AFTER_PREPROCESS
};

HYP_STRUCT()
struct VertexAttributeDefinition
{
    HYP_STRUCT_BODY(VertexAttributeDefinition);

    HYP_FIELD()
    String name;

    HYP_FIELD()
    String typeClass;

    HYP_FIELD()
    int location = -1;

    HYP_FIELD()
    String condition;
};

HYP_STRUCT()
struct ShaderProperty
{
    HYP_STRUCT_BODY(ShaderProperty);

    using Value = Variant<String, int, float>;

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    ShaderPropertyFlags flags;

    HYP_FIELD()
    Value currentValue;

    HYP_FIELD()
    Array<Value> enumValues;

    ShaderProperty()
        : flags(SPF_NONE)
    {
    }

    ShaderProperty(Name name, ShaderPropertyFlags flags = SPF_NONE)
        : name(name),
          flags(flags)
    {
    }

    ShaderProperty(Name name, const Value& currentValue, ShaderPropertyFlags flags = SPF_NONE)
        : name(name),
          flags(flags),
          currentValue(currentValue)
    {
    }

    explicit ShaderProperty(VertexAttribute::Type vertexAttribute)
        : name(CreateNameFromDynamicString(ANSIString("HYP_ATTRIBUTE_") + VertexAttribute::mapping.Get(vertexAttribute).name)),
          flags(SPF_VERTEX_ATTRIBUTE),
          currentValue(Value(String(VertexAttribute::mapping.Get(vertexAttribute).name)))
    {
    }

    ShaderProperty(const ShaderProperty& other)
        : name(other.name),
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
        flags = other.flags;
        currentValue = other.currentValue;
        enumValues = other.enumValues;

        return *this;
    }

    ShaderProperty(ShaderProperty&& other) noexcept
        : name(other.name),
          flags(other.flags),
          currentValue(std::move(other.currentValue)),
          enumValues(std::move(other.enumValues))
    {
        other.name = Name();
        other.flags = SPF_NONE;
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
        other.flags = SPF_NONE;

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
        return flags & SPF_PERMUTATION;
    }

    HYP_FORCE_INLINE bool IsStatic() const
    {
        return !IsPermutable() && !IsValueGroup();
    }

    HYP_FORCE_INLINE bool IsVertexAttribute() const
    {
        return flags & SPF_VERTEX_ATTRIBUTE;
    }

    HYP_FORCE_INLINE bool IsOptionalVertexAttribute() const
    {
        return IsVertexAttribute() && IsPermutable();
    }

    HYP_FORCE_INLINE void AddEnumValue(const Value& enumValue)
    {
        if (!enumValues.Contains(enumValue))
        {
            enumValues.PushBack(enumValue);
        }
    }

    HYP_API String GetValueString() const;

    HYP_API HashCode GetHashCode() const;

    HYP_API String ToString() const;
};

HYP_STRUCT()
class ShaderProperties
{
    friend class ShaderCompiler;

public:
    HYP_STRUCT_BODY(ShaderProperties);

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
            Set(property, true);
        }
    }

    template <SizeType Sz>
    ShaderProperties(Name const (&props)[Sz])
        : m_needsHashCodeRecalculation(true)
    {
        for (Name propKey : props)
        {
            Set(ShaderProperty(propKey, SPF_PERMUTATION), true); // default to permutable
        }
    }

    template <SizeType Sz>
    ShaderProperties(ShaderProperty const (&props)[Sz])
        : m_needsHashCodeRecalculation(true)
    {
        for (const ShaderProperty& property : props)
        {
            Set(property, true);
        }
    }

    template <SizeType Sz>
    ShaderProperties(const VertexAttributeSet& vertexAttributes, Name const (&props)[Sz])
        : m_requiredVertexAttributes(vertexAttributes),
          m_needsHashCodeRecalculation(true)
    {
        for (Name propKey : props)
        {
            m_props.Insert(ShaderProperty(propKey, SPF_PERMUTATION)); // default to permutable
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

    HYP_FORCE_INLINE Iterator Find(const ShaderProperty& property)
    {
        return m_props.Find(property);
    }

    HYP_FORCE_INLINE Iterator Find(WeakName name)
    {
        return m_props.FindIf([name](const ShaderProperty& other)
            {
                return other.name == name;
            });
    }

    HYP_FORCE_INLINE ConstIterator Find(const ShaderProperty& property) const
    {
        return const_cast<ShaderProperties*>(this)->Find(property);
    }

    HYP_FORCE_INLINE ConstIterator Find(WeakName name) const
    {
        return const_cast<ShaderProperties*>(this)->Find(name);
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
        return m_props.FindByHashCode(name.GetHashCode()) != m_props.End();
    }

    HYP_API ShaderProperties& Set(const ShaderProperty& property, bool enabled = true);

    HYP_FORCE_INLINE ShaderProperties& Set(Name name, bool enabled, ShaderPropertyFlags flags = SPF_NONE)
    {
        return Set(ShaderProperty(name, flags), enabled);
    }

    /*! \brief Applies \ref{other} properties onto this set */
    HYP_FORCE_INLINE void Merge(const ShaderProperties& other)
    {
        for (const ShaderProperty& property : other.m_props)
        {
            Set(property, true);
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

    /*! \brief Adds a new permutation shader property
     *  Permutations create new shader variants based on their values.
     *  Many permutations will drastically increase the number of shader variants generated,
     *  so use them sparingly. (prefer value groups or static properties where appropriate) */
    ShaderProperties& AddPermutation(Name key)
    {
        const ShaderProperty shaderProperty(key, SPF_PERMUTATION);

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

    /*! \brief Adds a new static property with key \ref{key}
     *  Static properties are applied to every shader variant and do not create new permutations. */
    ShaderProperties& AddStatic(Name key)
    {
        const ShaderProperty shaderProperty(key, SPF_NONE);

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

    /*! \brief Adds a new value group property with key \ref{key} and possible enum values \ref{enumValues}
     *  Value groups create new shader variants but their values are mututally exclusive to each other.
     *  i.e, only one value from the value group can be selected at a time. This reduces the number of
     *  shader variants generated compared to permutations. */
    ShaderProperties& AddValueGroup(Name key, const Array<ShaderProperty::Value>& enumValues)
    {
        ShaderProperty shaderProperty(key, SPF_NONE);

        if (enumValues.Any())
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

    HYP_FORCE_INLINE VertexAttributeSet GetRequiredVertexAttributes() const
    {
        return m_requiredVertexAttributes;
    }

    HYP_FORCE_INLINE VertexAttributeSet GetOptionalVertexAttributes() const
    {
        return m_optionalVertexAttributes;
    }

    HYP_FORCE_INLINE VertexAttributeSet GetAllVertexAttributes() const
    {
        return m_requiredVertexAttributes | m_optionalVertexAttributes;
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

    HYP_API String ToString() const;

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

        // NOTE: Intentionally left out m_optionalVertexAttributes
        // as they do not impact the final instantiated version of the shader properties.
        // m_requiredVertexAttributes needs to be checked by the caller as we could have
        // shader with less vertex attributes than the mesh in question has.

        m_cachedPropertySetHashCode = HashCode();

        Array<const ShaderProperty*> propsPtrs;
        propsPtrs.Reserve(m_props.Size());

        for (const ShaderProperty& property : m_props)
        {
            propsPtrs.PushBack(&property);
        }

        std::sort(propsPtrs.Begin(), propsPtrs.End(), [](const ShaderProperty* a, const ShaderProperty* b)
            {
                // sort by name to ensure consistent hashcode
                return std::strcmp(*a->name, *b->name) < 0;
            });

        for (const ShaderProperty* pShaderProperty : propsPtrs)
        {
            m_cachedPropertySetHashCode.Add(pShaderProperty->GetHashCode());
        }

        hc.Add(m_cachedPropertySetHashCode);

        m_cachedHashCode = hc;
    }

    HYP_FIELD()
    HashSet<ShaderProperty> m_props;

    HYP_FIELD()
    VertexAttributeSet m_requiredVertexAttributes;

    HYP_FIELD()
    VertexAttributeSet m_optionalVertexAttributes;

    mutable HashCode m_cachedHashCode;
    mutable HashCode m_cachedPropertySetHashCode;
    mutable bool m_needsHashCodeRecalculation;
};

HYP_STRUCT()
struct HashedShaderDefinition
{
    HYP_STRUCT_BODY(HashedShaderDefinition);

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    HashCode propertySetHash;

    HYP_FIELD()
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

HYP_ENUM()
enum class DescriptorUsageFlags : uint32
{
    NONE = 0x0,
    DYNAMIC = 0x1
};

HYP_MAKE_ENUM_FLAGS(DescriptorUsageFlags)

HYP_STRUCT()
struct DescriptorUsageType
{
    HYP_STRUCT_BODY(DescriptorUsageType);

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
    HYP_STRUCT_BODY(DescriptorUsage);

    HYP_FIELD(Property = "Slot", Serialize = true)
    DescriptorSlot slot;

    HYP_FIELD(Property = "SetName", Serialize = true)
    Name setName;

    HYP_FIELD(Property = "DescriptorName", Serialize = true)
    Name descriptorName;

    HYP_FIELD(Property = "Type", Serialize = true)
    DescriptorUsageType type;

    HYP_FIELD(Property = "Flags", Serialize = true)
    EnumFlags<DescriptorUsageFlags> flags;

    HYP_FIELD(Property = "Params", Serialize = true)
    HashMap<String, String> params;

    DescriptorUsage()
        : slot((DescriptorSlot)0),
          setName(Name::Invalid()),
          flags(DescriptorUsageFlags::NONE)
    {
    }

    DescriptorUsage(DescriptorSlot slot, Name setName, Name descriptorName, EnumFlags<DescriptorUsageFlags> flags = DescriptorUsageFlags::NONE, HashMap<String, String> params = {})
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
            return uint32(flags) < uint32(other.flags);
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

HYP_STRUCT()
struct DescriptorUsageSet
{
    HYP_STRUCT_BODY(DescriptorUsageSet);

    HYP_FIELD()
    FlatSet<DescriptorUsage> elements;

    void BuildDescriptorTableDeclaration(DescriptorTableDeclaration& table) const;

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

HYP_STRUCT()
struct ShaderDefinition
{
    HYP_STRUCT_BODY(ShaderDefinition);

    HYP_FIELD()
    Name name;

    HYP_FIELD()
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
struct HYP_API CompiledShader
{
    HYP_STRUCT_BODY(CompiledShader);

    HYP_FIELD(Property = "Definition")
    ShaderDefinition definition;

    HYP_FIELD(Property = "DescriptorTableDeclaration", Transient) // built after load, not serialized
    DescriptorTableDeclaration* descriptorTableDeclaration = nullptr;

    HYP_FIELD(Property = "DescriptorUsageSet")
    DescriptorUsageSet descriptorUsageSet;

    HYP_FIELD(Property = "EntryPointName")
    String entryPointName = "main";

    HYP_FIELD(Property = "Modules")
    FixedArray<ByteBuffer, SMT_MAX> modules;

    /// ===== Serialization only =====
    HYP_METHOD(Property = "RevisionNumber", NoScriptBindings)
    uint64 GetRevisionNumber() const;
    /// ==============================

    CompiledShader() = default;

    CompiledShader(const CompiledShader& other);
    CompiledShader& operator=(const CompiledShader& other);

    CompiledShader(CompiledShader&& other) noexcept;
    CompiledShader& operator=(CompiledShader&& other) noexcept;

    ~CompiledShader();

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

    HYP_FORCE_INLINE const DescriptorTableDeclaration* GetDescriptorTableDeclaration() const
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

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(definition.GetHashCode());
        hc.Add(modules.GetHashCode());

        return hc;
    }
};

HYP_STRUCT()
struct CompiledShaderBatch
{
    HYP_STRUCT_BODY(CompiledShaderBatch);

    HYP_FIELD()
    Array<CompiledShader> compiledShaders;

    HYP_FIELD()
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

    bool GetShaderInstance(Name name, const ShaderProperties& properties, CompiledShader& out) const;

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

void MergeGlobalShaderProperties(ShaderProperties& out);

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
        CompiledShaderBatch& out)
    {
        return CompileBundle(bundle, ShaderProperties(), out);
    }

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
