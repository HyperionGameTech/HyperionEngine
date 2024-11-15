/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADER_COMPILER_HPP
#define HYPERION_SHADER_COMPILER_HPP

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <math/Vertex.hpp>

#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <util/ini/INIFile.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <mutex>

namespace hyperion {
namespace json {

class JSONValue;

} // namespace json
} // namespace hyperion

namespace hyperion {

class Engine;

using renderer::ShaderModule;
using renderer::ShaderModuleType;
using renderer::DescriptorTableDeclaration;
using renderer::DescriptorSetDeclaration;
using renderer::DescriptorDeclaration;

using ShaderPropertyFlags = uint32;

enum ShaderPropertyFlagBits : ShaderPropertyFlags
{
    SHADER_PROPERTY_FLAG_NONE               = 0x0,
    SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE   = 0x1
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
    String              name;
    String              type_class;
    int                 location = -1;
    Optional<String>    condition;
};

struct ShaderProperty
{
    using Value = Variant<String, int, float>;

    String              name;
    bool                is_permutation;
    ShaderPropertyFlags flags;
    Value               value;
    Array<String>       possible_values;

    ShaderProperty()
        : is_permutation(false),
          flags(SHADER_PROPERTY_FLAG_NONE)
    {
    }

    ShaderProperty(const String &name) = delete;

    ShaderProperty(const String &name, bool is_permutation, ShaderPropertyFlags flags = SHADER_PROPERTY_FLAG_NONE)
        : name(name),
          is_permutation(is_permutation),
          flags(flags)
    {
    }

    ShaderProperty(const String &name, bool is_permutation, const Value &value, ShaderPropertyFlags flags = SHADER_PROPERTY_FLAG_NONE)
        : name(name),
          is_permutation(is_permutation),
          flags(flags),
          value(value)
    {
    }

    explicit ShaderProperty(VertexAttribute::Type vertex_attribute)
        : name(String("HYP_ATTRIBUTE_") + VertexAttribute::mapping.Get(vertex_attribute).name),
          is_permutation(false),
          flags(SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE),
          value(Value(String(VertexAttribute::mapping.Get(vertex_attribute).name)))
    {
    }

    ShaderProperty(const ShaderProperty &other)
        : name(other.name),
          is_permutation(other.is_permutation),
          flags(other.flags),
          value(other.value),
          possible_values(other.possible_values)
    {
    }

    ShaderProperty &operator=(const ShaderProperty &other)
    {
        name = other.name;
        is_permutation = other.is_permutation;
        flags = other.flags;
        value = other.value;
        possible_values = other.possible_values;

        return *this;
    }

    ShaderProperty(ShaderProperty &&other) noexcept
        : name(std::move(other.name)),
          is_permutation(other.is_permutation),
          flags(other.flags),
          value(std::move(other.value)),
          possible_values(std::move(other.possible_values))
    {
        other.is_permutation = false;
        other.flags = SHADER_PROPERTY_FLAG_NONE;
    }

    ShaderProperty &operator=(ShaderProperty &&other) noexcept
    {
        name = std::move(other.name);
        flags = other.flags;
        value = std::move(other.value);
        possible_values = std::move(other.possible_values);

        other.is_permutation = false;
        other.flags = SHADER_PROPERTY_FLAG_NONE;

        return *this;
    }
    
    HYP_FORCE_INLINE bool operator==(const ShaderProperty &other) const
        { return name == other.name; }
    
    HYP_FORCE_INLINE bool operator!=(const ShaderProperty &other) const
        { return name != other.name; }
    
    HYP_FORCE_INLINE bool operator==(const String &str) const
        { return name == str; }
    
    HYP_FORCE_INLINE bool operator!=(const String &str) const
        { return name != str; }
    
    HYP_FORCE_INLINE bool operator<(const ShaderProperty &other) const
        { return name < other.name; }
    
    HYP_FORCE_INLINE bool IsValueGroup() const
        { return possible_values.Any(); }

    /*! \brief If this ShaderProperty is a value group, returns the number of possible values,
     *  otherwise returns 0. If a default value is set for this ShaderProperty, that is taken into account */
    HYP_FORCE_INLINE  SizeType ValueGroupSize() const
    {
        if (!IsValueGroup()) {
            return 0;
        }

        const SizeType value_group_size = possible_values.Size();

        if (HasValue()) {
            const String *value_ptr = value.TryGet<String>();

            if (value_ptr && possible_values.Contains(*value_ptr)) {
                return value_group_size - 1;
            }
        }

        return value_group_size;
    }

