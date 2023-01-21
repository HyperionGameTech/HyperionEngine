#ifndef HYPERION_V2_SHADER_COMPILER_H
#define HYPERION_V2_SHADER_COMPILER_H

#include <core/Containers.hpp>
#include <core/Name.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <util/definitions/DefinitionsFile.hpp>
#include <util/Defines.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <set>
#include <mutex>

namespace hyperion::v2 {

class Engine;

using renderer::ShaderModule;
using renderer::VertexAttribute;
using renderer::VertexAttributeSet;

using ShaderPropertyFlags = UInt32;

enum ShaderPropertyFlagBits : ShaderPropertyFlags
{
    SHADER_PROPERTY_FLAG_NONE = 0x0,
    SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE = 0x1
};

struct VertexAttributeDefinition
{
    String name;
    String type_class;
    Int location = -1;
    Optional<String> condition;
};

struct ShaderProperty;

template <class Container>
static bool FindVertexAttributePropertyByName(const Container &container, const char *name, bool &out_is_required) {
    auto it = container.FindIf([name](const auto &property) {
        return (property.flags & SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE)
            && property.name == name;
    });

    if (it != container.End()) {
        out_is_required = !it->is_permutation;
        return true;
    }

    return false;
}

static bool FindVertexAttributeForDefinition(const String &name, VertexAttribute::Type &out_type)
{
    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++) {
        const auto &kv = VertexAttribute::mapping.KeyValueAt(i);

        if (!kv.second.name) {
            continue;
        }

        if (name == kv.second.name) {
            out_type = kv.first;

            return true;
        }
    }

    return false;
};

template <class Container>
static void CalculateVertexAttributes(
    const Container &container,
    VertexAttributeSet &out_required_vertex_attributes,
    VertexAttributeSet &out_optional_vertex_attributes
)
{
    VertexAttributeSet required_vertex_attributes;
    VertexAttributeSet optional_vertex_attributes;

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++) {
        const auto &kv = VertexAttribute::mapping.KeyValueAt(i);

        if (!kv.second.name) {
            continue;
        }

        bool is_required = false;
        
        if (FindVertexAttributePropertyByName<Container>(container, kv.second.name, is_required)) {
            if (is_required) {
                required_vertex_attributes |= kv.first;
            } else {
                optional_vertex_attributes |= kv.first;
            }
        }
    }

    out_required_vertex_attributes = required_vertex_attributes;
    out_optional_vertex_attributes = optional_vertex_attributes;
}
struct ShaderProperty
{
    using Value = Variant<String, Int, Float>;

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

    bool HasValue() const
        { return value.IsValid(); }

    bool IsVertexAttribute() const
        { return flags & SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE; }

