#include "mtl_loader.h"
#include "../../util/string_util.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"

#include <fstream>

/*
 Illumination    Properties that are turned on in the
 model           Property Editor

 0		Color on and Ambient off
 1		Color on and Ambient on
 2		Highlight on
 3		Reflection on and Ray trace on
 4		Transparency: Glass on
        Reflection: Ray trace on
 5		Reflection: Fresnel on and Ray trace on
 6		Transparency: Refraction on
        Reflection: Fresnel off and Ray trace on
 7		Transparency: Refraction on
        Reflection: Fresnel on and Ray trace on
 8		Reflection on and Ray trace off
 9		Transparency: Glass on
        Reflection: Ray trace off
 10		Casts shadows onto invisible surfaces
 */

namespace hyperion {
std::shared_ptr<Loadable> MtlLib::Clone()
{
    std::shared_ptr<MtlLib> new_mtl = std::make_shared<MtlLib>();
    new_mtl->m_materials = m_materials;
    return new_mtl;
}

AssetLoader::Result MtlLoader::LoadFromFile(const std::string &path)
{
    std::shared_ptr<MtlLib> mtl = std::make_shared<MtlLib>();

    auto loaded_text_result = TextLoader().LoadFromFile(path);

    if (!loaded_text_result) {
        return loaded_text_result;
    }

    auto loaded_text = std::dynamic_pointer_cast<TextLoader::LoadedText>(loaded_text_result.Value());

    if (loaded_text == nullptr) {
        return AssetLoader::Result(AssetLoader::Result::ASSET_ERR, "Could not load text file");
    }

    std::vector<std::string> lines = StringUtil::Split(loaded_text->GetText(), '\n');

    for (auto &line : lines) {
        line = StringUtil::Trim(line);

        auto tokens = StringUtil::Split(line, ' ');
        tokens = StringUtil::RemoveEmpty(tokens);

        if (!tokens.empty() && tokens[0] != "#") {
            if (tokens[0] == "newmtl") {
                if (tokens.size() >= 2) {
                    std::string name = tokens[1];
                    mtl->NewMaterial(name);
                }
            } else if (tokens[0] == "map_Kd") { // diffuse map
                if (tokens.size() >= 2) {
                    const std::string loc = tokens[1];

                    if (Material *last_mtl = mtl->GetLastMaterial()) {
                        if (auto tex = LoadTexture(loc, path)) {
                            last_mtl->SetTexture("DiffuseMap", tex);
                        }
                    }
                }
            } else if (tokens[0] == "map_bump") {
                if (tokens.size() >= 2) {
                    const std::string loc = tokens[1];

                    if (Material *last_mtl = mtl->GetLastMaterial()) {
                        if (auto tex = LoadTexture(loc, path)) {
                            last_mtl->SetTexture("NormalMap", tex);
                        }
                    }
                }
            } else if (tokens[0] == "map_Ks") {
                if (tokens.size() >= 2) {
                    const std::string loc = tokens[1];

                    if (Material *last_mtl = mtl->GetLastMaterial()) {
                        if (auto tex = LoadTexture(loc, path)) {
                            last_mtl->SetTexture("MetalnessMap", tex);
                        }
                    }
                }
            } else if (tokens[0] == "Kd") {
                if (tokens.size() >= 4) {
                    Vector4 color(1.0);

                    color.x = std::stof(tokens[1]);
                    color.y = std::stof(tokens[2]);
                    color.z = std::stof(tokens[3]);

                    if (tokens.size() >= 5) {
                        color.w = std::stof(tokens[4]);
                    }

                    if (Material *last_mtl = mtl->GetLastMaterial()) {
                        last_mtl->diffuse_color = color;
                    }
                }
            } else if (tokens[0] == "illum") {
                if (tokens.size() >= 2) {
                    int model = std::stoi(tokens[1]);

                    if (model >= 4) {
                        if (Material *last_mtl = mtl->GetLastMaterial()) {
                            last_mtl->SetParameter(MATERIAL_PARAMETER_METALNESS, float(model) / 9.0f); // kinda arb but gives a place to start
                        }
                    }
                }
            } else if (tokens[0] == "Ns") {
                if (tokens.size() >= 2) {
                    int spec_amount = std::stoi(tokens[1]);

                    if (Material *last_mtl = mtl->GetLastMaterial()) {
                        last_mtl->SetParameter(MATERIAL_PARAMETER_EMISSIVENESS, float(spec_amount) / 100.0f);
                    }
                }
            }
        }
    }

    return AssetLoader::Result(mtl);
}

std::shared_ptr<Texture> MtlLoader::LoadTexture(const std::string &name, const std::string &path)
{
    std::shared_ptr<Texture> tex;

    std::string dir(path);
    dir = dir.substr(0, dir.find_last_of("\\/"));
    if (!(StringUtil::Contains(dir, "/") || StringUtil::Contains(dir, "\\"))) {
        dir.clear();
    }
    dir += "/" + name;

    if (!(tex = AssetManager::GetInstance()->LoadFromFile<Texture>(dir))) {
        // load relatively
        tex = AssetManager::GetInstance()->LoadFromFile<Texture>(name);
    }

    return tex;
}
}