    /*! \brief If this ShaderProperty is a value group, returns an Array of all possible values.
     *  Otherwise, returns the currently set value (if applicable). If a default value is set for this ShaderProperty, that is included in the Array. */
    HYP_FORCE_INLINE FlatSet<ShaderProperty> GetAllPossibleValues()
    {
        FlatSet<ShaderProperty> result;

        if (IsValueGroup()) {
            result.Reserve(possible_values.Size() + (HasValue() ? 1 : 0));

            for (const auto &possible_value : possible_values) {
                result.Insert(ShaderProperty(possible_value, false));
            }

            if (HasValue()) {
                const String *value_ptr = value.TryGet<String>();
                AssertThrowMsg(value_ptr != nullptr, "For a valuegroup with a default value, the default value should be of type String. Got TypeID: %u\n", value.GetTypeID().GetHashCode().Value());

                result.Insert(ShaderProperty(name + "_" + *value_ptr, false));
            }
        } else {
            if (HasValue()) {
                result.Insert(ShaderProperty(name, false, value, flags));
            }
        }

        return result;
    }
    
    HYP_FORCE_INLINE bool HasValue() const
        { return value.IsValid(); }
    
    HYP_FORCE_INLINE const String &GetName() const
        { return name; }
    
    HYP_FORCE_INLINE ShaderPropertyFlags GetFlags() const
        { return flags; }
    
    HYP_FORCE_INLINE bool IsPermutable() const
        { return is_permutation; }
    
    HYP_FORCE_INLINE bool IsVertexAttribute() const
        { return flags & SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE; }
    
    HYP_FORCE_INLINE bool IsOptionalVertexAttribute() const
        { return IsVertexAttribute() && IsPermutable(); }
    
    HYP_FORCE_INLINE String GetValueString() const
    {
        if (const String *str = value.TryGet<String>()) {
            return *str;
        } else if (const int *i = value.TryGet<int>()) {
            return String::ToString(*i);
        } else {
            return String::empty;
        }
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name.GetHashCode());
        // hashcode does not depend on any other properties than name.

        return hc;
    }
};

class ShaderProperties
{
    friend class ShaderCompiler;

public:
    ShaderProperties()
        : m_needs_hash_code_recalculation(true)
    {
    }

    ShaderProperties(const Array<String> &props)
        : m_needs_hash_code_recalculation(true)
    {
        for (const String &prop_key : props) {
            m_props.Insert(ShaderProperty(prop_key, true)); // default to permutable
        }
    }

    ShaderProperties(const Array<ShaderProperty> &props)
        : m_needs_hash_code_recalculation(true)
    {
        for (const ShaderProperty &property : props) {
            Set(property);
        }
    }

    ShaderProperties(const VertexAttributeSet &vertex_attributes, const Array<String> &props)
        : m_required_vertex_attributes(vertex_attributes),
          m_needs_hash_code_recalculation(true)
    {
        for (const String &prop_key : props) {
            m_props.Insert(ShaderProperty(prop_key, true)); // default to permutable
        }
    }

    ShaderProperties(const VertexAttributeSet &vertex_attributes)
        : ShaderProperties(vertex_attributes, { })
    {
    }

    ShaderProperties(const ShaderProperties &other)             = default;
    ShaderProperties &operator=(const ShaderProperties &other)  = default;
    ShaderProperties(ShaderProperties &&other) noexcept         = default;
    ShaderProperties &operator=(ShaderProperties &&other)       = default;
    ~ShaderProperties()                                         = default;

    HYP_FORCE_INLINE bool operator==(const ShaderProperties &other) const
        { return (m_required_vertex_attributes == other.m_required_vertex_attributes) && (m_props == other.m_props); }

    HYP_FORCE_INLINE bool operator!=(const ShaderProperties &other) const
        { return m_required_vertex_attributes != other.m_required_vertex_attributes || m_props != other.m_props; }

    HYP_FORCE_INLINE bool Any() const
        { return m_props.Any(); }

    HYP_FORCE_INLINE bool Empty() const
        { return m_props.Empty(); }

