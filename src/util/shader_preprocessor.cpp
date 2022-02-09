#include "shader_preprocessor.h"
#include "../rendering/shader.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"
#include "string_util.h"

#include <iostream>

namespace hyperion {

std::string ShaderPreprocessor::ProcessShader(const std::string &code, 
    const ShaderProperties &shader_properties, 
    const std::string &path)
{
    std::istringstream ss(code);
    std::ostringstream os;
    std::streampos pos;
    std::string line;

    std::string local_path(path.substr(0, path.find_last_of("\\/")));

    if (StringUtil::StartsWith(local_path, "/") || StringUtil::StartsWith(local_path, "\\")) {
        local_path.clear(); // non-relative path
    }

    return FileHeader(path) + ProcessInner(ss, pos, shader_properties, local_path);
}

std::string ShaderPreprocessor::ProcessInner(std::istringstream &ss, 
    std::streampos &pos,
    const ShaderProperties &shader_properties,
    const std::string &local_path)
{
    ShaderProperties defines(shader_properties);

    std::string res;

    std::string line;

    while (std::getline(ss, line)) {
        line = StringUtil::Trim(line);

        std::string new_line;

        if (StringUtil::StartsWith(line, "#define $")) {
            std::string sub = line.substr(9);

            const std::vector<std::string> components = StringUtil::Split(sub, ' ');

            std::vector<std::string> new_components;
            new_components.reserve(components.size());

            for (const auto &item : components) {
                const auto trimmed = StringUtil::Trim(item);

                if (!trimmed.empty()) {
                    new_components.push_back(trimmed);
                }
            }

            if (new_components.size() >= 2) {
                std::string key = new_components[0];
                std::string value = line.substr(9 + key.size());

                ShaderProperties::ShaderProperty::Value result;

                if (new_components.size() == 2) {
                    if (StringUtil::ParseNumber<float>(value, &result.float_value)) {
                        if (MathUtil::Round(result.float_value) == result.float_value) {
                            defines.Define(key, int(result.float_value));
                        } else {
                            defines.Define(key, result.float_value);
                        }
                    } else if (StringUtil::ParseNumber<int>(value, &result.int_value)) {
                        defines.Define(key, result.int_value);
                    } else if (StringUtil::ParseNumber<bool>(value, &result.bool_value)) {
                        defines.Define(key, result.bool_value);
                    } else {
                        defines.Define(key, value);
                    }
                } else {
                    defines.Define(key, value);
                }
            } else {
                new_line += "#error \"The `#define $` directive must be defined in the format: `#define $NAME value`\"";
            }
        } else if (StringUtil::StartsWith(line, "#if !")) {
            std::string key = line.substr(5);
            std::string inner = ProcessInner(ss, pos, defines, local_path);

            if (!defines.GetValue(key).IsTruthy()) {
                new_line += inner;
            }
        } else if (StringUtil::StartsWith(line, "#if ")) {
            std::string key = line.substr(4);
            std::string inner = ProcessInner(ss, pos, defines, local_path);

            if (defines.GetValue(key).IsTruthy()) {
                new_line += inner;
            }
        } else if (StringUtil::StartsWith(line, "#endif")) {
            break; // exit current recursion
        } else if (StringUtil::StartsWith(line, "#include ")) {
            std::string val = line.substr(9);
            std::string path;

            if (val[0] == '\"') {
                int idx = 1;
                while (idx < val.size() && val[idx] != '\"') {
                    path += val[idx];
                    ++idx;
                }
            }

            std::string include_path;
            
            if (local_path.length() != 0) {
                include_path = local_path + "/" + path;
            } else {
                include_path = path;
            }

            if (auto loaded = AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(include_path)) {
                new_line += ProcessShader(loaded->GetText(), defines, include_path) + "\n";
                new_line += "\n";
                new_line += FileHeader(local_path);
            } else {
                new_line += "#error \"The include could not be found at: ";
                new_line += include_path;
                new_line += "\"";
            }
        } else {
            new_line += line;
        }

        res += new_line + "\n";
    }

    std::string res_processed;
    res_processed.reserve(res.length());

    for (size_t i = 0; i < res.length(); i++) {
        char ch = res[i];
        if (ch == '$') {
            size_t start_pos = i;
            std::string str = "";

            while (i + 1 < res.length() && (std::isalnum(res[i + 1]) || res[i + 1] == '_')) {
                ch = res[++i];
                str += ch;
            }

            if (defines.HasValue(str)) {
                auto value = defines.GetValue(str);
                res_processed += value.raw_value;
            } else {
                res_processed += "__UNDEFINED__";
            }
        } else {
            res_processed += ch;
        }
    }

    return res_processed;
}

std::string ShaderPreprocessor::FileHeader(const std::string &path)
{
    std::string header = "/* ===== ";
    header += path;
    header += " ===== */\n";
    return header;
}

} // namespace hyperion
