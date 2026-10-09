/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/WebGPU/WebGPUShared.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

#include <Core/IO/ByteReader.hpp>

namespace Hyperion {

struct WebGPUPipelineManifestBinding
{
    uint8 group;
    uint8 kind;
    uint8 visibility;
    uint8 type;
    uint8 viewDimension;
    uint8 format;
    uint16 binding;
};

static_assert(sizeof(WebGPUPipelineManifestBinding) == 8);

class WebGPUPipelineManifest final
{
public:
    static constexpr const char* FileName = "pipelinevariants.bin";

    WebGPUPipelineManifest();
    ~WebGPUPipelineManifest();

    void Record(const ShaderDesc& shaderDesc, const WGPUBindGroupLayout* layouts, uint32 numLayouts);

    void Update();

    void Save() const;

    void ReleasePipelines();

private:
    struct Entry
    {
        Name shaderName;
        ShaderPropertySet properties;
        Array<WebGPUPipelineManifestBinding, WebGPUAllocator> bindings;
        uint64 key = 0;

        bool isPrewarmed = false;
        bool isStale = false;
        ComputePipelineRef pipeline;
        uint32 lastTouchedFrame = 0;
    };

    static uint64 GetEntryKey(const Entry& entry);

    void Load();
    bool Read(ByteReader& reader);

    bool Prewarm(Entry& entry);

    Array<Entry, WebGPUAllocator> m_entries;
    bool m_isLoaded = false;
    bool m_isPrewarming = false;
    bool m_hasReported = true;
};

} // namespace Hyperion