    HYP_FORCE_INLINE bool HasRequiredVertexAttributes(VertexAttributeSet vertex_attributes) const
        { return (m_required_vertex_attributes & vertex_attributes) == vertex_attributes; }

    HYP_FORCE_INLINE bool HasRequiredVertexAttribute(VertexAttribute::Type vertex_attribute) const
        { return m_required_vertex_attributes.Has(vertex_attribute); }

    HYP_FORCE_INLINE bool HasOptionalVertexAttributes(VertexAttributeSet vertex_attributes) const
        { return (m_optional_vertex_attributes & vertex_attributes) == vertex_attributes; }

    HYP_FORCE_INLINE bool HasOptionalVertexAttribute(VertexAttribute::Type vertex_attribute) const
        { return m_optional_vertex_attributes.Has(vertex_attribute); }

    HYP_FORCE_INLINE bool Has(const ShaderProperty &shader_property) const
    {
        const auto it = m_props.Find(shader_property);

        if (it == m_props.End()) {
            return false;
        }

        return true;
    }

    HYP_API ShaderProperties &Set(const ShaderProperty &property, bool enabled = true);

    HYP_FORCE_INLINE ShaderProperties &Set(const String &name, bool enabled = true)
        { return Set(ShaderProperty(name, true), enabled); }

    /*! \brief Applies \ref{other} properties onto this set */
    HYP_FORCE_INLINE void Merge(const ShaderProperties &other)
    {
        for (const ShaderProperty &property : other.m_props) {
            Set(property);
        }

        m_required_vertex_attributes |= other.m_required_vertex_attributes;
        m_optional_vertex_attributes |= other.m_optional_vertex_attributes;

        m_needs_hash_code_recalculation = true;
    }

    HYP_FORCE_INLINE const FlatSet<ShaderProperty> &GetPropertySet() const
        { return m_props; }

    HYP_FORCE_INLINE VertexAttributeSet GetRequiredVertexAttributes() const
        { return m_required_vertex_attributes; }

    HYP_FORCE_INLINE VertexAttributeSet GetOptionalVertexAttributes() const
        { return m_optional_vertex_attributes; }

    HYP_FORCE_INLINE void SetRequiredVertexAttributes(VertexAttributeSet vertex_attributes)
    {
        m_required_vertex_attributes = vertex_attributes;
        m_optional_vertex_attributes = m_optional_vertex_attributes & ~m_required_vertex_attributes;

        m_needs_hash_code_recalculation = true;
    }

    HYP_FORCE_INLINE void SetOptionalVertexAttributes(VertexAttributeSet vertex_attributes)
    {
        m_optional_vertex_attributes = vertex_attributes & ~m_required_vertex_attributes;
    }

    HYP_FORCE_INLINE SizeType Size() const
        { return m_props.Size(); }

    HYP_FORCE_INLINE Array<ShaderProperty> ToArray() const
        { return m_props.ToArray(); }

    HYP_FORCE_INLINE String ToString() const
    {
        Array<String> property_names;

        for (const VertexAttribute::Type attribute_type : GetRequiredVertexAttributes().BuildAttributes()) {
            property_names.PushBack(String("HYP_ATTRIBUTE_") + VertexAttribute::mapping[attribute_type].name);
        }

        for (const ShaderProperty &property : GetPropertySet()) {
            property_names.PushBack(property.name);
        }

        String properties_string;
        SizeType index = 0;

        for (const String &name : property_names) {
            properties_string += name;

            if (index != property_names.Size() - 1) {
                properties_string += ", ";
            }

            index++;
        }

        return properties_string;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (m_needs_hash_code_recalculation) {
            RecalculateHashCode();

            m_needs_hash_code_recalculation = false;
        }

        return m_cached_hash_code;
    }

    HYP_FORCE_INLINE HashCode GetPropertySetHashCode() const
    {
        if (m_needs_hash_code_recalculation) {
            RecalculateHashCode();

            m_needs_hash_code_recalculation = false;
        }

        return m_cached_property_set_hash_code;
    }

private:
    void RecalculateHashCode() const
    {
        HashCode hc;

        // NOTE: Intentionally left out m_optional_vertex_attributes,
        // as they do not impact the final instantiated version of the shader properties.

        m_cached_property_set_hash_code = m_props.GetHashCode();

        hc.Add(m_required_vertex_attributes.GetHashCode());
        hc.Add(m_cached_property_set_hash_code);

        m_cached_hash_code = hc;
    }

