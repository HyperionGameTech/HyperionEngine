#ifndef HYPERION_V2_SHADER_COMPILER_H
#define HYPERION_V2_SHADER_COMPILER_H

#include <core/Containers.hpp>
#include <core/Name.hpp>

#include <math/Vertex.hpp>

#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <util/definitions/DefinitionsFile.hpp>
#include <util/Defines.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <set>
#include <mutex>

namespace hyperion {
namespace json {

class JSONValue;

} // namespace json
} // namespace hyperion

namespace hyperion::v2 {

class Engine;

using renderer::ShaderModule;
using renderer::ShaderModuleType;
using renderer::DescriptorTableDeclaration;
using renderer::DescriptorSetDeclaration;
using renderer::DescriptorDeclaration;

using ShaderPropertyFlags = uint32;

enum ShaderPropertyFlagBits : ShaderPropertyFlags
{
    SHADER_PROPERTY_FLAG_NONE = 0x0,
    SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE = 0x1
};

struct VertexAttributeDefinition
{
    String              name;
    String              type_class;
    int                 location = -1;
    Optional<String>    condition;
};

struct ShaderProperty;

static bool FindVertexAttributeForDefinition(const String &name, VertexAttribute::Type &out_type)
{
    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++) {
        const auto it = VertexAttribute::mapping.KeyValueAt(i);

        if (name == it.second.name) {
            out_type = it.first;

            return true;
        }
    }

    return false;
}

struct ShaderProperty
{
    using Value = Variant<String, int, float>;

    String name;
    bool is_permutation = false;
    ShaderPropertyFlags flags;
    Value value;
    Array<String> possible_values;

    ShaderProperty() = default;

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

    bool operator==(const ShaderProperty &other) const
        { return name == other.name; }

    bool operator!=(const ShaderProperty &other) const
        { return name != other.name; }

    bool operator==(const String &str) const
        { return name == str; }

    bool operator!=(const String &str) const
        { return name != str; }

    bool operator<(const ShaderProperty &other) const
        { return name < other.name; }

    bool IsValueGroup() const
        { return possible_values.Any(); }

    /**
     * \brief If this ShaderProperty is a value group, returns the number of possible values, otherwise returns 0. If a default value is set for this ShaderProperty, that is taken into account
     */
    SizeType ValueGroupSize() const
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

    /**
     * \brief If this ShaderProperty is a value group, returns an Array of all possible values. Otherwise, returns the currently set value (if applicable). If a default value is set for this ShaderProperty, that is included in the Array.
     */
    FlatSet<ShaderProperty> GetAllPossibleValues()
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

    bool HasValue() const
        { return value.IsValid(); }

    const String &GetName() const
        { return name; }

    ShaderPropertyFlags GetFlags() const
        { return flags; }

    bool IsPermutable() const
        { return is_permutation; }

    bool IsVertexAttribute() const
        { return flags & SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE; }

    bool IsOptionalVertexAttribute() const
        { return IsVertexAttribute() && IsPermutable(); }

    String GetValueString() const
    {
        if (const String *str = value.TryGet<String>()) {
            return *str;
        } else if (const int *i = value.TryGet<int>()) {
            return String::ToString(*i);
        } else {
            return String::empty;
        }
    }

    HashCode GetHashCode() const
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
    ShaderProperties() = default;

    ShaderProperties(const Array<String> &props)
    {
        for (const String &prop_key : props) {
            m_props.Insert(ShaderProperty(prop_key, true)); // default to permutable
        }
    }

    ShaderProperties(const Array<ShaderProperty> &props)
    {
        for (const ShaderProperty &property : props) {
            Set(property);
        }
    }

    ShaderProperties(const VertexAttributeSet &vertex_attributes, const Array<String> &props)
        : m_required_vertex_attributes(vertex_attributes)
    {
        for (const String &prop_key : props) {
            m_props.Insert(ShaderProperty(prop_key, true)); // default to permutable
        }
    }

    ShaderProperties(const VertexAttributeSet &vertex_attributes)
        : ShaderProperties(vertex_attributes, { })
    {
    }

    ShaderProperties(const ShaderProperties &other) = default;
    ShaderProperties &operator=(const ShaderProperties &other) = default;
    ShaderProperties(ShaderProperties &&other) noexcept = default;
    ShaderProperties &operator=(ShaderProperties &&other) = default;
    ~ShaderProperties() = default;

