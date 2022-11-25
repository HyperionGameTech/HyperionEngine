#ifndef HYPERION_V2_SHADER_COMPILER_H
#define HYPERION_V2_SHADER_COMPILER_H

#include <core/Containers.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <util/definitions/DefinitionsFile.hpp>
#include <util/Defines.hpp>
#include <HashCode.hpp>

#include <set>
#include <mutex>

namespace hyperion::v2 {

class Engine;

using renderer::ShaderModule;
using renderer::VertexAttributeSet;

struct ShaderProperty
{
    String name;
    bool is_permutation = false;
    bool is_global = false;

    ShaderProperty() = default;

    ShaderProperty(const String &name, bool is_permutation, bool is_global = false)
        : name(name),
          is_permutation(is_permutation),
          is_global(is_global)
    {
    }

    ShaderProperty(const ShaderProperty &other)
        : name(other.name),
          is_permutation(other.is_permutation),
          is_global(other.is_global)
    {
    }

    ShaderProperty &operator=(const ShaderProperty &other)
    {
        name = other.name;
        is_permutation = other.is_permutation;
        is_global = other.is_global;

        return *this;
    }

    ShaderProperty(ShaderProperty &&other) noexcept
        : name(std::move(other.name)),
          is_permutation(other.is_permutation),
          is_global(other.is_global)
    {
        other.is_permutation = false;
        other.is_global = false;
    }

    ShaderProperty &operator=(ShaderProperty &&other) noexcept
    {
        name = std::move(other.name);
        is_permutation = other.is_permutation;
        is_global = other.is_global;

        other.is_permutation = false;
        other.is_global = false;

        return *this;
    }

    bool operator==(const ShaderProperty &other) const
        { return name == other.name; }

    bool operator==(const String &str) const
        { return name == str; }

    bool operator<(const ShaderProperty &other) const
        { return name < other.name; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name.GetHashCode());
        // hashcode does not depend on is_permuation.

        return hc;
    }
};

class ShaderProps
{
    friend class ShaderCompiler;

public:
    using Iterator = FlatSet<ShaderProperty>::Iterator;
    using ConstIterator = FlatSet<ShaderProperty>::ConstIterator;

    ShaderProps() = default;

    ShaderProps(const FlatSet<ShaderProperty> &props)
        : m_props(props)
    {
    }

    ShaderProps(FlatSet<ShaderProperty> &&props)
        : m_props(std::move(props))
    {
    }

    ShaderProps(const Array<String> &props)
    {
        for (const String &prop_key : props) {
            m_props.Insert(ShaderProperty(prop_key, true)); // default to permutable
        }
    }

    ShaderProps(const VertexAttributeSet &vertex_attributes, const Array<String> &props)
    {
        for (const auto &attribute : vertex_attributes.BuildAttributes()) {
            m_props.Insert(ShaderProperty(String("HYP_ATTRIBUTE_") + attribute.name, false));
        }

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

    bool Any() const
        { return m_props.Any(); }

    bool Empty() const
        { return m_props.Empty(); }

    bool Get(const String &key) const
    {
        ShaderProperty shader_property(key, false);

        const auto it = m_props.Find(shader_property);

        if (it == m_props.End()) {
            return false;
        }

        return true;
    }

    bool Get(const ShaderProperty &shader_property) const
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

        return *this;
    }

    ShaderProps &Set(const String &name, bool enabled = true)
    {
        return Set(ShaderProperty(name, true), enabled);
    }

    /*! \brief Applies \ref{other} properties onto this set */
    void Merge(const ShaderProps &other)
    {
        for (const ShaderProperty &property : other) {
            Set(property);
        }
    }

    SizeType NumPermutations() const
    {
        SizeType num_permutable_properties = 0;

        for (const ShaderProperty &property : m_props) {
            if (property.is_permutation) {
                ++num_permutable_properties;
            }
        }

        return 1ull << num_permutable_properties;
    }

    SizeType Size() const
        { return m_props.Size(); }

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_props) {
            hc.Add(it);
        }

        return hc;
    }

    HYP_DEF_STL_BEGIN_END(
        m_props.Begin(),
        m_props.End()
    )

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

    FlatSet<ShaderProperty> m_props;
};

struct CompiledShader
{
    UInt64 version_hash = ~0u;
    HeapArray<ByteBuffer, ShaderModule::Type::MAX> modules;

    explicit operator bool() const
        { return IsValid(); }

    bool IsValid() const
    {
        return version_hash != ~0u
            && modules.Any([](const ByteBuffer &buffer) { return buffer.Size() != 0; });
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(version_hash);
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

    bool Get(const String &key, CompiledShaderBatch &out) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_compiled_shaders.Find(key);

        if (it == m_compiled_shaders.End()) {
            return false;
        }

        out = it->second;

        return true;
    }

    bool GetShaderInstance(const String &key, UInt64 version_hash, CompiledShader &out) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_compiled_shaders.Find(key);

        if (it == m_compiled_shaders.End()) {
            return false;
        }

        const auto version_it = it->second.compiled_shaders.FindIf([version_hash](const CompiledShader &item) {
            return item.version_hash == version_hash;
        });

        if (version_it == it->second.compiled_shaders.End()) {
            return false;
        }

        out = *version_it;

        return true;
    }

    void Set(const String &key, const CompiledShaderBatch &batch)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Set(key, batch);
    }

    void Set(const String &key, CompiledShaderBatch &&batch)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Set(key, std::move(batch));
    }

    void Remove(const String &key)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Erase(key);
    }

private:
    FlatMap<String, CompiledShaderBatch> m_compiled_shaders;
    mutable std::mutex m_mutex;
};

class ShaderCompiler
{
    static constexpr SizeType max_permutations = 2048;//temporalily increased, until some properties are "always enabled"

    struct VertexAttributeDefinition
    {
        String name;
        String type_class;
        Int location = -1;
        Optional<String> condition;
    };

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
        String name;
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

    ShaderCompiler(Engine *engine);
    ShaderCompiler(const ShaderCompiler &other) = delete;
    ShaderCompiler &operator=(const ShaderCompiler &other) = delete;
    ~ShaderCompiler();

    bool CanCompileShaders() const;

    bool LoadShaderDefinitions();

    CompiledShader GetCompiledShader(
        const String &name
    );

    CompiledShader GetCompiledShader(
        const String &name,
        const ShaderProps &versions
    );

    CompiledShader GetCompiledShader(
        const String &name,
        const ShaderProps &versions,
        const VertexAttributeSet &vertex_attributes
    );

    bool GetCompiledShader(
        const String &name,
        const ShaderProps &versions,
        const VertexAttributeSet &vertex_attributes,
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
        const Bundle &bundle,
        const ShaderProps &additional_versions,
        CompiledShaderBatch &out
    );

    bool HandleCompiledShaderBatch(
        const Bundle &bundle,
        const ShaderProps &additional_versions,
        const FilePath &output_file_path,
        CompiledShaderBatch &batch
    );

    bool LoadOrCreateCompiledShaderBatch(
        const String &name,
        const ShaderProps &additional_versions,
        CompiledShaderBatch &out
    );

    DefinitionsFile *m_definitions;
    ShaderCache m_cache;
    Array<Bundle> m_bundles;
};

} // namespace hyperion::v2

#endif