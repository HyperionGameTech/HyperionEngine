#include "shader.h"
#include "../util/string_util.h"
#include "../util/shader_preprocessor.h"
#include "../util.h"
#include "../gl_util.h"

namespace hyperion {

std::string EnumerateLines(const std::string &str);

Shader::Shader(const ShaderProperties &properties)
    : m_properties(properties),
      m_previous_properties_hash_code(properties.GetHashCode().Value()),
      m_override_cull(MaterialFaceCull::MaterialFace_None),
      is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
    ResetUniforms();
}

Shader::Shader(const ShaderProperties &properties,
    const std::string &vscode,
    const std::string &fscode)
    : m_properties(properties),
      m_previous_properties_hash_code(properties.GetHashCode().Value()),
      m_override_cull(MaterialFaceCull::MaterialFace_None),
      is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
    AddSubShader(SubShaderType::SUBSHADER_VERTEX, vscode, properties, "");
    AddSubShader(SubShaderType::SUBSHADER_FRAGMENT, fscode, properties, "");

    ResetUniforms();
}

Shader::~Shader()
{
    DestroyGpuData();
}

void Shader::CreateGpuData()
{
    AssertThrow(!is_created);

    progid = glCreateProgram();

    CatchGLErrors("Failed to create shader program.");

    for (auto &&sub : subshaders) {
        sub.second.id = glCreateShader(sub.first);

        CatchGLErrors("Failed to create subshader.");
    }

    is_created = true;
}

void Shader::UploadGpuData()
{
    AssertThrow(is_created && !is_uploaded);

    for (auto &&it : subshaders) {
        auto &sub = it.second;

        const char *code_str = sub.processed_code.c_str();
        glShaderSource(sub.id, 1, &code_str, NULL);
        glCompileShader(sub.id);
        glAttachShader(progid, sub.id);

        int status = -1;
        glGetShaderiv(sub.id, GL_COMPILE_STATUS, &status);

        if (!status) {
            int maxlen;
            glGetShaderiv(sub.id, GL_INFO_LOG_LENGTH, &maxlen);
            char *log = new char[maxlen];
            memset(log, 0, maxlen);
            glGetShaderInfoLog(sub.id, maxlen, NULL, log);

            std::cout << "Pre-processed shader code:\n\n";

            for (auto&& it : subshaders) {
                std::cout << "===================\n";

                switch (it.first) {
                case SubShaderType::SUBSHADER_FRAGMENT:
                    std::cout << "  FRAGMENT SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_VERTEX:
                    std::cout << "   VERTEX SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_GEOMETRY:
                    std::cout << "  GEOMETRY SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_COMPUTE:
                    std::cout << "  COMPUTE SHADER\n";
                    break;
                }

                std::cout << "===================\n";
                std::cout << EnumerateLines(it.second.processed_code);

                std::cout << "\n\n";
            }

            std::cout << "\n\n\n";

            std::cout << "In shader of class " << typeid(*this).name() << ":\n";
            std::cout << "\tShader compile error! ";
            std::cout << "\tCompile log: \n" << log << "\n";


            delete[] log;
        }
    }

    glBindFragDataLocation(progid, 0, "output0");
    glBindFragDataLocation(progid, 1, "output1");
    glBindFragDataLocation(progid, 2, "output2");
    glBindFragDataLocation(progid, 3, "output3");
    glBindFragDataLocation(progid, 4, "output4");
    glBindFragDataLocation(progid, 5, "output5");
    CatchGLErrors("Failed to bind shader frag data.");

    glBindAttribLocation(progid, 0, "a_position");
    glBindAttribLocation(progid, 1, "a_normal");
    glBindAttribLocation(progid, 2, "a_texcoord0");
    glBindAttribLocation(progid, 3, "a_texcoord1");
    glBindAttribLocation(progid, 4, "a_tangent");
    glBindAttribLocation(progid, 5, "a_bitangent");
    glBindAttribLocation(progid, 6, "a_boneweights");
    glBindAttribLocation(progid, 7, "a_boneindices");
    CatchGLErrors("Failed to bind shader attributes.");

    glLinkProgram(progid);
    glValidateProgram(progid);

    int linked = 0;
    glGetProgramiv(progid, GL_LINK_STATUS, &linked);

    if (!linked) {
        int maxlen = 0;
        glGetProgramiv(progid, GL_INFO_LOG_LENGTH, &maxlen);

        if (maxlen != 0) {
            char *log = new char[maxlen];

            std::cout << "Pre-processed shader code:\n\n";

            for (auto &&it : subshaders) {
                std::cout << "===================\n";

                switch (it.first) {
                case SubShaderType::SUBSHADER_FRAGMENT:
                    std::cout << "  FRAGMENT SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_VERTEX:
                    std::cout << "   VERTEX SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_GEOMETRY:
                    std::cout << "  GEOMETRY SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_COMPUTE:
                    std::cout << "  COMPUTE SHADER\n";
                }

                std::cout << "===================\n";
                std::cout << EnumerateLines(it.second.processed_code);

                std::cout << "\n\n";
            }

            glGetProgramInfoLog(progid, maxlen, NULL, log);

            std::cout << "\n\n\n";

            std::cout << "In shader of class " << typeid(*this).name() << ":\n";
            std::cout << "\tShader linker error! ";
            std::cout << "\tCompile log: \n" << log << "\n";

            glDeleteProgram(progid);

            delete[] log;

            return;
        }
    }

    is_uploaded = true;
}

