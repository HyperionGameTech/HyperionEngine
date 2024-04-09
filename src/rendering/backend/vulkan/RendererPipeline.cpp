#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <core/lib/FlatSet.hpp>
#include <core/lib/Memory.hpp>

#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

Pipeline<Platform::VULKAN>::Pipeline()
    : pipeline(VK_NULL_HANDLE),
      layout(VK_NULL_HANDLE),
      push_constants { }
{
}

Pipeline<Platform::VULKAN>::Pipeline(ShaderProgramRef<Platform::VULKAN> shader_program, DescriptorTableRef<Platform::VULKAN> descriptor_table)
    : m_shader_program(std::move(shader_program)),
      m_descriptor_table(std::move(descriptor_table)),
      pipeline(VK_NULL_HANDLE),
      layout(VK_NULL_HANDLE),
      push_constants { }
{
}

Pipeline<Platform::VULKAN>::~Pipeline()
{
    AssertThrowMsg(pipeline == VK_NULL_HANDLE, "Expected pipeline to have been destroyed");
    AssertThrowMsg(layout == VK_NULL_HANDLE, "Expected layout to have been destroyed");
}

void Pipeline<Platform::VULKAN>::SetDescriptorTable(DescriptorTableRef<Platform::VULKAN> descriptor_table)
{
    m_descriptor_table = std::move(descriptor_table);
}

void Pipeline<Platform::VULKAN>::SetShaderProgram(ShaderProgramRef<Platform::VULKAN> shader_program)
{
    m_shader_program = std::move(shader_program);
}

Array<VkDescriptorSetLayout> Pipeline<Platform::VULKAN>::GetDescriptorSetLayouts() const
{
    AssertThrowMsg(m_descriptor_table.IsValid(), "Invalid DescriptorTable provided to Pipeline");

    Array<VkDescriptorSetLayout> used_layouts;
    used_layouts.Reserve(m_descriptor_table->GetSets()[0].Size());

    for (const DescriptorSet2Ref<Platform::VULKAN> &descriptor_set : m_descriptor_table->GetSets()[0]) {
        AssertThrow(descriptor_set != nullptr);

        used_layouts.PushBack(descriptor_set->GetVkDescriptorSetLayout());
    }

    return used_layouts;
}

void Pipeline<Platform::VULKAN>::SetPushConstants(const void *data, SizeType size)
{
    AssertThrow(size <= sizeof(push_constants));

    Memory::MemCpy(&push_constants, data, size);

    if (size < sizeof(push_constants)) {
        Memory::MemSet(&push_constants + size, 0, sizeof(push_constants) - size);
    }
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