    HYP_FORCE_INLINE ShaderProperties &AddPermutation(const String &key)
    {
        const ShaderProperty shader_property(key, true);

        const auto it = m_props.Find(shader_property);

        if (it == m_props.End()) {
            m_props.Insert(shader_property);
        } else {
            *it = shader_property;
        }

        m_needs_hash_code_recalculation = true;

        return *this;
    }

    HYP_FORCE_INLINE ShaderProperties &AddValueGroup(const String &key, const Array<String> &possible_values)
    {
        ShaderProperty shader_property(key, false);
        shader_property.possible_values = possible_values;

        const auto it = m_props.Find(shader_property);

        if (it == m_props.End()) {
            m_props.Insert(shader_property);
        } else {
            *it = shader_property;
        }

        m_needs_hash_code_recalculation = true;

        return *this;
    }

    FlatSet<ShaderProperty> m_props;

    VertexAttributeSet      m_required_vertex_attributes;
    VertexAttributeSet      m_optional_vertex_attributes;

    mutable HashCode        m_cached_hash_code;
    mutable HashCode        m_cached_property_set_hash_code;
    mutable bool            m_needs_hash_code_recalculation;
};

struct HashedShaderDefinition
{
    Name                name;
    HashCode            property_set_hash;
    VertexAttributeSet  required_vertex_attributes;

    HYP_FORCE_INLINE bool operator==(const HashedShaderDefinition &other) const
    {
        return name == other.name
            && property_set_hash == other.property_set_hash
            && required_vertex_attributes == other.required_vertex_attributes;
    }

    HYP_FORCE_INLINE bool operator!=(const HashedShaderDefinition &other) const
    {
        return name != other.name
            || property_set_hash != other.property_set_hash
            || required_vertex_attributes != other.required_vertex_attributes;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name.GetHashCode());
        hc.Add(required_vertex_attributes.GetHashCode());
        hc.Add(property_set_hash);

        return hc;
    }
};

using DescriptorUsageFlags = uint32;

enum DescriptorUsageFlagBits : DescriptorUsageFlags
{
    DESCRIPTOR_USAGE_FLAG_NONE = 0x0,
    DESCRIPTOR_USAGE_FLAG_DYNAMIC = 0x1
};

struct DescriptorUsageType
{
    Name                            name; 
    uint32                          size = ~0u;
    Array<Name>                     field_names;
    Array<DescriptorUsageType, 0>   field_types;

    DescriptorUsageType()                                                   = default;

    DescriptorUsageType(Name name, uint32 size = ~0u)
        : name(name),
          size(size)
    {
    }

    DescriptorUsageType(const DescriptorUsageType &other)                   = default;
    DescriptorUsageType &operator=(const DescriptorUsageType &other)        = default;
    DescriptorUsageType(DescriptorUsageType &&other) noexcept               = default;
    DescriptorUsageType &operator=(DescriptorUsageType &&other) noexcept    = default;

    HYP_FORCE_INLINE bool IsValid() const
        { return name.IsValid(); }

    HYP_FORCE_INLINE bool HasExplicitSize() const
        { return size != ~0u; }

    HYP_FORCE_INLINE Name GetName() const
        { return name; }
    
    HYP_FORCE_INLINE uint32 GetSize() const
        { return size; }

    HYP_FORCE_INLINE Pair<Name, DescriptorUsageType &> AddField(Name field_name, const DescriptorUsageType &type)
    {
        return Pair<Name, DescriptorUsageType &> { field_names.PushBack(field_name), field_types.PushBack(type) };
    }

    HYP_FORCE_INLINE Pair<Name, DescriptorUsageType &> GetField(SizeType index)
    {
        return { field_names[index], field_types[index] };
    }

    HYP_FORCE_INLINE const Pair<Name, const DescriptorUsageType &> GetField(SizeType index) const
    {
        return { field_names[index], field_types[index] };
    }

    HYP_FORCE_INLINE Optional<Pair<Name, DescriptorUsageType &>> FindField(WeakName field_name)
    {
        for (SizeType i = 0; i < field_names.Size(); i++) {
            if (field_names[i] == field_name) {
                return Pair<Name, DescriptorUsageType &> { field_names[i], field_types[i] };
            }
        }

        return { };
    }

