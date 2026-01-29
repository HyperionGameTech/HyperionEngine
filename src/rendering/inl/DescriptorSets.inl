

// Bindless descriptors
HYP_DESCRIPTOR_SRV_COND(GlobalTextureSet, Textures, DescriptorType::IMAGE, ~0u, g_renderInterface->GetRenderConfig().bindlessTextures);
// For RT we use a bindless array of vertex and index buffers
HYP_DESCRIPTOR_SRV_COND(GlobalBufferSet, Buffers, DescriptorType::STORAGE_BUFFER, ~0u, g_renderInterface->GetRenderConfig().bindlessTextures && g_renderInterface->GetRenderConfig().rayTracing);