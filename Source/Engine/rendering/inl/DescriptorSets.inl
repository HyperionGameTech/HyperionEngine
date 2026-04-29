

// Bindless descriptors
DECLARE_SRV_COND(BindlessResources0, Textures, ShaderInputType::Image, ~0u, g_renderInterface->GetRenderConfig().bindlessTextures);
// For RT we use a bindless array of vertex and index buffers
DECLARE_SRV_COND(BindlessResources1, Buffers, ShaderInputType::StorageBuffer, ~0u, g_renderInterface->GetRenderConfig().bindlessTextures && g_renderInterface->GetRenderConfig().rayTracing);