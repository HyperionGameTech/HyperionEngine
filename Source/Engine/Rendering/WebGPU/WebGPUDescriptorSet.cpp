/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUDescriptorSet.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>
#include <Rendering/WebGPU/WebGPUGpuImageView.hpp>
#include <Rendering/WebGPU/WebGPUGpuImage.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPUSampler.hpp>
#include <Rendering/WebGPU/WebGPUAccelerationStructure.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>
#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>
#include <Rendering/WebGPU/WebGPUGraphicsPipeline.hpp>
#include <Rendering/WebGPU/WebGPUComputePipeline.hpp>
#include <Rendering/WebGPU/WebGPURayTracingPipeline.hpp>

#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderMemory.hpp>

#include <WebGPUDescriptorSet.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

static bool IsDynamicInput(const ShaderInput& shaderInput)
{
    return shaderInput.type == ShaderInputType::CBV_Dynamic
        || shaderInput.type == ShaderInputType::SRV_Dynamic
        || shaderInput.type == ShaderInputType::UAV_Dynamic;
}

static const WebGPUReflectedGroup& GetReflectedGroup(const WebGPUShaderInstance& shaderInstance, uint32 bindIndex)
{
    Assert(bindIndex < WebGPUCommandBuffer::MaxBindGroups, "Bind group index {} out of range", bindIndex);

    return shaderInstance.GetReflectedGroup(bindIndex);
}

#pragma region WebGPUDescriptorSet

WebGPUDescriptorSet::WebGPUDescriptorSet(const DescriptorSetLayout& layout)
    : DescriptorSetBase(layout),
      m_stateHashCode(0),
      m_pendingStateHashCode(0),
      m_isCreated(false)
{
    for (const ShaderInput& shaderInput : m_layout.GetElements())
    {
        switch (shaderInput.type)
        {
        case ShaderInputType::CBV:
        case ShaderInputType::CBV_Dynamic:
            PrefillElements<WebGPUGpuBuffer>(shaderInput.name, shaderInput.count);
            break;
        case ShaderInputType::SRV:
        case ShaderInputType::SRV_Dynamic:
        case ShaderInputType::UAV:
        case ShaderInputType::UAV_Dynamic:
            if (shaderInput.category == ShaderResourceCategory::Buffer)
            {
                PrefillElements<WebGPUGpuBuffer>(shaderInput.name, shaderInput.count);
            }
            else if (shaderInput.category == ShaderResourceCategory::Image)
            {
                if (shaderInput.type == ShaderInputType::SRV || shaderInput.type == ShaderInputType::SRV_Dynamic)
                {
                    PrefillElements<WebGPUGpuImageView>(shaderInput.name, shaderInput.count, RI.placeholderData->GetImageView2D1x1R8());
                }
                else
                {
                    PrefillElements<WebGPUGpuImageView>(shaderInput.name, shaderInput.count);
                }
            }
            else if (shaderInput.category == ShaderResourceCategory::AccelerationStructure)
            {
                PrefillElements<WebGPUTopLevelAS>(shaderInput.name, shaderInput.count);
            }
            else
            {
                HYP_UNREACHABLE();
            }

            break;
        case ShaderInputType::Sampler:
            PrefillElements<WebGPUSampler>(shaderInput.name, shaderInput.count);
            break;
        default:
            break;
        }
    }
}

WebGPUDescriptorSet::~WebGPUDescriptorSet()
{
    ReleaseBindGroups();
}

void WebGPUDescriptorSet::ReleaseBindGroups()
{
    for (CachedBindGroup& cachedBindGroup : m_bindGroups)
    {
        if (cachedBindGroup.bindGroup != nullptr)
        {
            wgpuBindGroupRelease(cachedBindGroup.bindGroup);
        }
    }

    m_bindGroups.Clear();
}

bool WebGPUDescriptorSet::IsCreated() const
{
    return m_isCreated;
}

RendererResult WebGPUDescriptorSet::Create()
{
    if (!m_layout.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Descriptor set layout is not valid: {}", 0, m_layout.GetName());
    }

    if (m_layout.IsTemplate())
    {
        return {};
    }

    m_isCreated = true;

    UpdateDirtyState();
    Update();

    return {};
}

uint64 WebGPUDescriptorSet::CalculateStateHashCode() const
{
    HashCode hashCode;

    for (const auto& it : m_elements)
    {
        const DescriptorSetElement& element = it.second;

        const ShaderInputWithBinding* shaderInput = m_layout.GetElement(it.first);

        if (shaderInput == nullptr || element.values.Empty())
        {
            continue;
        }

        // only the first array element is ever bound, WGSL has no resource arrays
        ObjectBase* object = element.values[0];

        hashCode.Add(shaderInput->binding);
        hashCode.Add(uintptr_t(object));
        hashCode.Add(element.bufferStride);

        if (object == nullptr)
        {
            continue;
        }

        // wrappers get recycled and resized, so the native object they hold is part of the identity
        if (shaderInput->type == ShaderInputType::Sampler)
        {
            hashCode.Add(uintptr_t(static_cast<WebGPUSampler*>(object)->GetWGPUSampler()));
        }
        else if (shaderInput->category == ShaderResourceCategory::Buffer || shaderInput->type == ShaderInputType::CBV || shaderInput->type == ShaderInputType::CBV_Dynamic)
        {
            hashCode.Add(uintptr_t(static_cast<WebGPUGpuBuffer*>(object)->GetWGPUBuffer()));
        }
        else if (shaderInput->category == ShaderResourceCategory::Image)
        {
            const WebGPUGpuImageRef& image = static_cast<WebGPUGpuImageView*>(object)->GetImage();

            hashCode.Add(uintptr_t(image.IsValid() ? image->GetWGPUTexture() : nullptr));
        }
    }

    return hashCode.Value();
}

void WebGPUDescriptorSet::UpdateDirtyState(bool* outIsDirty)
{
    m_pendingStateHashCode = CalculateStateHashCode();

    if (outIsDirty)
    {
        *outIsDirty = m_pendingStateHashCode != m_stateHashCode;
    }
}

void WebGPUDescriptorSet::Update(bool force)
{
    if (force)
    {
        m_pendingStateHashCode = CalculateStateHashCode();
    }
    else if (m_pendingStateHashCode == m_stateHashCode)
    {
        return;
    }

    m_stateHashCode = m_pendingStateHashCode;

    ReleaseBindGroups();

    for (auto& it : m_elements)
    {
        it.second.dirtyRange = {};
    }
}

const ShaderInputWithBinding* WebGPUDescriptorSet::FindElementByBinding(uint32 binding) const
{
    for (const ShaderInputWithBinding& shaderInput : m_layout.GetElements())
    {
        if (shaderInput.binding == binding)
        {
            return &shaderInput;
        }
    }

    return nullptr;
}

