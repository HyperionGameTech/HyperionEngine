#include "RxDescriptorCache.hpp"

#include "RxShader.hpp"

#include <Core/FxRefUtil.hpp>
#include <Renderer/Backend/RxDsLayoutBuilder.hpp>
#include <Renderer/RxDeferred.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

RxShaderDescriptorId RxDescriptorCache::Register(uint32 set, RxShaderType shader_type,
                                                 const FxSizedArray<RxShaderOutlineEntry>& entries)
{
    Section& section = mSections[set];

    // Hash -> shader type and outline entries
    FxHash32 hash = FxHashData32(FxSlice(entries), FxHashStr32(RxShaderUtil::TypeToName(shader_type)));

    FxLogInfo("");
    FxLogInfo("=== Registering Set {} for {} ===", set, RxShaderUtil::TypeToName(shader_type));

    // Check if the DS already exists
    if (section.find(hash) != section.end()) {
        FxLogInfo("");
        return RxShaderDescriptorId { .Set = set, .Hash = hash };
    }

    // Insert the entry into the hashmap
    FxRef<RxDescriptorSet>& entry = section[hash];
    entry = FxMakeRef<RxDescriptorSet>();

    RxDescriptorPool& dp = gRenderer->pDeferredRenderer->DescriptorPool;
    RxDsLayoutBuilder layout_builder {};

    bool has_dynamic_offsets = false;

    for (const RxShaderOutlineEntry& entry : entries) {
        using SOType = RxShaderOutlineEntryType;

        if (entry.ShaderType != shader_type) {
            continue;
        }

        VkDescriptorType ds_type;

        if (entry.bUseDynamicType) {
            has_dynamic_offsets = true;
        }

        const char* binding_name = "None";

        switch (entry.Type) {
        case SOType::eSampler2D:
            ds_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding_name = "Sampler2D";
            break;
        case SOType::eStructuredBuffer:
            ds_type = entry.bUseDynamicType ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
                                            : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            binding_name = "Structured Buffer";
            break;
        case SOType::eUniformBuffer:
            ds_type = entry.bUseDynamicType ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                            : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding_name = "Uniform Buffer";

            break;
        }

        FxLogInfo("\tBinding {} => {} ({}) -> ST: {}", entry.Binding, binding_name,
                  entry.bUseDynamicType ? "Dynamic" : "Normal", RxShaderUtil::TypeToName(shader_type));

        layout_builder.AddBinding(entry.Binding, ds_type, shader_type);
    }

    FxLogInfo("");

    entry->Create(dp, layout_builder.Build(), has_dynamic_offsets);

    return RxShaderDescriptorId { .Set = set, .Hash = hash, .bContainsDynamicEntry = has_dynamic_offsets };
}

FxRef<RxDescriptorSet> RxDescriptorCache::Request(const RxShaderDescriptorId& id)
{
    Section& section = mSections[id.Set];
    return section[id.Hash];
}


RxDescriptorCache::~RxDescriptorCache()
{
    for (uint32 section_index = 0; section_index < scMaxSections; section_index++) {
        Section& section = mSections[section_index];

        for (auto& [key, ref] : section) {
            ref->DestroyLayout();
            ref.DestroyRef();
        }
    }
}