    HYP_FORCE_INLINE Optional<Pair<Name, const DescriptorUsageType &>> FindField(WeakName field_name) const
    {
        for (SizeType i = 0; i < field_names.Size(); i++) {
            if (field_names[i] == field_name) {
                return Pair<Name, const DescriptorUsageType &> { field_names[i], field_types[i] };
            }
        }

        return { };
    }

    HYP_FORCE_INLINE bool operator<(const DescriptorUsageType &other) const
    {
        if (size != other.size) {
            return size < other.size;
        }

        if (field_types.Size() != other.field_types.Size()) {
            return field_types.Size() < other.field_types.Size();
        }

        for (SizeType i = 0; i < field_types.Size(); i++) {
            if (field_types[i] != other.field_types[i]) {
                return field_types[i] < other.field_types[i];
            }
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const DescriptorUsageType &other) const
    {
        return name == other.name
            && size == other.size
            && field_names == other.field_names
            && field_types == other.field_types;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsageType &other) const
    {
        return name != other.name
            || size != other.size
            || field_names != other.field_names
            || field_types != other.field_types;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(size);
        hc.Add(field_names);
        hc.Add(field_types);

        return hc;
    }
};

struct DescriptorUsage
{
    renderer::DescriptorSlot    slot;
    Name                        set_name;
    Name                        descriptor_name;
    DescriptorUsageType         type;
    DescriptorUsageFlags        flags;
    HashMap<String, String>     params;

    DescriptorUsage()
        : slot(renderer::DESCRIPTOR_SLOT_NONE),
          set_name(Name::Invalid()),
          flags(DESCRIPTOR_USAGE_FLAG_NONE)
    {
    }

    DescriptorUsage(renderer::DescriptorSlot slot, Name set_name, Name descriptor_name, DescriptorUsageFlags flags = DESCRIPTOR_USAGE_FLAG_NONE, HashMap<String, String> params = { })
        : slot(slot),
          set_name(set_name),
          descriptor_name(descriptor_name),
          flags(flags),
          params(std::move(params))
    {
    }

    DescriptorUsage(const DescriptorUsage &other)
        : slot(other.slot),
          set_name(other.set_name),
          descriptor_name(other.descriptor_name),
          type(other.type),
          flags(other.flags),
          params(other.params)
    {
    }

    DescriptorUsage &operator=(const DescriptorUsage &other)
    {
        if (this == &other) {
            return *this;
        }

        slot = other.slot;
        set_name = other.set_name;
        descriptor_name = other.descriptor_name;
        type = other.type;
        flags = other.flags;
        params = other.params;

        return *this;
    }

    DescriptorUsage(DescriptorUsage &&other) noexcept
        : slot(other.slot),
          set_name(std::move(other.set_name)),
          descriptor_name(std::move(other.descriptor_name)),
          type(std::move(other.type)),
          flags(other.flags),
          params(std::move(other.params))
    {
    }

    DescriptorUsage &operator=(DescriptorUsage &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        slot = other.slot;
        set_name = std::move(other.set_name);
        descriptor_name = std::move(other.descriptor_name);
        type = std::move(other.type);
        flags = other.flags;
        params = std::move(other.params);

        return *this;
    }

    ~DescriptorUsage() = default;

    HYP_FORCE_INLINE bool operator==(const DescriptorUsage &other) const
    {
        return slot == other.slot
            && set_name == other.set_name
            && descriptor_name == other.descriptor_name
            && type == other.type
            && flags == other.flags
            && params == other.params;
    }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsage &other) const
    {
        return slot != other.slot
            || set_name != other.set_name
            || descriptor_name != other.descriptor_name
            || type != other.type
            || flags != other.flags
            || params != other.params;
    }

    HYP_FORCE_INLINE bool operator<(const DescriptorUsage &other) const
    {
        if (slot != other.slot) {
            return slot < other.slot;
        }

        if (set_name != other.set_name) {
            return set_name < other.set_name;
        }

        if (descriptor_name != other.descriptor_name) {
            return descriptor_name < other.descriptor_name;
        }

        if (type != other.type) {
            return type < other.type;
        }

        if (flags != other.flags) {
            return flags < other.flags;
        }

        return false;
    }