    bool operator==(const ShaderProperties &other) const
        { return m_props == other.m_props; }

    bool operator!=(const ShaderProperties &other) const
        { return m_props != other.m_props; }

    bool Any() const
        { return m_props.Any(); }

    bool Empty() const
        { return m_props.Empty(); }

    bool HasRequiredVertexAttributes(VertexAttributeSet vertex_attributes) const
        { return (m_required_vertex_attributes & vertex_attributes) == vertex_attributes; }

    bool HasRequiredVertexAttribute(VertexAttribute::Type vertex_attribute) const
        { return m_required_vertex_attributes.Has(vertex_attribute); }

    bool HasOptionalVertexAttributes(VertexAttributeSet vertex_attributes) const
        { return (m_optional_vertex_attributes & vertex_attributes) == vertex_attributes; }

    bool HasOptionalVertexAttribute(VertexAttribute::Type vertex_attribute) const
        { return m_optional_vertex_attributes.Has(vertex_attribute); }

    bool Has(const ShaderProperty &shader_property) const
    {
        const auto it = m_props.Find(shader_property);

        if (it == m_props.End()) {
            return false;
        }

        return true;
    }

    ShaderProperties &Set(const ShaderProperty &property, bool enabled = true)
    {
        if (property.IsVertexAttribute()) {
            VertexAttribute::Type type;

            if (!FindVertexAttributeForDefinition(property.GetValueString(), type)) {
                DebugLog(
                    LogType::Error,
                    "Invalid vertex attribute name for shader: %s\n",
                    property.GetValueString().Data()
                );

                return *this;
            }

            if (property.IsOptionalVertexAttribute()) {
                if (enabled) {
                    m_optional_vertex_attributes |= type;
                    m_optional_vertex_attributes &= ~m_required_vertex_attributes;
                } else {
                    m_optional_vertex_attributes &= ~type;
                }
            } else {
                if (enabled) {
                    m_required_vertex_attributes |= type;
                    m_optional_vertex_attributes &= ~type;
                } else {
                    m_required_vertex_attributes &= ~type;
                }

                m_needs_hash_code_recalculation = true;
            }
        } else {
            const auto it = m_props.Find(property);

            if (enabled) {
                if (it == m_props.End()) {
                    m_props.Insert(property);

                    m_needs_hash_code_recalculation = true;
                } else {
                    if (*it != property) {
                        *it = property;

                        m_needs_hash_code_recalculation = true;
                    }
                }
            } else {
                if (it != m_props.End()) {
                    m_props.Erase(it);

                    m_needs_hash_code_recalculation = true;
                }
            }
        }

        return *this;
    }

    ShaderProperties &Set(const String &name, bool enabled = true)
    {
        return Set(ShaderProperty(name, true), enabled);
    }

    /*! \brief Applies \ref{other} properties onto this set */
    void Merge(const ShaderProperties &other)
    {
        for (const ShaderProperty &property : other.m_props) {
            Set(property);
        }

        m_required_vertex_attributes |= other.m_required_vertex_attributes;
        m_optional_vertex_attributes |= other.m_optional_vertex_attributes;

        m_needs_hash_code_recalculation = true;
    }

    const FlatSet<ShaderProperty> &GetPropertySet() const
        { return m_props; }

    VertexAttributeSet GetRequiredVertexAttributes() const
        { return m_required_vertex_attributes; }

    VertexAttributeSet GetOptionalVertexAttributes() const
        { return m_optional_vertex_attributes; }

    void SetRequiredVertexAttributes(VertexAttributeSet vertex_attributes)
    {
        m_required_vertex_attributes = vertex_attributes;
        m_optional_vertex_attributes = m_optional_vertex_attributes & ~m_required_vertex_attributes;

        m_needs_hash_code_recalculation = true;
    }

    void SetOptionalVertexAttributes(VertexAttributeSet vertex_attributes)
    {
        m_optional_vertex_attributes = vertex_attributes & ~m_required_vertex_attributes;
    }

    SizeType Size() const
        { return m_props.Size(); }

    Array<ShaderProperty> ToArray() const
        { return m_props.ToArray(); }

    String ToString() const
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

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        if (m_needs_hash_code_recalculation) {
            RecalculateHashCode();

            m_needs_hash_code_recalculation = false;
        }

