/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Name/Name.hpp>

#include <Core/Memory/Pimpl.hpp>

#include <Core/Utilities/Span.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class Shader;

enum class ShaderCacheId : uint64;
static constexpr ShaderCacheId InvalidShaderCacheId = ShaderCacheId(0);

// Serialized to disk; used to preload some shaders on init that cannot be async loaded
struct ShaderPreloadEntry
{
    char nameStr[128];
    ShaderPropertySet properties;
    VertexInputLayoutDesc inputLayout;
};

class ShaderManager
{
public:
    ShaderManager();

    ShaderInstanceRef GetOrCreate(
        Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout,
        bool waitForCompile = true);

    void ExpireShaderEntries(const Shader* shader);

    void PreloadShadersFromCacheFile(bool blockingWait = false);
    void PreloadShaders(Span<const ShaderPreloadEntry> shadersToPreload, bool blockingWait = false);

    size_t CalculateMemoryUsage() const;

private:
    /*! \brief Gets a unique ShaderCacheId for the given shader info.
     *   If the shader has already been loaded or if this method has been called before,
     *   the same ShaderCacheId will be returned.
     *   However, this value is not persistent across runs.
     */
    ShaderCacheId GetShaderCacheId(
        Name name,
        const ShaderPropertySet& properties,
        const VertexInputLayoutDesc& inputLayout,
        bool createIfNotExists = true) const;

    Pimpl<class ShaderManagerImpl> m_impl;
};

} // namespace Hyperion