    HYP_FORCE_INLINE uint32 GetCount() const
    {
        uint32 value = 1;

        auto it = params.Find("count");

        if (it == params.End()) {
            return value;
        }

        if (StringUtil::Parse(it->second, &value)) {
            return value;
        }

        return 1;
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        if (type.size != ~0u) {
            return type.size;
        }

        uint32 value = ~0u;

        auto it = params.Find("size");

        if (it == params.End()) {
            return value;
        }

        if (StringUtil::Parse(it->second, &value)) {
            return value;
        }

        return uint32(-1);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(slot);
        hc.Add(set_name.GetHashCode());
        hc.Add(descriptor_name.GetHashCode());
        hc.Add(type);
        hc.Add(flags);
        hc.Add(params.GetHashCode());

        return hc;
    
    }
};

struct DescriptorUsageSet
{
    FlatSet<DescriptorUsage>    elements;

    HYP_FORCE_INLINE DescriptorUsage &operator[](SizeType index)
        { return elements[index]; }

    HYP_FORCE_INLINE const DescriptorUsage &operator[](SizeType index) const
        { return elements[index]; }

    HYP_FORCE_INLINE bool operator==(const DescriptorUsageSet &other) const
        { return elements == other.elements; }

    HYP_FORCE_INLINE bool operator!=(const DescriptorUsageSet &other) const
        { return elements != other.elements; }

    HYP_FORCE_INLINE SizeType Size() const
        { return elements.Size(); }

    HYP_FORCE_INLINE void Add(const DescriptorUsage &descriptor_usage)
        { elements.Insert(descriptor_usage); }

    HYP_FORCE_INLINE DescriptorUsage *Find(WeakName descriptor_name)
    {
        auto it = elements.FindIf([descriptor_name](const DescriptorUsage &descriptor_usage)
        {
            return descriptor_usage.descriptor_name == descriptor_name;
        });

        if (it == elements.End()) {
            return nullptr;
        }

        return it;
    }

    HYP_FORCE_INLINE const DescriptorUsage *Find(WeakName descriptor_name) const
        { return const_cast<const DescriptorUsageSet *>(this)->Find(descriptor_name); }

    HYP_FORCE_INLINE void Merge(const Array<DescriptorUsage> &other)
        { elements.Merge(other); }

    HYP_FORCE_INLINE void Merge(Array<DescriptorUsage> &&other)
        { elements.Merge(std::move(other)); }

    HYP_FORCE_INLINE void Merge(const DescriptorUsageSet &other)
        { elements.Merge(other.elements); }

    HYP_FORCE_INLINE void Merge(DescriptorUsageSet &&other)
        { elements.Merge(std::move(other.elements)); }

    HYP_NODISCARD DescriptorTableDeclaration BuildDescriptorTable() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
        { return elements.GetHashCode(); }
};

struct ShaderDefinition
{
    Name                name;
    ShaderProperties    properties;

    HYP_FORCE_INLINE Name GetName() const
        { return name; }

    HYP_FORCE_INLINE ShaderProperties &GetProperties()
        { return properties; }

    HYP_FORCE_INLINE const ShaderProperties &GetProperties() const
        { return properties; }

    HYP_FORCE_INLINE explicit operator bool() const
        { return name.IsValid(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return name.IsValid(); }

    HYP_FORCE_INLINE bool operator==(const ShaderDefinition &other) const
        { return GetHashCode() == other.GetHashCode(); }

    HYP_FORCE_INLINE bool operator!=(const ShaderDefinition &other) const
        { return GetHashCode() != other.GetHashCode(); }

    HYP_FORCE_INLINE bool operator<(const ShaderDefinition &other) const
        { return GetHashCode() < other.GetHashCode(); }

    HYP_FORCE_INLINE explicit operator HashedShaderDefinition() const
        { return HashedShaderDefinition { name, properties.GetPropertySetHashCode(), properties.GetRequiredVertexAttributes() }; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        // ensure they return the same hash codes so they can be compared.
        return (operator HashedShaderDefinition()).GetHashCode();
    }
};

struct CompiledShader
{
    ShaderDefinition                                definition;
    DescriptorUsageSet                              descriptor_usages;
    String                                          entry_point_name = "main";
    HeapArray<ByteBuffer, ShaderModuleType::MAX>    modules;

