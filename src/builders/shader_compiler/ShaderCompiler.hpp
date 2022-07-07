#ifndef HYPERION_V2_SHADER_COMPILER_H
#define HYPERION_V2_SHADER_COMPILER_H

#include <HashCode.hpp>

#include <set>
#include <string>
#include <vector>

namespace hyperion::v2 {

class ShaderProps {
public:
    bool Get(const std::string &key) const
    {
        const auto it = m_props.find(key);

        if (it == m_props.end()) {
            return false;
        }

        return true;
    }

    ShaderProps &Set(const std::string &key, bool value)
    {
        if (!value) {
            m_props.erase(key);
        } else {
            m_props.insert(key);
        }

        return *this;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_props) {
            hc.Add(it);
        }

        return hc;
    }

private:
    std::set<std::string> m_props;
};

class ShaderCompiler {
public:
    struct File {
        std::string path;
    };

    struct Bundle {
        std::vector<File> files;
        ShaderProps       props;
    };

    void AddBundle(Bundle &&bundle)
    {
        m_bundles.push_back(std::move(bundle));
    }

private:
    std::vector<Bundle> m_bundles;
};

} // namespace hyperion::v2

#endif