    String GetValueString() const
    {
        if (const String *str = value.TryGet<String>()) {
            return *str;
        } else if (const Int *i = value.TryGet<Int>()) {
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

class ShaderProps
{
    friend class ShaderCompiler;

public:
    ShaderProps() = default;

    // ShaderProps(const FlatSet<ShaderProperty> &props)
    //     : m_props(props)
    // {
    //     // CalculateVertexAttributes(m_props, m_required_vertex_attributes, m_optional_vertex_attributes);
    // }

    // ShaderProps(FlatSet<ShaderProperty> &&props)
    //     : m_props(std::move(props))
    // {
    //     // CalculateVertexAttributes(m_props, m_required_vertex_attributes, m_optional_vertex_attributes);
    // }

    ShaderProps(const Array<String> &props)
    {
        for (const String &prop_key : props) {
            m_props.Insert(ShaderProperty(prop_key, true)); // default to permutable
        }
    }

    ShaderProps(const Array<ShaderProperty> &props)
    {
        for (const ShaderProperty &property : props) {
            if (property.IsVertexAttribute()) {
                VertexAttribute::Type type;

                if (!FindVertexAttributeForDefinition(property.GetValueString(), type)) {
                    DebugLog(
                        LogType::Warn,
                        "Invalid vertex attribute name for shader: %s\n",
                        property.GetValueString().Data()
                    );

                    continue;
                }

                if (property.is_permutation) {
                    m_optional_vertex_attributes |= type;
                } else {
                    m_required_vertex_attributes |= type;
                }
            } else {
                m_props.Insert(property);
            }
        }

        // CalculateVertexAttributes(m_props, m_required_vertex_attributes, m_optional_vertex_attributes);
    }

    ShaderProps(const VertexAttributeSet &vertex_attributes, const Array<String> &props)
        : m_required_vertex_attributes(vertex_attributes)
    {
        // for (const auto &attribute : vertex_attributes.BuildAttributes()) {
        //     m_props.Insert(ShaderProperty(
        //         String("HYP_ATTRIBUTE_") + attribute.name,
        //         false,
        //         ShaderProperty::Value(String(attribute.name)),
        //         SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE
        //     ));
        // }

        for (const String &prop_key : props) {
            m_props.Insert(ShaderProperty(prop_key, true)); // default to permutable
        }
    }

    ShaderProps(const VertexAttributeSet &vertex_attributes)
        : ShaderProps(vertex_attributes, { })
    {
    }

    ShaderProps(const ShaderProps &other) = default;
    ShaderProps &operator=(const ShaderProps &other) = default;
    ShaderProps(ShaderProps &&other) noexcept = default;
    ShaderProps &operator=(ShaderProps &&other) = default;
    ~ShaderProps() = default;

    bool operator==(const ShaderProps &other) const
        { return m_props == other.m_props; }

    bool operator!=(const ShaderProps &other) const
        { return m_props == other.m_props; }

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

    ShaderProps &Set(const ShaderProperty &shader_property, bool enabled = true)
    {
        const auto it = m_props.Find(shader_property);

        if (enabled) {
            if (it == m_props.End()) {
                m_props.Insert(shader_property);
            } else {
                *it = shader_property;
            }
        } else {
            if (it != m_props.End()) {
                m_props.Erase(it);
            }
        }

        if (shader_property.IsVertexAttribute()) {
            // CalculateVertexAttributes(m_props, m_required_vertex_attributes, m_optional_vertex_attributes);
        }

        return *this;
    }

    ShaderProps &Set(const String &name, bool enabled = true)
    {
        return Set(ShaderProperty(name, true), enabled);
    }

    /*! \brief Applies \ref{other} properties onto this set */
    void Merge(const ShaderProps &other)
    {
        for (const ShaderProperty &property : other.m_props) {
            Set(property);
        }

        m_required_vertex_attributes |= other.m_required_vertex_attributes;
        m_optional_vertex_attributes |= other.m_optional_vertex_attributes;
    }

    const FlatSet<ShaderProperty> &GetPropertySet() const
        { return m_props; }

    VertexAttributeSet GetRequiredVertexAttributes() const
        { return m_required_vertex_attributes; }

    VertexAttributeSet GetOptionalVertexAttributes() const
        { return m_optional_vertex_attributes; }

    void SetRequiredVertexAttributes(VertexAttributeSet vertex_attributes)
    {
        // temp
        // for (auto it = m_props.Begin(); it != m_props.End();) {
        //     if (it->IsVertexAttribute()) {
        //         it = m_props.Erase(it);
        //     } else {
        //         ++it;
        //     }
        // }

        // m_required_vertex_attributes = VertexAttributeSet { };
        // Merge(ShaderProps(vertex_attributes));

        m_required_vertex_attributes = vertex_attributes;

        m_optional_vertex_attributes = m_optional_vertex_attributes & ~m_required_vertex_attributes;
    }

    void SetOptionalVertexAttributes(VertexAttributeSet vertex_attributes)
    {
        m_optional_vertex_attributes = vertex_attributes & ~m_required_vertex_attributes;
    }

    SizeType Size() const
        { return m_props.Size(); }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_required_vertex_attributes.GetHashCode());
        // hc.Add(m_optional_vertex_attributes.GetHashCode());

        for (const auto &it : m_props) {
            hc.Add(it);
        }

        return hc;
    }

    Array<ShaderProperty> ToArray() const
    {
        return m_props.ToArray();
    }

    String ToString() const
    {
        Array<String> property_names;

        for (const VertexAttribute &attribute : GetRequiredVertexAttributes().BuildAttributes()) {
            property_names.PushBack(String("HYP_ATTRIBUTE_") + attribute.name);
        }

        // for (const VertexAttribute &attribute : GetOptionalVertexAttributes().BuildAttributes()) {
        //     property_names.PushBack(String("HYP_ATTRIBUTE_") + attribute.name);
        // }

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

private:

    ShaderProps &AddPermutation(const String &key)
    {
        ShaderProperty shader_property(key, true);

        const auto it = m_props.Find(shader_property);

        if (it == m_props.End()) {
            m_props.Insert(shader_property);
        } else {
            *it = shader_property;
        }

        return *this;
    }

    ShaderProps &AddValueGroup(const String &key, const Array<String> &possible_values)
    {
        ShaderProperty shader_property(key, false);
        shader_property.possible_values = possible_values;

        const auto it = m_props.Find(shader_property);

        if (it == m_props.End()) {
            m_props.Insert(shader_property);
        } else {
            *it = shader_property;
        }

        return *this;
    }

    FlatSet<ShaderProperty> m_props;

    VertexAttributeSet m_required_vertex_attributes;
    VertexAttributeSet m_optional_vertex_attributes;
};

struct ShaderDefinition
{
    Name name;
    ShaderProps properties;

    Name GetName() const
        { return name; }

    ShaderProps &GetProperties()
        { return properties; }

    const ShaderProps &GetProperties() const
        { return properties; }

    explicit operator bool() const
        { return name.IsValid(); }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name.GetHashCode());
        hc.Add(properties.GetHashCode());

        return hc;
    }
};

