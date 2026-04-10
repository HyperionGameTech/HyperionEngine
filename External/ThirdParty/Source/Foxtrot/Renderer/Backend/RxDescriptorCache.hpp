#pragma once

#include "RxDescriptors.hpp"

#include <Core/FxRef.hpp>
#include <Core/FxStackArray.hpp>
#include <Core/FxTypes.hpp>
#include <unordered_map>


struct RxShaderDescriptorId;
struct RxShaderOutlineEntry;

enum class RxShaderType : uint16;

class RxDescriptorCache
{
    static constexpr uint32 scMaxSections = 6;
    static constexpr uint32 scMaxSetsPerSection = 4;


public:
    using Section = std::unordered_map<FxHash32, FxRef<RxDescriptorSet>, FxHash32Stl>;

public:
    RxDescriptorCache() { mSections.MarkFull(); };

    /**
     * @brief Registers a new descriptor set in the cache.
     * @returns The identifier to access it by
     */
    RxShaderDescriptorId Register(uint32 set, RxShaderType shader_type,
                                  const FxSizedArray<RxShaderOutlineEntry>& entry_list);

    FxRef<RxDescriptorSet> Request(const RxShaderDescriptorId& id);

    ~RxDescriptorCache();

public:
    FxStackArray<Section, scMaxSections> mSections;
};