        return m_cached_hash_code;
    }

    HYP_FORCE_INLINE
    HashCode GetPropertySetHashCode() const
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

    ShaderProperties &AddPermutation(const String &key)
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

    ShaderProperties &AddValueGroup(const String &key, const Array<String> &possible_values)
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
    mutable bool            m_needs_hash_code_recalculation = true;
};

struct HashedShaderDefinition
{
    Name                name;
    HashCode            property_set_hash;
    VertexAttributeSet  required_vertex_attributes;

    HYP_FORCE_INLINE
    bool operator==(const HashedShaderDefinition &other) const
    {
        return name == other.name
            && property_set_hash == other.property_set_hash
            && required_vertex_attributes == other.required_vertex_attributes;
    }

    HYP_FORCE_INLINE
    bool operator!=(const HashedShaderDefinition &other) const
        { return !(*this == other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
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

struct DescriptorUsage
{
    renderer::DescriptorSlot    slot;
    Name                        set_name;
    Name                        descriptor_name;
    DescriptorUsageFlags        flags;
    HashMap<String, String>     params;

    DescriptorUsage()
        : slot(renderer::DESCRIPTOR_SLOT_NONE),
          set_name(Name::invalid),
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
          flags(other.flags),
          params(other.params)
    {
    }

    DescriptorUsage &operator=(const DescriptorUsage &other)
    {
        slot = other.slot;
        set_name = other.set_name;
        descriptor_name = other.descriptor_name;
        flags = other.flags;
        params = other.params;

        return *this;
    }

    DescriptorUsage(DescriptorUsage &&other) noexcept
        : slot(other.slot),
          set_name(std::move(other.set_name)),
          descriptor_name(std::move(other.descriptor_name)),
          flags(other.flags),
          params(std::move(other.params))
    {
    }

    DescriptorUsage &operator=(DescriptorUsage &&other) noexcept
    {
        slot = other.slot;
        set_name = std::move(other.set_name);
        descriptor_name = std::move(other.descriptor_name);
        flags = other.flags;
        params = std::move(other.params);

        return *this;
    }

    ~DescriptorUsage()                                              = default;

    bool operator==(const DescriptorUsage &other) const
    {
        return slot == other.slot
            && set_name == other.set_name
            && descriptor_name == other.descriptor_name
            && flags == other.flags
            && params == other.params;
    }

    bool operator<(const DescriptorUsage &other) const
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

        if (flags != other.flags) {
            return flags < other.flags;
        }

        return false;
    }

    uint GetCount() const
    {
        uint value = 1;

        auto it = params.Find("count");

        if (it == params.End()) {
            return value;
        }

        if (StringUtil::Parse(it->second, &value)) {
            return value;
        }

        return 1;
    }

    uint GetSize() const
    {
        uint value = uint(-1);

        auto it = params.Find("size");

        if (it == params.End()) {
            return value;
        }

        if (StringUtil::Parse(it->second, &value)) {
            return value;
        }

        return uint(-1);
    }
};

struct DescriptorUsageSet
{
    FlatSet<DescriptorUsage>    descriptor_usages;

    DescriptorUsage &operator[](SizeType index)
        { return descriptor_usages[index]; }

    const DescriptorUsage &operator[](SizeType index) const
        { return descriptor_usages[index]; }

    bool operator==(const DescriptorUsageSet &other) const
        { return descriptor_usages == other.descriptor_usages; }

    SizeType Size() const
        { return descriptor_usages.Size(); }

    void Add(DescriptorUsage descriptor_usage)
        { descriptor_usages.Insert(descriptor_usage); }

    void Merge(const Array<DescriptorUsage> &other)
        { descriptor_usages.Merge(other); }

    void Merge(Array<DescriptorUsage> &&other)
        { descriptor_usages.Merge(std::move(other)); }

    void Merge(const DescriptorUsageSet &other)
        { descriptor_usages.Merge(other.descriptor_usages); }

    void Merge(DescriptorUsageSet &&other)
        { descriptor_usages.Merge(std::move(other.descriptor_usages)); }

    DescriptorTableDeclaration BuildDescriptorTable() const;
};

struct ShaderDefinition
{
    Name                name;
    ShaderProperties    properties;
    DescriptorUsageSet  descriptor_usages;

    HYP_FORCE_INLINE
    Name GetName() const
        { return name; }

    HYP_FORCE_INLINE
    ShaderProperties &GetProperties()
        { return properties; }

    HYP_FORCE_INLINE
    const ShaderProperties &GetProperties() const
        { return properties; }

    HYP_FORCE_INLINE
    DescriptorUsageSet &GetDescriptorUsages()
        { return descriptor_usages; }

    HYP_FORCE_INLINE
    const DescriptorUsageSet &GetDescriptorUsages() const
        { return descriptor_usages; }

    HYP_FORCE_INLINE
    explicit operator bool() const
        { return name.IsValid(); }

    HYP_FORCE_INLINE
    bool IsValid() const
        { return name.IsValid(); }

    HYP_FORCE_INLINE
    bool operator==(const ShaderDefinition &other) const
        { return GetHashCode() == other.GetHashCode(); }

    HYP_FORCE_INLINE
    bool operator!=(const ShaderDefinition &other) const
        { return GetHashCode() != other.GetHashCode(); }

    HYP_FORCE_INLINE
    explicit operator HashedShaderDefinition() const
        { return HashedShaderDefinition { name, properties.GetPropertySetHashCode(), properties.GetRequiredVertexAttributes() }; }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        // ensure they return the same hash codes so they can be compared.
        return (operator HashedShaderDefinition()).GetHashCode();
    }
};

struct CompiledShader
{
    ShaderDefinition                                definition;
    String                                          entry_point_name = "main";
    HeapArray<ByteBuffer, ShaderModuleType::MAX>  modules;

