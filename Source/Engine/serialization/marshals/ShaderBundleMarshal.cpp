/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/serialization/fbom/FBOM.hpp>
#include <Core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <rendering/DescriptorSet.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/ShaderCompiler.hpp>

namespace Hyperion {
HYP_DECLARE_LOG_CHANNEL(ShaderCompiler);
} // namespace Hyperion

namespace Hyperion::serialization {

class CompiledShaderMarshal : public ObjectMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        return ObjectMarshal::Serialize(in, out);
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override
    {
        Handle<Shader> shader = MakeHandle<Shader>();

        out = BoxedValue(shader);

        if (FBOMResult err = ObjectMarshal::Deserialize_Internal(context, in, Shader::StaticClass(), out))
        {
            return err;
        }

        if (shader->GetRevisionNumber() != GetStaticDescriptorTableDeclaration().GetHashCode().Value())
        {
            // force recompile
            return { FBOMResult::FBOM_ERR, "Shader out of date" };
        }

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Shader, CompiledShaderMarshal);

} // namespace Hyperion::serialization
