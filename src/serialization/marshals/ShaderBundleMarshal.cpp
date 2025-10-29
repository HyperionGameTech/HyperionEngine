/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/reflection/HypData.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {
HYP_DECLARE_LOG_CHANNEL(ShaderCompiler);
} // namespace hyperion

namespace hyperion::serialization {

class CompiledShaderMarshal : public HypClassInstanceMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        return HypClassInstanceMarshal::Serialize(in, out);
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        out = HypData(CompiledShader());

        CompiledShader& compiledShader = out.Get<CompiledShader>();

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, CompiledShader::Class(), out))
        {
            return err;
        }

        if (compiledShader.GetRevisionNumber() != GetStaticDescriptorTableDeclaration().GetHashCode().Value())
        {
            // force recompile
            return { FBOMResult::FBOM_ERR, "Shader out of date" };
        }

        compiledShader.descriptorTableDeclaration = compiledShader.descriptorUsageSet.BuildDescriptorTableDeclaration();

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShader, CompiledShaderMarshal);

} // namespace hyperion::serialization