    CompiledShader() = default;
    CompiledShader(ShaderDefinition shader_definition, String entry_point_name = "main")
        : definition(std::move(shader_definition)),
          entry_point_name(std::move(entry_point_name))
    {
    }

    CompiledShader(const CompiledShader &other)                 = default;
    CompiledShader &operator=(const CompiledShader &other)      = default;
    CompiledShader(CompiledShader &&other) noexcept             = default;
    CompiledShader &operator=(CompiledShader &&other) noexcept  = default;
    ~CompiledShader()                                           = default;

    explicit operator bool() const
        { return IsValid(); }

    bool IsValid() const
        { return modules.Any([](const ByteBuffer &buffer) { return buffer.Any(); }); }

    ShaderDefinition &GetDefinition()
        { return definition; }

    const ShaderDefinition &GetDefinition() const
        { return definition; }

    Name GetName() const
        { return definition.name; }

    const String &GetEntryPointName() const
        { return entry_point_name; }

    const ShaderProperties &GetProperties() const
        { return definition.properties; }

    const HeapArray<ByteBuffer, ShaderModuleType::MAX> &GetModules() const
        { return modules; }

    HashCode GetHashCode() const
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

    bool HasErrors() const
        { return error_messages.Any(); }

    HashCode GetHashCode() const
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

        ProcessResult() = default;
        ProcessResult(const ProcessResult &other) = default;
        ProcessResult &operator=(const ProcessResult &other) = default;
        ProcessResult(ProcessResult &&other) noexcept = default;
        ProcessResult &operator=(ProcessResult &&other) noexcept = default;
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

    struct Bundle // combination of shader files, .frag, .vert etc.
    {
        Name                                    name;
        String                                  entry_point_name = "main";
        FlatMap<ShaderModuleType, SourceFile>   sources;
        ShaderProperties                        versions; // permutations
        DescriptorUsageSet                      descriptor_usages;

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

    bool CanCompileShaders() const;

    bool LoadShaderDefinitions(bool precompile_shaders = false);

    CompiledShader GetCompiledShader(Name name);
    CompiledShader GetCompiledShader(Name name, const ShaderProperties &properties);

    bool GetCompiledShader(
        Name name,
        const ShaderProperties &properties,
        CompiledShader &out
    );

private:
    void GetPlatformSpecificProperties(
        ShaderProperties &out
    ) const;

    ProcessResult ProcessShaderSource(
        const String &source
    );

    void ParseDefinitionSection(
        const DefinitionsFile::Section &section,
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

    bool LoadOrCreateCompiledShaderBatch(
        Name name,
        const ShaderProperties &additional_versions,
        CompiledShaderBatch &out
    );

    DefinitionsFile *m_definitions;
    ShaderCache     m_cache;
    Array<Bundle>   m_bundles;
};

} // namespace hyperion::v2

#endif
