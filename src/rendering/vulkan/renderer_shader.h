//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_SHADER_H
#define HYPERION_RENDERER_SHADER_H

#include "../backend/spirv.h"
#include "renderer_device.h"

namespace hyperion {

struct RendererShaderModule {
    SPIRVObject::Type type;
    VkShaderModule module;
};

class RendererShader {
public:
    void AttachShader(RendererDevice *device, const SPIRVObject &spirv);
    //void AttachShader(RendererDevice *device, ShaderType type, const uint32_t *code, const size_t code_size);
    VkPipelineShaderStageCreateInfo CreateShaderStage(RendererShaderModule *module, const char *entry_point);
    void CreateProgram(const char *entry_point);
    void Destroy();

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
private:
    std::vector<RendererShaderModule> shader_modules;
    RendererDevice *device = nullptr;
};

}; /* namespace hyperion */

#endif //HYPERION_RENDERER_SHADER_H
