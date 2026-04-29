

// Bindless descriptors
DECLARE_SRV_COND(BindlessResources0, Textures, ShaderInputType::SRV, ~0u, g_renderInterface->GetRenderConfig().bindlessTextures, ShaderResourceCategory::Image);
// For RT we use a bindless array of vertex and index buffers
DECLARE_SRV_COND(BindlessResources1, Buffers, ShaderInputType::SRV, ~0u, g_renderInterface->GetRenderConfig().bindlessTextures && g_renderInterface->GetRenderConfig().rayTracing, ShaderResourceCategory::Buffer);