const WebGPUDescriptorSet::CachedBindGroup* WebGPUDescriptorSet::GetOrCreateBindGroup(const WebGPUReflectedGroup& reflectedGroup) const
{
    for (const CachedBindGroup& cachedBindGroup : m_bindGroups)
    {
        if (cachedBindGroup.reflectionHashCode == reflectedGroup.hashCode)
        {
            return cachedBindGroup.bindGroup != nullptr ? &cachedBindGroup : nullptr;
        }
    }

    const bool float32Filterable = RI.GetDeviceFeatures().float32Filterable;

    Array<WGPUBindGroupLayoutEntry, WebGPUAllocator> layoutEntries;
    Array<WGPUBindGroupEntry, WebGPUAllocator> entries;

    layoutEntries.Reserve(reflectedGroup.bindings.Size());
    entries.Reserve(reflectedGroup.bindings.Size());

    CachedBindGroup newBindGroup;
    newBindGroup.reflectionHashCode = reflectedGroup.hashCode;

    bool isComplete = true;

    for (const WebGPUReflectedBinding& reflectedBinding : reflectedGroup.bindings)
    {
        const ShaderInputWithBinding* shaderInput = FindElementByBinding(reflectedBinding.binding);

        Assert(shaderInput != nullptr, "Shader reads binding {} of descriptor set {}, which the set does not declare",
            reflectedBinding.binding, m_layout.GetName());

        const auto elementIt = m_elements.Find(shaderInput->name);

        ObjectBase* object = (elementIt != m_elements.End() && elementIt->second.values.Any())
            ? elementIt->second.values[0]
            : nullptr;

        WGPUBindGroupLayoutEntry layoutEntry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        layoutEntry.binding = reflectedBinding.binding;
        layoutEntry.visibility = reflectedBinding.visibility;

        WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
        entry.binding = reflectedBinding.binding;

        switch (reflectedBinding.kind)
        {
        case WebGPUBindingKind::UniformBuffer:
        case WebGPUBindingKind::ReadOnlyStorageBuffer:
        case WebGPUBindingKind::StorageBuffer:
        {
            Assert(object != nullptr, "Descriptor never bound for descriptor set element: {}.{}", m_layout.GetName(), shaderInput->name);

            WebGPUGpuBuffer* buffer = static_cast<WebGPUGpuBuffer*>(object);
            Assert(buffer->IsCreated(), "Buffer not created for descriptor set element: {}.{}", m_layout.GetName(), shaderInput->name);

            const DescriptorSetElement& element = elementIt->second;
            const bool isDynamic = IsDynamicInput(*shaderInput);
            const bool hasStride = element.bufferStride != ~0u && element.bufferStride != 0;

            switch (reflectedBinding.kind)
            {
            case WebGPUBindingKind::UniformBuffer:
                layoutEntry.buffer.type = WGPUBufferBindingType_Uniform;
                break;
            case WebGPUBindingKind::ReadOnlyStorageBuffer:
                layoutEntry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
                break;
            default:
                layoutEntry.buffer.type = WGPUBufferBindingType_Storage;
                break;
            }

            layoutEntry.buffer.hasDynamicOffset = isDynamic;

            entry.buffer = buffer->GetWGPUBuffer();
            entry.offset = 0;
            entry.size = WGPU_WHOLE_SIZE;

            if (isDynamic)
            {
                Assert(hasStride, "Buffer {}.{} has a dynamic offset, so it must be bound with a stride", m_layout.GetName(), shaderInput->name);

                entry.size = MathUtil::Min(uint64(element.bufferStride), buffer->GetAllocatedSize());

                Assert(newBindGroup.numDynamicElements < WebGPUBoundBindGroup::MaxDynamicOffsets);

                const uint32 alignment = reflectedBinding.kind == WebGPUBindingKind::UniformBuffer
                    ? RI.GetDeviceLimits().minUniformBufferOffsetAlignment
                    : RI.GetDeviceLimits().minStorageBufferOffsetAlignment;

                // element index * element size only lands on the alignment when the element size is a multiple of it
                const bool isRealigned = reflectedBinding.kind == WebGPUBindingKind::ReadOnlyStorageBuffer
                    && element.bufferStride % alignment != 0;

                if (isRealigned)
                {
                    entry.buffer = RI.GetRealignBuffer();
                    entry.size = ByteUtil::AlignAs(element.bufferStride, 4u);
                }

                newBindGroup.dynamicElementAlignments[newBindGroup.numDynamicElements] = alignment;
                newBindGroup.dynamicElementRealigned[newBindGroup.numDynamicElements] = isRealigned;

                newBindGroup.dynamicElementNames[newBindGroup.numDynamicElements++] = shaderInput->name;
            }
            else if (reflectedBinding.kind == WebGPUBindingKind::UniformBuffer)
            {
                const uint64 maxBindingSize = RI.GetDeviceLimits().maxUniformBufferBindingSize;

                if (hasStride)
                {
                    entry.size = MathUtil::Min(uint64(element.bufferStride), buffer->GetAllocatedSize());
                }
                else if (buffer->GetAllocatedSize() > maxBindingSize)
                {
                    entry.size = maxBindingSize;
                }
            }

            break;
        }
        case WebGPUBindingKind::Sampler:
        case WebGPUBindingKind::ComparisonSampler:
        {
            Assert(object != nullptr, "Descriptor never bound for descriptor set element: {}.{}", m_layout.GetName(), shaderInput->name);

            WebGPUSampler* sampler = static_cast<WebGPUSampler*>(object);
            Assert(sampler->IsCreated(), "Sampler not created for descriptor set element: {}.{}", m_layout.GetName(), shaderInput->name);

            const bool wantsComparison = reflectedBinding.kind == WebGPUBindingKind::ComparisonSampler;

            if (wantsComparison != sampler->IsComparison())
            {
                HYP_LOG_ONCE(RenderingBackend, Error, "Sampler bound to {}.{} is {}a comparison sampler, the shader expects the opposite",
                    m_layout.GetName(), shaderInput->name, sampler->IsComparison() ? "" : "not ");

                isComplete = false;

                break;
            }

            layoutEntry.sampler.type = wantsComparison
                ? WGPUSamplerBindingType_Comparison
                : (sampler->IsFiltering() ? WGPUSamplerBindingType_Filtering : WGPUSamplerBindingType_NonFiltering);

            entry.sampler = sampler->GetWGPUSampler();

            break;
        }
        case WebGPUBindingKind::Texture:
        case WebGPUBindingKind::DepthTexture:
        {
            WebGPUGpuImageView* imageView = static_cast<WebGPUGpuImageView*>(object);

            const bool wantsDepth = reflectedBinding.kind == WebGPUBindingKind::DepthTexture;

            WGPUTextureSampleType sampleType = wantsDepth ? WGPUTextureSampleType_Depth : reflectedBinding.sampleType;
            WGPUTextureView view = nullptr;

            if (imageView != nullptr && imageView->GetImage().IsValid() && imageView->GetImage()->IsCreated())
            {
                const TextureFormat format = imageView->GetImage()->GetTextureFormat();
                const WGPUTextureSampleType imageSampleType = GetDefaultSampleType(format, float32Filterable);

                bool isCompatible;

                if (wantsDepth)
                {
                    isCompatible = TextureUtils::IsDepthFormat(format);
                }
                else if (reflectedBinding.sampleType == WGPUTextureSampleType_Float)
                {
                    isCompatible = imageSampleType == WGPUTextureSampleType_Float || imageSampleType == WGPUTextureSampleType_UnfilterableFloat;
                    sampleType = imageSampleType;
                }
                else
                {
                    isCompatible = imageSampleType == reflectedBinding.sampleType;
                }

                if (isCompatible)
                {
                    view = imageView->GetSampledView(reflectedBinding.viewDimension);
                }
            }

            // the engine prefills every image slot with a 2D float placeholder, which does not fit most other declarations
            if (view == nullptr)
            {
                if (imageView != nullptr && imageView != RI.placeholderData->GetImageView2D1x1R8().Get() && imageView->GetImage().IsValid())
                {
                    HYP_LOG_ONCE(RenderingBackend, Warning, "Image bound to {}.{} ({}, {}) does not fit the shader declaration (view dimension {}, sample type {}), an empty texture is bound instead",
                        m_layout.GetName(), shaderInput->name,
                        EnumToString(imageView->GetImage()->GetTextureFormat()), EnumToString(imageView->GetImage()->GetType()),
                        uint32(reflectedBinding.viewDimension), uint32(reflectedBinding.sampleType));
                }

                sampleType = wantsDepth ? WGPUTextureSampleType_Depth : reflectedBinding.sampleType;
                view = RI.GetFallbackTextureView(reflectedBinding.viewDimension, sampleType);
            }

            layoutEntry.texture.sampleType = sampleType;
            layoutEntry.texture.viewDimension = reflectedBinding.viewDimension;

            entry.textureView = view;

            break;
        }
        case WebGPUBindingKind::StorageTexture:
        {
            Assert(object != nullptr, "Descriptor never bound for descriptor set element: {}.{}", m_layout.GetName(), shaderInput->name);

            WebGPUGpuImageView* imageView = static_cast<WebGPUGpuImageView*>(object);

            const WebGPUGpuImageRef& image = imageView->GetImage();

            layoutEntry.storageTexture.access = reflectedBinding.isStorageTextureRead ? WGPUStorageTextureAccess_ReadWrite : WGPUStorageTextureAccess_WriteOnly;
            layoutEntry.storageTexture.format = image.IsValid() ? ToWGPUTextureFormat(image->GetTextureFormat()) : reflectedBinding.storageFormat;
            layoutEntry.storageTexture.viewDimension = reflectedBinding.viewDimension;

            entry.textureView = imageView->GetStorageView(reflectedBinding.viewDimension);

            if (entry.textureView == nullptr)
            {
                HYP_LOG_ONCE(RenderingBackend, Error, "Image bound to {}.{} cannot be viewed as the storage texture the shader declares",
                    m_layout.GetName(), shaderInput->name);

                isComplete = false;
            }

            break;
        }
        }

        layoutEntries.PushBack(layoutEntry);
        entries.PushBack(entry);
    }

    if (isComplete)
    {
        newBindGroup.layout = RI.GetOrCreateBindGroupLayout(layoutEntries.Data(), uint32(layoutEntries.Size()));

        WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        descriptor.layout = newBindGroup.layout;
        descriptor.entryCount = entries.Size();
        descriptor.entries = entries.Data();

#ifdef HYP_RHI_DEBUG_NAMES
        if (m_debugName.IsValid())
        {
            descriptor.label = ToWGPUStringView(*m_debugName);
        }
#endif

        newBindGroup.bindGroup = wgpuDeviceCreateBindGroup(RI.GetDevice(), &descriptor);
    }

    m_bindGroups.PushBack(newBindGroup);

    return newBindGroup.bindGroup != nullptr ? &m_bindGroups.Back() : nullptr;
}