void Shader::DestroyGpuData()
{
    if (is_created) {
        glDeleteProgram(progid);

        for (auto &&sub : subshaders) {
            glDeleteShader(sub.second.id);
        }
    }

    is_created = false;
    is_uploaded = false;
}

bool Shader::ShaderPropertiesChanged() const
{
    // TODO: memoization
    return m_properties.GetHashCode().Value() != m_previous_properties_hash_code;
}

void Shader::ResetUniforms()
{
    SetUniform("HasDiffuseMap", 0);
    SetUniform("HasNormalMap", 0);
    SetUniform("HasParallaxMap", 0);
    SetUniform("HasAoMap", 0);
    SetUniform("HasBrdfMap", 0);
    SetUniform("HasMetalnessMap", 0);
    SetUniform("HasRoughnessMap", 0);
}

void Shader::ApplyMaterial(const Material &mat)
{
    ResetUniforms();

    MaterialFaceCull cull_mode(mat.cull_faces);

    if (m_override_cull != MaterialFaceCull::MaterialFace_None) {
        cull_mode = m_override_cull;
    }

    if (cull_mode == (MaterialFaceCull::MaterialFace_Front | MaterialFaceCull::MaterialFace_Back)) {
        glCullFace(GL_FRONT_AND_BACK);
    } else if (cull_mode & MaterialFaceCull::MaterialFace_Front) {
        glCullFace(GL_FRONT);
    } else if (cull_mode & MaterialFaceCull::MaterialFace_Back) {
        glCullFace(GL_BACK);
    } else if (cull_mode == MaterialFaceCull::MaterialFace_None) {
        glDisable(GL_CULL_FACE);
    }

    if (mat.alpha_blended) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (!mat.depth_test) {
        glDisable(GL_DEPTH_TEST);
    }
    if (!mat.depth_write) {
        glDepthMask(false);
    }

    const auto &param = mat.GetParameter(MATERIAL_PARAMETER_FLIP_UV);
    SetUniform("FlipUV_X", int(param[0]));
    SetUniform("FlipUV_Y", int(param[1]));
}

void Shader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    SetUniform("u_modelMatrix", transform.GetMatrix());
    SetUniform("u_viewMatrix", camera->GetViewMatrix());
    SetUniform("u_projMatrix", camera->GetProjectionMatrix());
    SetUniform("u_viewProjMatrix", camera->GetViewProjectionMatrix());
}

void Shader::ApplyUniforms()
{
    if (uniform_changed) {
        int texture_index = 1;

        for (auto &&uniform : uniforms) {
            int loc = glGetUniformLocation(progid, uniform.first.c_str());

            if (loc != -1) {
                switch (uniform.second.type) {
                case Uniform::Uniform_Float:
                    glUniform1f(loc, uniform.second.data[0]);
                    break;
                case Uniform::Uniform_Int:
                    glUniform1i(loc, (int)uniform.second.data[0]);
                    break;
                case Uniform::Uniform_Vector2:
                    glUniform2f(loc, uniform.second.data[0], uniform.second.data[1]);
                    break;
                case Uniform::Uniform_Vector3:
                    glUniform3f(loc, uniform.second.data[0], uniform.second.data[1],
                        uniform.second.data[2]);
                    break;
                case Uniform::Uniform_Vector4:
                    glUniform4f(loc, uniform.second.data[0], uniform.second.data[1],
                        uniform.second.data[2], uniform.second.data[3]);
                    break;
                case Uniform::Uniform_Matrix4:
                    glUniformMatrix4fv(loc, 1, true, &uniform.second.data[0]);
                    break;
                case Uniform::Uniform_Texture2D:
                    Texture::ActiveTexture(texture_index);
                    glBindTexture(GL_TEXTURE_2D, int(uniform.second.data[0]));
                    glUniform1i(loc, texture_index);
                    texture_index++;
                    break;
                case Uniform::Uniform_Texture3D:
                    Texture::ActiveTexture(texture_index);
                    glBindTexture(GL_TEXTURE_3D, int(uniform.second.data[0]));
                    glUniform1i(loc, texture_index);
                    texture_index++;
                    break;
                case Uniform::Uniform_TextureCube:
                    Texture::ActiveTexture(texture_index);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, int(uniform.second.data[0]));
                    glUniform1i(loc, texture_index);
                    texture_index++;
                    break;
                default:
                    std::cout << "invalid uniform: " << uniform.first << "\n";
                    break;
                }

                CatchGLErrors((uniform.first + ": Failed to set uniform").c_str(), false);
            }
        }

        uniform_changed = false;
    }
}

void Shader::Use()
{
    if (!is_created) {
        CreateGpuData();
    }

    if (!is_uploaded) {
        UploadGpuData();
    } else if (ShaderPropertiesChanged()) {
        DestroyGpuData();

        for (auto &sub_shader : subshaders) {
            ReprocessSubShader(sub_shader.second, m_properties);
        }

        CreateGpuData();
        UploadGpuData();

        m_previous_properties_hash_code = m_properties.GetHashCode().Value();
    }

    glUseProgram(progid);

    ApplyUniforms();
}

void Shader::End()
{
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Shader::AddSubShader(SubShaderType type,
    const std::string &code,
    const ShaderProperties &properties,
    const std::string &path)
{
    SubShader sub_shader;
    sub_shader.id = 0;
    sub_shader.type = type;
    sub_shader.code = code;
    sub_shader.path = path;
    sub_shader.processed_code = ShaderPreprocessor::ProcessShader(code, properties, path);
    subshaders[type] = sub_shader;
}

void Shader::ReprocessSubShader(SubShader &sub_shader, const ShaderProperties &properties)
{
    sub_shader.processed_code = ShaderPreprocessor::ProcessShader(sub_shader.code, properties, sub_shader.path);
}

std::string EnumerateLines(const std::string &str)
{
    std::string result;
    result.reserve(str.length());

    std::vector<std::string> lines = StringUtil::Split(str, '\n');

    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].length() == 0) {
            if (i != 0 && lines[i - 1].length() == 0) {
                // no-op
            } else if (i != lines.size() - 1 && lines[i + 1].length() == 0) {
                result += " ... ";
            } 
        } else {
            result += "[" + std::to_string(i + 1) + "]: " + lines[i];
        }

        result += "\n";
    }

    return result;
}

} // namespace hyperion