struct CompiledShader
{
    ShaderDefinition definition;
    HeapArray<ByteBuffer, ShaderModule::Type::MAX> modules;

    CompiledShader() = default;
    CompiledShader(Name name, const ShaderProps &properties)
        : definition { name, properties }
    {
    }

    CompiledShader(const CompiledShader &other) = default;
    CompiledShader &operator=(const CompiledShader &other) = default;
    CompiledShader(CompiledShader &&other) noexcept = default;
    CompiledShader &operator=(CompiledShader &&other) noexcept = default;

    ~CompiledShader() = default;

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

    const ShaderProps &GetProperties() const
        { return definition.properties; }

    const HeapArray<ByteBuffer, ShaderModule::Type::MAX> &GetModules() const
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
    Array<CompiledShader> compiled_shaders;
    Array<String> error_messages;
    ShaderProps base_props;

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
    ShaderCache() = default;
    ShaderCache(const ShaderCache &other) = delete;
    ShaderCache &operator=(const ShaderCache &other) = delete;
    ShaderCache(ShaderCache &&other) noexcept = delete;
    ShaderCache &operator=(ShaderCache &&other) noexcept = delete;
    ~ShaderCache() = default;

    bool Get(Name name, CompiledShaderBatch &out) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_compiled_shaders.Find(name);

        if (it == m_compiled_shaders.End()) {
            return false;
        }

        out = it->value;

        return true;
    }

    bool GetShaderInstance(Name name, UInt64 version_hash, CompiledShader &out) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_compiled_shaders.Find(name);

        if (it == m_compiled_shaders.End()) {
            return false;
        }

        const auto version_it = it->value.compiled_shaders.FindIf([version_hash](const CompiledShader &item) {
            return item.definition.properties.GetHashCode().Value() == version_hash;
        });

        if (version_it == it->value.compiled_shaders.End()) {
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
    HashMap<Name, CompiledShaderBatch> m_compiled_shaders;
    mutable std::mutex m_mutex;
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
        String final_source;
        Array<ProcessError> errors;
        Array<VertexAttributeDefinition> required_attributes;
        Array<VertexAttributeDefinition> optional_attributes;
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
        Name name;
        FlatMap<ShaderModule::Type, SourceFile> sources;
        ShaderProps versions; // permutations

        bool HasRTShaders() const
        {
            return sources.Any([](const KeyValuePair<ShaderModule::Type, SourceFile> &item) {
                return ShaderModule::IsRaytracingType(item.first);
            });
        }

        bool IsComputeShader() const
        {
            return sources.Every([](const KeyValuePair<ShaderModule::Type, SourceFile> &item) {
                return item.first == ShaderModule::Type::COMPUTE;
            });
        }

        bool HasVertexShader() const
        {
            return sources.Any([](const KeyValuePair<ShaderModule::Type, SourceFile> &item) {
                return item.first == ShaderModule::Type::VERTEX;
            });
        }
    };

    ShaderCompiler();
    ShaderCompiler(const ShaderCompiler &other) = delete;
    ShaderCompiler &operator=(const ShaderCompiler &other) = delete;
    ~ShaderCompiler();

    bool CanCompileShaders() const;

    bool LoadShaderDefinitions();

    CompiledShader GetCompiledShader(Name name);
    CompiledShader GetCompiledShader(Name name, const ShaderProps &properties);

    bool GetCompiledShader(
        Name name,
        const ShaderProps &versions,
        CompiledShader &out
    );

private:
    void GetPlatformSpecificProperties(
        ShaderProps &out
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
        const ShaderProps &additional_versions,
        CompiledShaderBatch &out
    );

    bool HandleCompiledShaderBatch(
        Bundle &bundle,
        const ShaderProps &additional_versions,
        const FilePath &output_file_path,
        CompiledShaderBatch &batch
    );

    bool LoadOrCreateCompiledShaderBatch(
        Name name,
        const ShaderProps &additional_versions,
        CompiledShaderBatch &out
    );

    DefinitionsFile *m_definitions;
    ShaderCache m_cache;
    Array<Bundle> m_bundles;
};

} // namespace hyperion::v2

#endif