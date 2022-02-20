#include "text_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace hyperion {
AssetLoader::Result TextLoader::LoadFromFile(const std::string &filepath)
{
    std::ifstream file(filepath);

    if (!file.is_open()) {
        return AssetLoader::Result(AssetLoader::Result::ASSET_ERR, "File could not be opened.");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return AssetLoader::Result(std::make_shared<LoadedText>(buffer.str()));
}

std::shared_ptr<Loadable> TextLoader::LoadedText::Clone()
{
    return CloneImpl();
}

std::shared_ptr<TextLoader::LoadedText> TextLoader::LoadedText::CloneImpl()
{
    return std::make_shared<LoadedText>(text);
}
}
