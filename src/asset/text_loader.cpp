#include "text_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace hyperion {
std::shared_ptr<Loadable> TextLoader::LoadFromFile(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return std::make_shared<LoadedText>(buffer.str());
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