    CompiledShader() = default;
    CompiledShader(ShaderDefinition shader_definition, DescriptorUsageSet descriptor_usages, String entry_point_name = "main")
        : definition(std::move(shader_definition)),
          descriptor_usages(std::move(descriptor_usages)),
          entry_point_name(std::move(entry_point_name))
    {
    }

    CompiledShader(const CompiledShader &other)
        : definition(other.definition),
          descriptor_usages(other.descriptor_usages),
          entry_point_name(other.entry_point_name),
          modules(other.modules)
    {
    }

    CompiledShader &operator=(const CompiledShader &other)
    {
        if (this == &other) {
            return *this;
        }

        definition = other.definition;
        descriptor_usages = other.descriptor_usages;
        entry_point_name = other.entry_point_name;
        modules = other.modules;

        return *this;
    }

    CompiledShader(CompiledShader &&other) noexcept
        : definition(std::move(other.definition)),
          descriptor_usages(std::move(other.descriptor_usages)),
          entry_point_name(std::move(other.entry_point_name)),
          modules(std::move(other.modules))
    {
    }
    
    CompiledShader &operator=(CompiledShader &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        definition = std::move(other.definition);
        descriptor_usages = std::move(other.descriptor_usages);
        entry_point_name = std::move(other.entry_point_name);
        modules = std::move(other.modules);

        return *this;
    }

    ~CompiledShader() = default;
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }
    
    HYP_FORCE_INLINE bool IsValid() const
    {
        return definition.IsValid()
            && modules.Any([](const ByteBuffer &buffer) { return buffer.Any(); });
    }
    
    HYP_FORCE_INLINE Name GetName() const
        { return definition.name; }
    
    HYP_FORCE_INLINE ShaderDefinition &GetDefinition()
        { return definition; }
    
    HYP_FORCE_INLINE const ShaderDefinition &GetDefinition() const
        { return definition; }
    
    HYP_FORCE_INLINE DescriptorUsageSet &GetDescriptorUsages()
        { return descriptor_usages; }

    HYP_FORCE_INLINE const DescriptorUsageSet &GetDescriptorUsages() const
        { return descriptor_usages; }
    
    HYP_FORCE_INLINE const String &GetEntryPointName() const
        { return entry_point_name; }
    
    HYP_FORCE_INLINE const ShaderProperties &GetProperties() const
        { return definition.properties; }
    
    HYP_FORCE_INLINE const HeapArray<ByteBuffer, ShaderModuleType::MAX> &GetModules() const
        { return modules; }
    
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
    Array<CompiledShader>   compiled_shaders;
    Array<String>           error_messages;

    CompiledShaderBatch() = default;

    CompiledShaderBatch(const CompiledShaderBatch &other)
        : compiled_shaders(other.compiled_shaders),
          error_messages(other.error_messages)
    {
    }

    CompiledShaderBatch &operator=(const CompiledShaderBatch &other)
    {
        if (this == &other) {
            return *this;
        }

        compiled_shaders = other.compiled_shaders;
        error_messages = other.error_messages;

        return *this;
    }

    CompiledShaderBatch(CompiledShaderBatch &&other) noexcept
        : compiled_shaders(std::move(other.compiled_shaders)),
          error_messages(std::move(other.error_messages))
    {
    }

    CompiledShaderBatch &operator=(CompiledShaderBatch &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        compiled_shaders = std::move(other.compiled_shaders);
        error_messages = std::move(other.error_messages);

        return *this;
    }

    ~CompiledShaderBatch() = default;

    HYP_FORCE_INLINE bool HasErrors() const
        { return error_messages.Any(); }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return compiled_shaders.GetHashCode();
    }
};

class ShaderCache
{
public:
    ShaderCache()                                           = default;
    ShaderCache(const ShaderCache &other)                   = delete;
    ShaderCache &operator=(const ShaderCache &other)        = delete;
    ShaderCache(ShaderCache &&other) noexcept               = delete;
    ShaderCache &operator=(ShaderCache &&other) noexcept    = delete;
    ~ShaderCache()                                          = default;

    bool Get(Name name, CompiledShaderBatch &out) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_compiled_shaders.Find(name);

        if (it == m_compiled_shaders.End()) {
            return false;
        }

