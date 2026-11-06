/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Util/ShaderDefinitions.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Utilities/Optional.hpp>

#include <Framework/EngineGlobals.hpp>

#include <ShaderDefinitions.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

const ShaderDefinitions* GetShaderDefinitions()
{
    static const Optional<ShaderDefinitions> s_definitions = []() -> Optional<ShaderDefinitions>
    {
        const FilePath shadersPath = EngineGlobals::GetConfigDirectory() / "Shaders.hmf";

        FileByteReader reader { shadersPath };

        if (reader.Eof())
        {
            HYP_LOG(Rendering, Error, "Failed to open shader definitions file at path: {}", shadersPath);

            return {};
        }

        HMF::ParseResult parseResult = HMF::Parse(shadersPath, reader);

        if (parseResult.HasError())
        {
            HYP_LOG(Rendering, Error, "Failed to parse shader definitions file: {}", parseResult.GetError().GetMessage());

            return {};
        }

        if (!parseResult.GetValue().Is<ShaderDefinitions>())
        {
            HYP_LOG(Rendering, Error, "Parsed result of '{}' is not a ShaderDefinitions instance", shadersPath);

            return {};
        }

        return parseResult.GetValue().Get<ShaderDefinitions>();
    }();

    if (!s_definitions.HasValue())
    {
        return nullptr;
    }

    return &s_definitions.Get();
}

} // namespace Hyperion
