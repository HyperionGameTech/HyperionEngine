/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_PIPELINE_HPP
#define HYPERION_BACKEND_RENDERER_PIPELINE_HPP

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/containers/Array.hpp>
#include <core/utilities/Optional.hpp>
#include <core/Defines.hpp>

#include <math/Vector4.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
struct PipelinePlatformImpl;

template <PlatformType PLATFORM>
class Pipeline
{
public:
    friend struct PipelinePlatformImpl<PLATFORM>;

    HYP_API Pipeline();
    HYP_API Pipeline(const ShaderRef<PLATFORM> &shader, const DescriptorTableRef<PLATFORM> &descriptor_table);
    Pipeline(const Pipeline &other)             = delete;
    Pipeline &operator=(const Pipeline &other)  = delete;
    HYP_API ~Pipeline();
    
    HYP_API Result Destroy(Device<PLATFORM> *device);

    HYP_API bool IsCreated() const;

    [[nodiscard]]
    HYP_FORCE_INLINE
    PipelinePlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const PipelinePlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const DescriptorTableRef<PLATFORM> &GetDescriptorTable() const
        { return m_descriptor_table; }

    HYP_API void SetDescriptorTable(const DescriptorTableRef<PLATFORM> &descriptor_table);

    [[nodiscard]]
    HYP_FORCE_INLINE
    const ShaderRef<PLATFORM> &GetShader() const
        { return m_shader; }

    HYP_API void SetShader(const ShaderRef<PLATFORM> &shader);
    
    HYP_API void SetPushConstants(const void *data, SizeType size);

protected:
    PipelinePlatformImpl<PLATFORM>  m_platform_impl;

    ShaderRef<PLATFORM>             m_shader;
    DescriptorTableRef<PLATFORM>    m_descriptor_table;

    PushConstantData                m_push_constants;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererPipeline.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Pipeline = platform::Pipeline<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif