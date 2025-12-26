/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/RenderObject.hpp>

namespace Hyperion {

class Material;
class Texture;

enum class MaterialTextureKey : uint64;

class HYP_API MaterialDescriptorSetManager
{
public:
    MaterialDescriptorSetManager();

    MaterialDescriptorSetManager(const MaterialDescriptorSetManager& other) = delete;
    MaterialDescriptorSetManager& operator=(const MaterialDescriptorSetManager& other) = delete;

    MaterialDescriptorSetManager(MaterialDescriptorSetManager&& other) noexcept = delete;
    MaterialDescriptorSetManager& operator=(MaterialDescriptorSetManager&& other) noexcept = delete;

    ~MaterialDescriptorSetManager();

    /*! \brief Retrieve the descriptor set for the material and the given frame index. The material must be bound in this frame
     *  \detail Only call from the render thread or a render task */
    const DescriptorSetRef& ForBoundMaterial(const Material* material, uint32 frameIndex);

    FixedArray<DescriptorSetRef, NumFramesInFlight> Allocate(uint32 boundIndex);
    FixedArray<DescriptorSetRef, NumFramesInFlight> Allocate(
        uint32 boundIndex,
        Span<const uint32> textureIndirectIndices,
        Span<const Handle<Texture>> textures);
    void Remove(uint32 boundIndex);

    void CreateFallbackMaterialDescriptorSet();

private:
    FixedArray<DescriptorSetRef, NumFramesInFlight> m_fallbackMaterialDescriptorSets;

    // bound index => descriptor sets
    HashMap<uint32, FixedArray<DescriptorSetRef, NumFramesInFlight>> m_materialDescriptorSets;
};

} // namespace Hyperion
