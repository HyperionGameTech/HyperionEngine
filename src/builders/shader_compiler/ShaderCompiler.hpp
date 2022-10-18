#ifndef HYPERION_V2_SHADER_COMPILER_H
#define HYPERION_V2_SHADER_COMPILER_H

#include <core/Containers.hpp>
#include <HashCode.hpp>
#include <util/Defines.hpp>

#include <set>
#include <string>
#include <vector>

namespace hyperion::v2 {

class ShaderProps
{
public:
    using Iterator = FlatSet<String>::Iterator;
    using ConstIterator = FlatSet<String>::ConstIterator;

    bool Get(const String &key) const
    {
        const auto it = m_props.Find(key);

        if (it == m_props.End()) {
            return false;
        }

        return true;
    }

    ShaderProps &Set(const String &key, bool value)
    {
        if (!value) {
            m_props.Erase(key);
        } else {
            m_props.Insert(key);
        }

        return *this;
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
    FlatSet<String> m_props;
};

class ShaderCompiler
{
public:
    struct SourceFile
    {
        String path;

        HashCode GetHashCode() const
        {
            // TODO: should it include timestamp of last modified?
            HashCode hc;
            hc.Add(path);

            return hc;
        }
    };

    struct Bundle // combination of shader files, .frag, .vert etc.
    {
        DynArray<SourceFile> files;
        ShaderProps props; // permutations
    };

    void AddBundle(Bundle &&bundle)
    {
        m_bundles.PushBack(std::move(bundle));
    }

    void CompileBundles();

private:
    bool CompileBundle(Bundle &bundle);

    DynArray<Bundle> m_bundles;
};

} // namespace hyperion::v2

#endif