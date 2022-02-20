#ifndef MTL_LOADER_H
#define MTL_LOADER_H

#include "../asset_loader.h"
#include "../../rendering/material.h"

#include <vector>
#include <utility>
#include <algorithm>
#include <string>
#include <algorithm>

namespace hyperion {

class MtlLib : public Loadable {
public:
    virtual ~MtlLib() = default;

    inline void NewMaterial(const std::string &name)
    {
        m_materials.push_back({ name, std::make_shared<Material>() });
    }

    inline Material *GetMaterial(const std::string &name)
    {
        const auto it = std::find_if(m_materials.begin(), m_materials.end(), [name](auto p) {
            return p.first == name;
        });

        if (it == m_materials.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    inline Material *GetLastMaterial()
    {
        if (m_materials.empty()) {
            return nullptr;
        }

        return m_materials.back().second.get();
    }

    virtual std::shared_ptr<Loadable> Clone() override;

private:
    std::vector<std::pair<std::string, std::shared_ptr<Material>>> m_materials;
};

class MtlLoader : public AssetLoader {
public:
    virtual ~MtlLoader() = default;

    virtual Result LoadFromFile(const std::string &) override;

private:
    std::shared_ptr<Texture> LoadTexture(const std::string &name, const std::string &path);
};
}

#endif