void WebGPUDescriptorSet::BindInternal(
    WebGPUCommandBuffer* commandBuffer,
    bool isCompute,
    const WebGPUReflectedGroup& reflectedGroup,
    const DescriptorSetOffsetMap* offsets,
    uint32 bindIndex) const
{
    Assert(m_isCreated);

    if (reflectedGroup.bindings.Empty())
    {
        return;
    }

    const CachedBindGroup* cachedBindGroup = GetOrCreateBindGroup(reflectedGroup);

    if (cachedBindGroup == nullptr)
    {
        commandBuffer->SetBindGroup(isCompute, bindIndex, nullptr, nullptr, nullptr, 0);

        return;
    }

    uint32 dynamicOffsets[WebGPUBoundBindGroup::MaxDynamicOffsets] = {};

    for (uint32 dynamicElementIndex = 0; dynamicElementIndex < cachedBindGroup->numDynamicElements; dynamicElementIndex++)
    {
        const Name elementName = cachedBindGroup->dynamicElementNames[dynamicElementIndex];

        uint32 offset = 0;

        for (uint32 offsetIndex = 0; offsets != nullptr && offsetIndex < offsets->count; offsetIndex++)
        {
            if (offsets->keys[offsetIndex] == StringHash(elementName))
            {
                offset = offsets->values[offsetIndex];

                break;
            }
        }

        const auto elementIt = m_elements.Find(elementName);
        AssertDebug(elementIt != m_elements.End() && elementIt->second.values.Any());

        const DescriptorSetElement& element = elementIt->second;
        const WebGPUGpuBuffer* buffer = static_cast<const WebGPUGpuBuffer*>(element.values[0]);

        if (uint64(offset) + uint64(element.bufferStride) > buffer->GetAllocatedSize())
        {
            HYP_LOG_ONCE(RenderingBackend, Error, "Dynamic buffer offset {} (+{} bytes) for '{}' is out of range for buffer size {}, binding offset 0 instead. Likely an unbound resource index.",
                offset, element.bufferStride, elementName, buffer->GetAllocatedSize());

            offset = 0;
        }

        const uint32 alignment = cachedBindGroup->dynamicElementAlignments[dynamicElementIndex];

        if (cachedBindGroup->dynamicElementRealigned[dynamicElementIndex])
        {
            offset = commandBuffer->RealignDynamicRange(buffer, offset, element.bufferStride);
        }
        else if (alignment != 0 && offset % alignment != 0)
        {
            HYP_LOG_ONCE(RenderingBackend, Error, "Dynamic buffer offset {} for '{}.{}' (stride {}) is not a multiple of the device alignment {}",
                offset, m_layout.GetName(), elementName, element.bufferStride, alignment);
        }

        dynamicOffsets[dynamicElementIndex] = offset;
    }

    commandBuffer->SetBindGroup(isCompute, bindIndex, cachedBindGroup->bindGroup, cachedBindGroup->layout, dynamicOffsets, cachedBindGroup->numDynamicElements);
}