        out = it->second;

        return true;
    }

    bool GetShaderInstance(Name name, uint64 version_hash, CompiledShader &out) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_compiled_shaders.Find(name);

        if (it == m_compiled_shaders.End()) {
            return false;
        }

        const auto version_it = it->second.compiled_shaders.FindIf([version_hash](const CompiledShader &item)
        {
            return item.definition.properties.GetHashCode().Value() == version_hash;
        });

        if (version_it == it->second.compiled_shaders.End()) {
            return false;
        }

        out = *version_it;

        return true;
    }

    void Set(Name name, const CompiledShaderBatch &batch)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Set(name, batch);
    }

    void Set(Name name, CompiledShaderBatch &&batch)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Set(name, std::move(batch));
    }

    void Remove(Name name)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Erase(name);
    }

private:
    HashMap<Name, CompiledShaderBatch>  m_compiled_shaders;
    mutable std::mutex                  m_mutex;
};


class ShaderCompiler
{
    static constexpr SizeType max_permutations = 2048;//temporalily increased, until some properties are "always enabled"

    struct ProcessError
    {
        String error_message;
    };

    struct ProcessResult
    {
        String                              processed_source;
        Array<ProcessError>                 errors;
        Array<VertexAttributeDefinition>    required_attributes;
        Array<VertexAttributeDefinition>    optional_attributes;
        Array<DescriptorUsage>              descriptor_usages;

        ProcessResult()                                             = default;
        ProcessResult(const ProcessResult &other)                   = default;
        ProcessResult &operator=(const ProcessResult &other)        = default;
        ProcessResult(ProcessResult &&other) noexcept               = default;
        ProcessResult &operator=(ProcessResult &&other) noexcept    = default;
        ~ProcessResult()                                            = default;
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
        Name                                    name;
        String                                  entry_point_name = "main";
        FlatMap<ShaderModuleType, SourceFile>   sources;
        ShaderProperties                        versions; // permutations

        bool HasRTShaders() const
        {
            return sources.Any([](const KeyValuePair<ShaderModuleType, SourceFile> &item)
            {
                return ShaderModule::IsRaytracingType(item.first);
            });
        }

        bool IsComputeShader() const
        {
            return sources.Every([](const KeyValuePair<ShaderModuleType, SourceFile> &item)
            {
                return item.first == ShaderModuleType::COMPUTE;
            });
        }

        bool HasVertexShader() const
        {
            return sources.Any([](const KeyValuePair<ShaderModuleType, SourceFile> &item)
            {
                return item.first == ShaderModuleType::VERTEX;
            });
        }
    };

    ShaderCompiler();
    ShaderCompiler(const ShaderCompiler &other) = delete;
    ShaderCompiler &operator=(const ShaderCompiler &other) = delete;
    ~ShaderCompiler();

    HYP_API bool CanCompileShaders() const;
    HYP_API bool LoadShaderDefinitions(bool precompile_shaders = false);

    HYP_API CompiledShader GetCompiledShader(Name name);
    HYP_API CompiledShader GetCompiledShader(Name name, const ShaderProperties &properties);

    HYP_API bool GetCompiledShader(
        Name name,
        const ShaderProperties &properties,
        CompiledShader &out
    );

private:
    void GetPlatformSpecificProperties(
        ShaderProperties &out
    ) const;

    ProcessResult ProcessShaderSource(
        ProcessShaderSourcePhase phase,
        ShaderModuleType type,
        ShaderLanguage language,
        const String &source,
        const String &filename,
        const ShaderProperties &properties
    );

    void ParseDefinitionSection(
        const INIFile::Section &section,
        Bundle &bundle
    );

    bool CompileBundle(
        Bundle &bundle,
        const ShaderProperties &additional_versions,
        CompiledShaderBatch &out
    );

    bool HandleCompiledShaderBatch(
        Bundle &bundle,
        const ShaderProperties &additional_versions,
        const FilePath &output_file_path,
        CompiledShaderBatch &batch
    );

    bool LoadOrCompileBatch(
        Name name,
        const ShaderProperties &additional_versions,
        CompiledShaderBatch &out
    );

    INIFile         *m_definitions;
    ShaderCache     m_cache;
    Array<Bundle>   m_bundles;
};

} // namespace hyperion

#endif
