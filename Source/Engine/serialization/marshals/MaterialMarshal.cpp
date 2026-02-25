/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/serialization/fbom/FBOM.hpp>
#include <Core/serialization/fbom/FBOMArray.hpp>
#include <Core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/ShaderInstance.hpp>

#include <engine/EngineGlobals.hpp>

namespace Hyperion::serialization {

class MaterialMarshal : public ObjectMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = ObjectMarshal::Serialize(in, out))
        {
            return err;
        }

#if 0
        const Material& inObject = in.Get<Material>();

        // clang-format off
        FBOMObject attributesObject = FBOMObject()
            .SetProperty("Bucket", uint32(inObject.GetRenderAttributes().bucket))
            .SetProperty("Flags", uint32(inObject.GetRenderAttributes().flags))
            .SetProperty("CullMode", uint32(inObject.GetRenderAttributes().cullFaces))
            .SetProperty("FillMode", uint32(inObject.GetRenderAttributes().fillMode))
            .SetProperty("BlendFunction", FBOMData::FromStruct(inObject.GetRenderAttributes().blendFunction))
            .SetProperty("StencilFunction", FBOMData::FromStruct(inObject.GetRenderAttributes().stencilFunction));
        // clang-format on

        out.SetProperty("Attributes", FBOMData::FromObject(std::move(attributesObject)));

        FBOMArray paramsArray;

        for (SizeType i = 0; i < inObject.GetParameters().Size(); i++)
        {
            const Pair<MaterialParameterKey, const MaterialParameter&> keyValue = inObject.GetParameters().KeyValueAt(i);

            FBOMObject paramObject;
            paramObject.SetProperty("Key", uint64(keyValue.first));
            paramObject.SetProperty("Type", uint32(keyValue.second.type));

            if (keyValue.second.IsIntType())
            {
                paramObject.SetProperty("Data", FBOMSequence(FBOMInt32(), ArraySize(keyValue.second.value.intValues)), ArraySize(keyValue.second.value.intValues) * sizeof(int32), &keyValue.second.value.intValues[0]);
            }
            else if (keyValue.second.IsFloatType())
            {
                paramObject.SetProperty("Data", FBOMSequence(FBOMFloat(), ArraySize(keyValue.second.value.floatValues)), ArraySize(keyValue.second.value.intValues) * sizeof(float), &keyValue.second.value.floatValues[0]);
            }

            paramsArray.AddElement(FBOMData::FromObject(std::move(paramObject)));
        }

        out.SetProperty("Parameters", FBOMData::FromArray(std::move(paramsArray)));

        FixedArray<uint32, MaterialTextures::MaxTextures> textureKeys = { 0 };

        // for (SizeType i = 0, textureIndex = 0; i < inObject.GetTextures().Size(); i++) {
        //     if (textureIndex >= textureKeys.Size()) {
        //         break;
        //     }

        //     const MaterialTextureKey key = inObject.GetTextures().KeyAt(i);
        //     const Handle<Texture> &value = inObject.GetTextures().ValueAt(i);

        //     if (value.IsValid()) {
        //         if (FBOMResult err = out.AddChild(*value, FBOMObjectSerializeFlags::EXTERNAL)) {
        //             return err;
        //         }

        //         textureKeys[textureIndex++] = uint32(key);
        //     }
        // }

        out.SetProperty(
            "TextureKeys",
            FBOMSequence(FBOMUInt32(), textureKeys.Size()),
            textureKeys.ByteSize(),
            &textureKeys[0]);
#endif
        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override
    {
#if 0
        MaterialAttributes attributes;
        attributes.shaderName = NAME("GeometryPass");

        MaterialParameters parameters = Material::DefaultParameters();
        MaterialTextures textures;

        FBOMObject attributesObject;

        if (FBOMResult err = in.GetProperty("Attributes").ReadObject(context, attributesObject))
        {
            HYP_BREAKPOINT;
            return err;
        }

        attributesObject.GetProperty("Bucket").ReadUInt32(&attributes.bucket);
        attributesObject.GetProperty("Flags").ReadUInt32(&attributes.flags);
        attributesObject.GetProperty("CullMode").ReadUInt32(&attributes.cullFaces);
        attributesObject.GetProperty("FillMode").ReadUInt32(&attributes.fillMode);
        attributesObject.GetProperty("BlendFunction").ReadStruct(&attributes.blendFunction);
        attributesObject.GetProperty("StencilFunction").ReadStruct(&attributes.stencilFunction);

        FBOMArray paramsArray;

        if (FBOMResult err = in.GetProperty("Parameters").ReadArray(context, paramsArray))
        {
            HYP_BREAKPOINT;
            return err;
        }

        for (SizeType i = 0; i < paramsArray.Size(); i++)
        {
            MaterialParameter param;

            if (const FBOMData& element = paramsArray.GetElement(i))
            {
                FBOMObject paramObject;

                if (FBOMResult err = element.ReadObject(context, paramObject))
                {
                    return err;
                }

                MaterialParameterKey paramKey;

                if (FBOMResult err = paramObject.GetProperty("Key").ReadUInt64(&paramKey))
                {
                    HYP_BREAKPOINT;
                    return err;
                }

                if (FBOMResult err = paramObject.GetProperty("Type").ReadUInt32(&param.type))
                {
                    HYP_BREAKPOINT;
                    return err;
                }

                if (param.IsIntType())
                {
                    if (FBOMResult err = paramObject.GetProperty("Data").ReadElements(FBOMInt32(), ArraySize(param.value.intValues), &param.value.intValues[0]))
                    {
                        HYP_BREAKPOINT;
                        return err;
                    }
                }
                else if (param.IsFloatType())
                {
                    if (FBOMResult err = paramObject.GetProperty("Data").ReadElements(FBOMFloat(), ArraySize(param.value.floatValues), &param.value.floatValues[0]))
                    {
                        HYP_BREAKPOINT;
                        return err;
                    }
                }

                parameters[paramKey] = param;
            }
        }

        FixedArray<uint32, MaterialTextures::MaxTextures> textureKeys { 0 };

        if (FBOMResult err = in.GetProperty("TextureKeys").ReadElements(FBOMUInt32(), textureKeys.Size(), &textureKeys[0]))
        {
            HYP_BREAKPOINT;
            return err;
        }

        uint32 textureIndex = 0;

        for (const FBOMObject& child : in.GetChildren())
        {
            HYP_LOG(Serialization, Verbose, "Material : Child TypeId: {}, TypeName: {}", child.GetType().GetNativeTypeId().Value(), child.GetType().name);
            if (child.GetType().IsOrExtends("Texture"))
            {
                if (textureIndex < textureKeys.Size())
                {
                    if (Optional<const Handle<Texture>&> textureOpt = child.m_deserializedObject->TryGet<Handle<Texture>>())
                    {
                        textures[MaterialTextureKey(textureKeys[textureIndex])] = *textureOpt;

                        ++textureIndex;
                    }
                }
            }
        }
#endif

        Handle<Material> material = MakeHandle<Material>();
        BoxedValue materialData = BoxedValue(material);

        if (FBOMResult err = ObjectMarshal::Deserialize_Internal(context, in, Material::StaticClass(), materialData))
        {
            return err;
        }

        Handle<Material> newMaterial = g_materialCache->GetOrCreate(
            material->GetName(),
            material->GetRenderAttributes(),
            material->GetParameters(),
            material->GetTextures());
        material.Reset();

        materialData = BoxedValue(std::move(newMaterial));
        out = std::move(materialData);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Material, MaterialMarshal);

} // namespace Hyperion::serialization
