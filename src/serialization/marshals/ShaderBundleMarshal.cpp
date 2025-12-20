/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <rendering/DescriptorSet.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {
HYP_DECLARE_LOG_CHANNEL(ShaderCompiler);
} // namespace hyperion

namespace hyperion::serialization {

class CompiledShaderMarshal : public ObjectMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        return ObjectMarshal::Serialize(in, out);
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override
    {
        out = BoxedValue(CompiledShader());

        CompiledShader& compiledShader = out.Get<CompiledShader>();

        if (FBOMResult err = ObjectMarshal::Deserialize_Internal(context, in, CompiledShader::StaticClass(), out))
        {
            return err;
        }

        if (compiledShader.GetRevisionNumber() != GetStaticDescriptorTableDeclaration().GetHashCode().Value())
        {
            // force recompile
            return { FBOMResult::FBOM_ERR, "Shader out of date" };
        }

        AssertDebug(compiledShader.descriptorTableDeclaration == nullptr);

        compiledShader.descriptorTableDeclaration = new DescriptorTableDeclaration;
        compiledShader.descriptorUsageSet.BuildDescriptorTableDeclaration(*compiledShader.descriptorTableDeclaration);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShader, CompiledShaderMarshal);

} // namespace hyperion::serialization