void WebGPUDescriptorSet::Bind(WebGPUCommandBuffer* commandBuffer, const WebGPUGraphicsPipeline* pipeline, uint32 bindIndex) const
{
    BindInternal(commandBuffer, false, GetReflectedGroup(*pipeline->GetShader(), bindIndex), nullptr, bindIndex);
}

void WebGPUDescriptorSet::Bind(WebGPUCommandBuffer* commandBuffer, const WebGPUGraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    BindInternal(commandBuffer, false, GetReflectedGroup(*pipeline->GetShader(), bindIndex), &offsets, bindIndex);
}

void WebGPUDescriptorSet::Bind(WebGPUCommandBuffer* commandBuffer, const WebGPUComputePipeline* pipeline, uint32 bindIndex) const
{
    BindInternal(commandBuffer, true, GetReflectedGroup(*pipeline->GetShader(), bindIndex), nullptr, bindIndex);
}

void WebGPUDescriptorSet::Bind(WebGPUCommandBuffer* commandBuffer, const WebGPUComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    BindInternal(commandBuffer, true, GetReflectedGroup(*pipeline->GetShader(), bindIndex), &offsets, bindIndex);
}

void WebGPUDescriptorSet::Bind(WebGPUCommandBuffer* commandBuffer, const WebGPURayTracingPipeline* pipeline, uint32 bindIndex) const
{
}

void WebGPUDescriptorSet::Bind(WebGPUCommandBuffer* commandBuffer, const WebGPURayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
}

WebGPUDescriptorSetRef WebGPUDescriptorSet::Clone() const
{
    WebGPUDescriptorSetRef descriptorSet = MakeHandle<WebGPUDescriptorSet>(GetLayout());

#ifdef HYP_RHI_DEBUG_NAMES
    descriptorSet->SetDebugName(GetDebugName());
#endif

    return descriptorSet;
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUDescriptorSet::SetDebugName(Name name)
{
    DescriptorSetBase::SetDebugName(name);
}
#endif

#pragma endregion WebGPUDescriptorSet

#pragma region WebGPUDescriptorTable

WebGPUDescriptorTable::WebGPUDescriptorTable(const ShaderInputGroup* decl)
    : DescriptorTableBase(decl)
{
}

WebGPUDescriptorTable::~WebGPUDescriptorTable() = default;

#pragma endregion WebGPUDescriptorTable

} // namespace Hyperion
