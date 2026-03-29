/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/GpuImage.hpp>

#include <GpuImage.generated.inl>

namespace Hyperion {

void GpuImageBase::SetStencilState(ResourceState newState)
{
    if (!TextureUtils::HasStencilComponent(m_textureDesc.format))
    {
        m_stencilState = RS_UNDEFINED;
        return;
    }

    m_stencilState = newState;
}

void GpuImageBase::SetResourceState(ResourceState newState)
{
    m_resourceState = newState;
    m_stencilState = TextureUtils::HasStencilComponent(m_textureDesc.format) ? newState : RS_UNDEFINED;

    m_subResourceStates.Clear();
}

ResourceState GpuImageBase::GetSubResourceState(const ImageSubResource& subResource) const
{
    if (!HasSubResourceStates() || IsFullSubResource(subResource))
    {
        return GetResourceState();
    }
    
    if (subResource.numLayers == 1 && subResource.numLevels == 1)
    {
        auto it = m_subResourceStates.Find(subResource.GetSubResourceKey());

        if (it != m_subResourceStates.End())
        {
            return it->second;
        }
    }
    else
    {
        // this path assumes that all of the subresources specified by subResource are in the same state.

        ImageSubResource currSubResource {};
        currSubResource.baseArrayLayer = subResource.baseArrayLayer;
        currSubResource.baseMipLevel = subResource.baseMipLevel;
        currSubResource.numLevels = 1;
        currSubResource.numLayers = 1;
        
        auto it = m_subResourceStates.Find(currSubResource.GetSubResourceKey());

        if (it != m_subResourceStates.End())
        {
            return it->second;
        }
    }

    return m_resourceState;
}

void GpuImageBase::SetSubResourceState(const ImageSubResource& subResource, ResourceState newState)
{
    if (IsFullSubResource(subResource))
    {
        SetResourceState(newState);

        return;
    }

    if (subResource.numLayers == 1 && subResource.numLevels == 1)
    {
        if (subResource.baseMipLevel >= m_textureDesc.NumMips()
            || subResource.baseArrayLayer >= m_textureDesc.NumArrayLayers())
        {
            return;
        }

        const uint64 key = subResource.GetSubResourceKey();
    
        // no sense setting it if it is the current state and non existent
        if (newState == m_resourceState)
        {
            auto it = m_subResourceStates.Find(key);

            if (it != m_subResourceStates.End())
            {
                // so we just delete it from our map since it will be the same state as the image itself
                m_subResourceStates.Erase(it);
            }

            return;
        }

        m_subResourceStates[key] = newState;
    }
    else
    {
        const uint8 maxMip = MathUtil::Min(subResource.baseMipLevel + subResource.numLevels, m_textureDesc.NumMips());
        const uint16 maxLayer = MathUtil::Min(subResource.baseArrayLayer + subResource.numLayers, m_textureDesc.NumArrayLayers());

        for (uint8 mip = subResource.baseMipLevel; mip < maxMip; mip++)
        {
            for (uint16 layer = subResource.baseArrayLayer; layer < maxLayer; layer++)
            {
                ImageSubResource currSubResource {};
                currSubResource.baseArrayLayer = layer;
                currSubResource.baseMipLevel = mip;
                currSubResource.numLevels = 1;
                currSubResource.numLayers = 1;

                const uint64 key = currSubResource.GetSubResourceKey();

                auto it = m_subResourceStates.Find(key);
    
                // no sense setting it if it is the current state and non existent
                if (newState == m_resourceState)
                {
                    if (it != m_subResourceStates.End())
                    {
                        // so we just delete it from our map since it will be the same state as the image itself
                        m_subResourceStates.Erase(it);
                    }

                    continue;
                }

                if (it != m_subResourceStates.End())
                {
                    it->second = newState;

                    continue;
                }

                m_subResourceStates[key] = newState;
            }
        }
    }
}

bool GpuImageBase::IsFullSubResource(const ImageSubResource& subResource) const
{
    return subResource.baseMipLevel == 0
        && subResource.numLevels >= m_textureDesc.NumMips()
        && subResource.baseArrayLayer == 0
        && subResource.numLayers >= m_textureDesc.NumArrayLayers();
}

} // namespace Hyperion
