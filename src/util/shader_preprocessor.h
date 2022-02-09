#ifndef SHADER_PREPROCESSOR_H
#define SHADER_PREPROCESSOR_H

#include <string>
#include <sstream>

namespace hyperion {
class ShaderProperties;

class ShaderPreprocessor {
public:
    static std::string ProcessShader(const std::string &code, 
        const ShaderProperties &shader_properties,
        const std::string &path = "");

private:
    static std::string ProcessInner(std::istringstream &is, 
        std::streampos &pos, 
        const ShaderProperties &shader_properties,
        const std::string &local_path);

    static std::string FileHeader(const std::string &path);
};

} // namespace hyperion

#endif
